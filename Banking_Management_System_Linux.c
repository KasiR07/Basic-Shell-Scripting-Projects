#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>

/*
 * PROCESS MONITORING SYSTEM
 * A tool for analyzing and manipulating process relationships
 */

// Forward declarations for data structures
struct ProcessNode;
struct ProcessCollection;
struct CommandHandler;

// =============== DATA STRUCTURES ===============

// Process metadata container
typedef struct {
    int id;              // Process ID
    int parent_id;       // Parent Process ID
    int defunct_flag;    // Zombie state indicator
} ProcessMetadata;

// Process node for building relationships
typedef struct ProcessNode {
    ProcessMetadata data;
    struct ProcessNode* parent;
    struct ProcessNode** children;
    int child_count;
    int child_capacity;
} ProcessNode;

// Process collection manager
typedef struct ProcessCollection {
    ProcessNode** nodes;
    int size;
    int capacity;
    ProcessNode* root;
} ProcessCollection;

// Command operation result container
typedef struct {
    int* result_ids;
    int count;
    const char* empty_message;
} CommandResult;

// Command handler function signature
typedef CommandResult (*CommandFunction)(ProcessCollection*, int);

// Command mapping structure
typedef struct {
    const char* flag;
    CommandFunction handler;
    const char* description;
} CommandMapping;

// =============== PROCESS NODE FUNCTIONS ===============

// Create a new process node
ProcessNode* create_node(int pid, int ppid, int zombie_status) {
    ProcessNode* node = malloc(sizeof(ProcessNode));
    if (!node) {
        perror("Memory allocation failed for process node");
        exit(EXIT_FAILURE);
    }
    
    node->data.id = pid;
    node->data.parent_id = ppid;
    node->data.defunct_flag = zombie_status;
    node->parent = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    
    return node;
}

// Add a child node to a parent node
void add_child(ProcessNode* parent, ProcessNode* child) {
    if (!parent->children) {
        parent->child_capacity = 4;  // Initial capacity
        parent->children = malloc(sizeof(ProcessNode*) * parent->child_capacity);
        if (!parent->children) {
            perror("Memory allocation failed for children array");
            exit(EXIT_FAILURE);
        }
    }
    
    // Resize if needed
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = realloc(parent->children, 
                                  sizeof(ProcessNode*) * parent->child_capacity);
        if (!parent->children) {
            perror("Memory reallocation failed for children array");
            exit(EXIT_FAILURE);
        }
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

// Free a process node and its children
void free_node(ProcessNode* node) {
    if (!node) return;
    
    for (int i = 0; i < node->child_count; i++) {
        free_node(node->children[i]);
    }
    
    if (node->children) {
        free(node->children);
    }
    
    free(node);
}

// =============== PROCESS COLLECTION FUNCTIONS ===============

// Initialize a process collection
ProcessCollection* init_collection(int initial_capacity) {
    ProcessCollection* collection = malloc(sizeof(ProcessCollection));
    if (!collection) {
        perror("Memory allocation failed for process collection");
        exit(EXIT_FAILURE);
    }
    
    collection->nodes = malloc(sizeof(ProcessNode*) * initial_capacity);
    if (!collection->nodes) {
        perror("Memory allocation failed for nodes array");
        free(collection);
        exit(EXIT_FAILURE);
    }
    
    collection->size = 0;
    collection->capacity = initial_capacity;
    collection->root = NULL;
    
    return collection;
}

// Add a node to the collection
void add_to_collection(ProcessCollection* collection, ProcessNode* node) {
    if (collection->size >= collection->capacity) {
        collection->capacity *= 2;
        collection->nodes = realloc(collection->nodes, 
                                   sizeof(ProcessNode*) * collection->capacity);
        if (!collection->nodes) {
            perror("Memory reallocation failed for nodes array");
            exit(EXIT_FAILURE);
        }
    }
    
    collection->nodes[collection->size++] = node;
}

// Find a node by process ID
ProcessNode* find_node_by_pid(ProcessCollection* collection, int pid) {
    for (int i = 0; i < collection->size; i++) {
        if (collection->nodes[i]->data.id == pid) {
            return collection->nodes[i];
        }
    }
    return NULL;
}

// Free the entire collection
void free_collection(ProcessCollection* collection) {
    if (!collection) return;
    
    // Root node will recursively free all children
    if (collection->root) {
        free_node(collection->root);
    } else {
        // If no root is set, free nodes individually
        for (int i = 0; i < collection->size; i++) {
            // Check if this is a root node (not a child of another node)
            if (!collection->nodes[i]->parent) {
                free_node(collection->nodes[i]);
            }
        }
    }
    
    free(collection->nodes);
    free(collection);
}

// =============== PROCESS DATA FUNCTIONS ===============

// Read process data from /proc filesystem
int fetch_process_data(int pid, ProcessMetadata* data) {
    char filepath[256];
    char buffer[256];
    FILE* file;
    
    snprintf(filepath, sizeof(filepath), "/proc/%d/status", pid);
    file = fopen(filepath, "r");
    if (!file) {
        return 0;
    }
    
    data->id = pid;
    data->parent_id = -1;
    data->defunct_flag = 0;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "PPid:", 5) == 0) {
            sscanf(buffer, "PPid: %d", &data->parent_id);
        }
        else if (strncmp(buffer, "State:", 6) == 0) {
            data->defunct_flag = (strchr(buffer, 'Z') != NULL);
        }
    }
    
    fclose(file);
    return 1;
}

// =============== TREE BUILDING FUNCTIONS ===============

// Recursive function to discover child processes
void discover_children(ProcessCollection* collection, int parent_pid) {
    DIR* dir;
    struct dirent* entry;
    ProcessMetadata proc_data;
    
    dir = opendir("/proc");
    if (!dir) {
        perror("Failed to open /proc directory");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid > 0) {
                if (fetch_process_data(pid, &proc_data)) {
                    if (proc_data.parent_id == parent_pid) {
                        // Skip if already in collection
                        if (find_node_by_pid(collection, pid)) continue;
                        
                        // Create and add the node
                        ProcessNode* node = create_node(pid, proc_data.parent_id, proc_data.defunct_flag);
                        add_to_collection(collection, node);
                        
                        // Link with parent
                        ProcessNode* parent = find_node_by_pid(collection, parent_pid);
                        if (parent) {
                            add_child(parent, node);
                        }
                        
                        // Recursively find its children
                        discover_children(collection, pid);
                    }
                }
            }
        }
    }
    
    closedir(dir);
}

// Build the process hierarchy from a root process
void build_process_hierarchy(ProcessCollection* collection, int root_pid) {
    ProcessMetadata root_data;
    
    // Add root process
    if (fetch_process_data(root_pid, &root_data)) {
        ProcessNode* root = create_node(root_pid, root_data.parent_id, root_data.defunct_flag);
        add_to_collection(collection, root);
        collection->root = root;
        
        // Discover all child processes
        discover_children(collection, root_pid);
    }
    
    // Link all nodes to establish the tree structure
    for (int i = 0; i < collection->size; i++) {
        ProcessNode* node = collection->nodes[i];
        if (node->data.id != root_pid) {
            ProcessNode* parent = find_node_by_pid(collection, node->data.parent_id);
            if (parent && node->parent == NULL) {
                add_child(parent, node);
            }
        }
    }
}

// =============== QUERY FUNCTIONS ===============

// Initialize a command result
CommandResult init_result(const char* empty_message) {
    CommandResult result;
    result.result_ids = malloc(sizeof(int) * 1000);  // Allocate space for up to 1000 results
    result.count = 0;
    result.empty_message = empty_message;
    return result;
}

// Count zombie processes
int count_zombies(ProcessCollection* collection) {
    int count = 0;
    for (int i = 0; i < collection->size; i++) {
        if (collection->nodes[i]->data.defunct_flag) {
            count++;
        }
    }
    return count;
}

// Get direct descendants (children)
CommandResult get_direct_descendants(ProcessCollection* collection, int pid) {
    CommandResult result = init_result("No direct descendants");
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node) {
        for (int i = 0; i < node->child_count; i++) {
            result.result_ids[result.count++] = node->children[i]->data.id;
        }
    }
    
    return result;
}

// Get grandchildren
CommandResult get_grandchildren(ProcessCollection* collection, int pid) {
    CommandResult result = init_result("No grandchildren");
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node) {
        for (int i = 0; i < node->child_count; i++) {
            ProcessNode* child = node->children[i];
            for (int j = 0; j < child->child_count; j++) {
                result.result_ids[result.count++] = child->children[j]->data.id;
            }
        }
    }
    
    return result;
}

// Helper function for recursive retrieval of indirect descendants
void collect_indirect_descendants(ProcessNode* node, int* result, int* count) {
    for (int i = 0; i < node->child_count; i++) {
        ProcessNode* child = node->children[i];
        
        // Add grandchildren and deeper descendants
        for (int j = 0; j < child->child_count; j++) {
            result[(*count)++] = child->children[j]->data.id;
            collect_indirect_descendants(child->children[j], result, count);
        }
    }
}

// Get non-direct descendants (grandchildren and beyond)
CommandResult get_indirect_descendants(ProcessCollection* collection, int pid) {
    CommandResult result = init_result("No non-direct descendants");
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node) {
        collect_indirect_descendants(node, result.result_ids, &result.count);
    }
    
    return result;
}

// Get sibling processes
CommandResult get_siblings(ProcessCollection* collection, int pid) {
    CommandResult result = init_result("No sibling/s");
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node && node->parent) {
        for (int i = 0; i < node->parent->child_count; i++) {
            ProcessNode* sibling = node->parent->children[i];
            if (sibling->data.id != pid) {
                result.result_ids[result.count++] = sibling->data.id;
            }
        }
    }
    
    return result;
}

// Get defunct siblings
CommandResult get_defunct_siblings(ProcessCollection* collection, int pid) {
    CommandResult result = init_result("No defunct sibling/s");
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node && node->parent) {
        for (int i = 0; i < node->parent->child_count; i++) {
            ProcessNode* sibling = node->parent->children[i];
            if (sibling->data.id != pid && sibling->data.defunct_flag) {
                result.result_ids[result.count++] = sibling->data.id;
            }
        }
    }
    
    return result;
}

// Helper function for recursive retrieval of defunct descendants
void collect_defunct_descendants(ProcessNode* node, int* result, int* count) {
    for (int i = 0; i < node->child_count; i++) {
        ProcessNode* child = node->children[i];
        
        if (child->data.defunct_flag) {
            result[(*count)++] = child->data.id;
        }
        
        collect_defunct_descendants(child, result, count);
    }
}

// Get defunct descendants
CommandResult get_defunct_descendants(ProcessCollection* collection, int pid) {
    CommandResult result = init_result("No descendant zombie process/es");
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node) {
        collect_defunct_descendants(node, result.result_ids, &result.count);
    }
    
    return result;
}

// =============== ACTION FUNCTIONS ===============

// Signal all descendants of a process
CommandResult signal_all_descendants(ProcessCollection* collection, int pid, int signal_type) {
    CommandResult result = init_result(NULL);
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node) {
        for (int i = 0; i < collection->size; i++) {
            ProcessNode* current = collection->nodes[i];
            ProcessNode* parent = current->parent;
            
            // Traverse up the tree to check if pid is an ancestor
            while (parent) {
                if (parent->data.id == pid) {
                    kill(current->data.id, signal_type);
                    result.result_ids[result.count++] = current->data.id;
                    break;
                }
                parent = parent->parent;
            }
        }
    }
    
    return result;
}

// Kill parents of zombie processes
CommandResult kill_zombie_parents(ProcessCollection* collection, int pid) {
    CommandResult result = init_result(NULL);
    
    // Get defunct descendants
    CommandResult zombies = get_defunct_descendants(collection, pid);
    
    // Kill their parents
    for (int i = 0; i < zombies.count; i++) {
        ProcessNode* zombie = find_node_by_pid(collection, zombies.result_ids[i]);
        if (zombie && zombie->data.parent_id > 1) {  // Don't kill init
            kill(zombie->data.parent_id, SIGKILL);
            result.result_ids[result.count++] = zombie->data.parent_id;
        }
    }
    
    free(zombies.result_ids);
    return result;
}

// =============== COMMAND HANDLER FUNCTIONS ===============

// Handler for -dc (count defunct)
CommandResult handle_count_defunct(ProcessCollection* collection, int pid) {
    CommandResult result = init_result(NULL);
    printf("%d\n", count_zombies(collection));
    return result;
}

// Handler for -ds (list non-direct descendants)
CommandResult handle_non_direct_descendants(ProcessCollection* collection, int pid) {
    return get_indirect_descendants(collection, pid);
}

// Handler for -id (list immediate descendants)
CommandResult handle_immediate_descendants(ProcessCollection* collection, int pid) {
    return get_direct_descendants(collection, pid);
}

// Handler for -lg (list siblings)
CommandResult handle_siblings(ProcessCollection* collection, int pid) {
    return get_siblings(collection, pid);
}

// Handler for -lz (list defunct siblings)
CommandResult handle_defunct_siblings(ProcessCollection* collection, int pid) {
    return get_defunct_siblings(collection, pid);
}

// Handler for -df (list defunct descendants)
CommandResult handle_defunct_descendants(ProcessCollection* collection, int pid) {
    return get_defunct_descendants(collection, pid);
}

// Handler for -gc (list grandchildren)
CommandResult handle_grandchildren(ProcessCollection* collection, int pid) {
    return get_grandchildren(collection, pid);
}

// Handler for -do (print process status)
CommandResult handle_process_status(ProcessCollection* collection, int pid) {
    CommandResult result = init_result(NULL);
    ProcessNode* node = find_node_by_pid(collection, pid);
    
    if (node) {
        printf("%s\n", node->data.defunct_flag ? "Defunct" : "Not defunct");
    }
    
    return result;
}

// Handler for --pz (kill parents of zombie processes)
CommandResult handle_kill_zombie_parents(ProcessCollection* collection, int pid) {
    return kill_zombie_parents(collection, pid);
}

// Handler for -sk (kill all descendants)
CommandResult handle_kill_descendants(ProcessCollection* collection, int pid) {
    return signal_all_descendants(collection, pid, SIGKILL);
}

// Handler for -st (stop all descendants)
CommandResult handle_stop_descendants(ProcessCollection* collection, int pid) {
    return signal_all_descendants(collection, pid, SIGSTOP);
}

// Handler for -dt (continue all stopped descendants)
CommandResult handle_continue_descendants(ProcessCollection* collection, int pid) {
    return signal_all_descendants(collection, pid, SIGCONT);
}

// Handler for -rp (kill root process)
CommandResult handle_kill_root(ProcessCollection* collection, int pid) {
    CommandResult result = init_result(NULL);
    if (collection->root) {
        kill(collection->root->data.id, SIGKILL);
    }
    return result;
}

// =============== COMMAND REGISTRY ===============

// Command registry
CommandMapping command_registry[] = {
    {"-dc", handle_count_defunct, "Count defunct descendants"},
    {"-ds", handle_non_direct_descendants, "List non-direct descendants"},
    {"-id", handle_immediate_descendants, "List immediate descendants"},
    {"-lg", handle_siblings, "List sibling processes"},
    {"-lz", handle_defunct_siblings, "List defunct sibling processes"},
    {"-df", handle_defunct_descendants, "List defunct descendants"},
    {"-gc", handle_grandchildren, "List grandchildren"},
    {"-do", handle_process_status, "Print process status"},
    {"--pz", handle_kill_zombie_parents, "Kill parents of zombie processes"},
    {"-sk", handle_kill_descendants, "Kill all descendants"},
    {"-st", handle_stop_descendants, "Stop all descendants"},
    {"-dt", handle_continue_descendants, "Continue all stopped descendants"},
    {"-rp", handle_kill_root, "Kill root process"},
    {NULL, NULL, NULL}  // Sentinel
};

// Find command handler
CommandFunction find_command_handler(const char* option) {
    for (int i = 0; command_registry[i].flag != NULL; i++) {
        if (strcmp(command_registry[i].flag, option) == 0) {
            return command_registry[i].handler;
        }
    }
    return NULL;
}

// =============== OUTPUT FUNCTIONS ===============

// Print command results
void display_results(CommandResult* result) {
    if (result->count == 0 && result->empty_message) {
        printf("%s\n", result->empty_message);
    } else {
        for (int i = 0; i < result->count; i++) {
            printf("%d\n", result->result_ids[i]);
        }
    }
    
    free(result->result_ids);
}

// =============== MAIN FUNCTION ===============

int main(int argc, char* argv[]) {
    // Validate arguments
    if (argc < 3) {
        printf("Usage: %s [root_process] [target_process] [option]\n", argv[0]);
        return 1;
    }
    
    int root_pid = atoi(argv[1]);
    int target_pid = atoi(argv[2]);
    
    // Initialize process collection and build hierarchy
    ProcessCollection* processes = init_collection(1000);
    build_process_hierarchy(processes, root_pid);
    
    // Basic case - no option
    if (argc == 3) {
        ProcessNode* node = find_node_by_pid(processes, target_pid);
        if (node) {
            printf("%d %d\n", node->data.id, node->data.parent_id);
        } else {
            printf("Does not belong to the process tree\n");
        }
    }
    // Option case
    else {
        const char* option = argv[3];
        CommandFunction handler = find_command_handler(option);
        
        // Special case for -rp which doesn't require process verification
        if (strcmp(option, "-rp") == 0) {
            CommandResult result = handle_kill_root(processes, root_pid);
            display_results(&result);
        }
        // For other commands, verify the process exists in the tree
        else if (handler) {
            if (find_node_by_pid(processes, target_pid)) {
                CommandResult result = handler(processes, target_pid);
                display_results(&result);
            } else {
                printf("The process %d does not belong to the tree rooted at %d\n", 
                       target_pid, root_pid);
            }
        }
        else {
            printf("Invalid option: %s\n", option);
        }
    }
    
    // Clean up
    free_collection(processes);
    
    return 0;
}