// Distributed File System - S1 Server Implementation
// This file implements the main server (S1) which interacts with the client and other servers (S2, S3, S4).
// S1 handles .c files locally and forwards other file types to the appropriate servers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h> // for socket()
#include <netinet/in.h> // for sockaddr_in
#include <netdb.h> // for gethostbyname()
#include <arpa/inet.h> // for inet_ntoa()
#include <sys/stat.h> // for stat()
#include <fcntl.h> // for open()
#include <dirent.h> // for opendir()
#include <libgen.h> // for basename()
#include <sys/wait.h> // for waitpid()
#include <sys/sendfile.h> // for sendfile()
#include <time.h> // for time()
#include <errno.h> // for errno

#define PORT 4307 // S1 server port
#define MAX_CLIENTS 5 // Maximum number of clients
#define BUFFER_SIZE 1024 // Buffer size for file transfer
#define MAX_PATH_LEN 1024 // Maximum path length

// Server ports for S2, S3, S4
#define S2_PORT 4308
#define S3_PORT 4309
#define S4_PORT 4310

// Function prototypes
void handle_client(int client_sock);
int upload_file(int client_sock, char *filename, char *dest_path);
int download_file(int client_sock, char *filename);
int remove_file(int client_sock, char *filename);
int download_tar(int client_sock, char *filetype);
int display_filenames(int client_sock, char *pathname);
int send_to_server(int port, char *command, char *response);
int create_directory_tree(char *path);
void error(const char *msg);

// Main function initializes the server and listens for client connections.
// It creates a child process for each client to handle requests concurrently.
int main() 
{
    // Create socket
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pid_t pid;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        error("ERROR opening socket");
    }

    // Initialize socket structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Bind the host address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        error("ERROR on binding");
    }

    // Start listening for the clients
    listen(sockfd, MAX_CLIENTS);
    clilen = sizeof(cli_addr);

    // Print server start message
    printf("S1 (MAIN SERVER) started on port %d\n", PORT);

    // Main loop to accept clients
    while (1) 
    {
        // Accept connection from client
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            error("ERROR on accept");
        }

        // Create child process to handle client
        pid = fork();
        if (pid < 0) 
        {
            error("ERROR on fork");
        }

        if (pid == 0) 
        {
            // Child process
            close(sockfd);
            handle_client(newsockfd);
            close(newsockfd);
            exit(0);
        } 

        else 
        {
            // Parent process
            close(newsockfd);
            // Clean up zombie processes
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(sockfd); // Close the socket
    return 0;
}

// Function to handle client requests
// Parses the command received from the client and calls the appropriate function.
void handle_client(int client_sock) 
{
    char buffer[BUFFER_SIZE]; // Buffer for commands
    int n;
    
    // Read command from client
    bzero(buffer, BUFFER_SIZE);
    n = read(client_sock, buffer, BUFFER_SIZE - 1);
    if (n < 0) 
    {
        error("ERROR reading from socket");
    }
    
    printf("Received command: %s\n", buffer);
    
    // Parse command
    char *cmd = strtok(buffer, " ");
    if (cmd == NULL) 
    {
        write(client_sock, "ERROR: Invalid command", 22);
        return;
    }
    
    if (strcmp(cmd, "uploadf") == 0) 
    {
        // Handle file upload
        char *filename = strtok(NULL, " ");
        char *dest_path = strtok(NULL, " ");
        if (filename == NULL || dest_path == NULL) 
        {
            write(client_sock, "ERROR: Invalid uploadf command format", 36);
            return;
        }
        upload_file(client_sock, filename, dest_path);
    } 
    else if (strcmp(cmd, "downlf") == 0) 
    {
        // Handle file download
        char *filename = strtok(NULL, " ");
        if (filename == NULL) 
        {
            write(client_sock, "ERROR: Invalid downlf command format", 34);
            return;
        }
        download_file(client_sock, filename);
    } 
    else if (strcmp(cmd, "removef") == 0) 
    {
        // Handle file removal
        char *filename = strtok(NULL, " ");
        if (filename == NULL) 
        {
            write(client_sock, "ERROR: Invalid removef command format", 35);
            return;
        }
        remove_file(client_sock, filename);
    } 
    else if (strcmp(cmd, "downltar") == 0) 
    {
        // Handle tar file download
        char *filetype = strtok(NULL, " ");
        if (filetype == NULL) 
        {
            write(client_sock, "ERROR: Invalid downltar command format", 36);
            return;
        }
        download_tar(client_sock, filetype);
    } 
    else if (strcmp(cmd, "dispfnames") == 0) 
    {
        // Handle display filenames request
        char *pathname = strtok(NULL, " ");
        if (pathname == NULL) 
        {
            write(client_sock, "ERROR: Invalid dispfnames command format", 38);
            return;
        }
        display_filenames(client_sock, pathname);
    } 
    else 
    {
        // Handle unknown command
        write(client_sock, "ERROR: Unknown command", 21);
    }
}

// Function to upload a file to S1 or forward it to the appropriate server
// Receives the file from the client and determines its type based on the extension.
int upload_file(int client_sock, char *filename, char *dest_path) 
{
    // First, receive the file from client
    char buffer[BUFFER_SIZE];
    int n;
    
    // Send acknowledgment to client to start sending file
    write(client_sock, "READY", 5);
    
    // Get file size
    off_t file_size;
    read(client_sock, &file_size, sizeof(off_t));
    
    // Determine file type
    char *ext = strrchr(filename, '.');
    if (ext == NULL) 
    {
        write(client_sock, "ERROR: File has no extension", 27);
        return -1;
    }
    
    // Create destination path in S1
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), dest_path + 3); // +3 to skip "~S1"
    
    // Create directory tree if needed
    if (create_directory_tree(s1_path) < 0) 
    {
        write(client_sock, "ERROR: Failed to create directory", 32);
        return -1;
    }
    
    // Construct full file path
    char *base_name = basename(filename);
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", s1_path, base_name);
    
    // Open file for writing
    int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) 
    {
        write(client_sock, "ERROR: Failed to create file", 27);
        return -1;
    }
    
    // Receive file data
    off_t remaining = file_size;
    while (remaining > 0) 
    {
        n = read(client_sock, buffer, BUFFER_SIZE);
        if (n <= 0) 
        {
            close(fd);
            write(client_sock, "ERROR: File transfer failed", 27);
            return -1;
        }
        write(fd, buffer, n);
        remaining -= n;
    }
    close(fd);
    
    // Determine which server should handle this file
    int target_port = 0;
    if (strcmp(ext, ".c") == 0) 
    {
        // File stays in S1
        write(client_sock, "SUCCESS: File uploaded to S1", 27);
        return 0;
    } 
    else if (strcmp(ext, ".pdf") == 0) 
    {
        target_port = S2_PORT;
    } 
    else if (strcmp(ext, ".txt") == 0) 
    {
        target_port = S3_PORT;
    } 
    else if (strcmp(ext, ".zip") == 0) 
    {
        target_port = S4_PORT;
    } 
    else 
    {
        write(client_sock, "ERROR: Unsupported file type", 27);
        return -1;
    }
    
    // Forward file to appropriate server
    char command[MAX_PATH_LEN * 2];
    snprintf(command, MAX_PATH_LEN * 2, "uploadf %s %s", full_path, dest_path);
    
    char response[BUFFER_SIZE];
    if (send_to_server(target_port, command, response) < 0) 
    {
        write(client_sock, "ERROR: Failed to forward file to target server", 44);
        return -1;
    }
    
    // Remove the file from S1 after forwarding
    unlink(full_path);
    
    write(client_sock, response, strlen(response));
    return 0;
}

// Function to download a file from S1 or request it from the appropriate server
// Checks if the file exists in S1 and sends it to the client, or forwards the request to another server.
int download_file(int client_sock, char *filename) 
{
    // Check if file exists in S1
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), filename + 3); // +3 to skip "~S1"
    
    struct stat st;
    if (stat(s1_path, &st) == 0) 
    {
        // File exists in S1 - send it directly
        int fd = open(s1_path, O_RDONLY);
        if (fd < 0) 
        {
            write(client_sock, "ERROR: Failed to open file", 25);
            return -1;
        }
        
        // Send file size
        if (write(client_sock, &st.st_size, sizeof(off_t)) != sizeof(off_t)) 
        {
            close(fd);
            write(client_sock, "ERROR: Failed to send file size", 31);
            return -1;
        }
        
        // Send file data
        off_t remaining = st.st_size;
        char buffer[BUFFER_SIZE];
        while (remaining > 0) 
        {
            ssize_t n = read(fd, buffer, (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
            if (n <= 0) 
            {
                close(fd);
                write(client_sock, "ERROR: File transfer failed", 27);
                return -1;
            }
            if (write(client_sock, buffer, n) != n) 
            {
                close(fd);
                write(client_sock, "ERROR: File transfer failed", 27);
                return -1;
            }
            remaining -= n;
        }
        close(fd);
        return 0;
    }
    
    // File not in S1 - forward to appropriate server
    char *ext = strrchr(filename, '.');
    int target_port = 0;
    if (strcmp(ext, ".pdf") == 0) 
    {
        target_port = S2_PORT;
    } 
    else if (strcmp(ext, ".txt") == 0) 
    {
        target_port = S3_PORT;
    } 
    else if (strcmp(ext, ".zip") == 0) 
    {
        target_port = S4_PORT;
    } 
    else 
    {
        write(client_sock, "ERROR: Unsupported file type", 28);
        return -1;
    }
    
    // Forward request to target server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downlf %s", filename);
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        write(client_sock, "ERROR: Socket creation failed", 29);
        return -1;
    }

    server = gethostbyname("localhost");
    if (server == NULL) 
    {
        close(sockfd);
        write(client_sock, "ERROR: Host resolution failed", 29);
        return -1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(target_port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        close(sockfd);
        write(client_sock, "ERROR: Connection to server failed", 34);
        return -1;
    }

    // Send command to target server
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        close(sockfd);
        write(client_sock, "ERROR: Command send failed", 26);
        return -1;
    }

    // Read file size from target server
    off_t filesize;
    if (read(sockfd, &filesize, sizeof(off_t)) != sizeof(off_t)) 
    {
        close(sockfd);
        write(client_sock, "ERROR: Failed to read file size", 31);
        return -1;
    }

    // Send file size to client
    if (write(client_sock, &filesize, sizeof(off_t)) != sizeof(off_t)) 
    {
        close(sockfd);
        return -1;
    }

    // Relay file content from target server to client
    off_t remaining = filesize;
    char buffer[BUFFER_SIZE];
    while (remaining > 0) 
    {
        ssize_t bytes_read = read(sockfd, buffer, sizeof(buffer));
        if (bytes_read <= 0) break;
        write(client_sock, buffer, bytes_read);
        remaining -= bytes_read;
    }

    close(sockfd);
    return 0;
}

// Function to remove a file from S1 or request its removal from another server
// Determines the file's location based on its extension and sends the removal request.
int remove_file(int client_sock, char *filename) 
{
    // Check if file exists in S1
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), filename + 3); // +3 to skip "~S1"
    
    if (unlink(s1_path) == 0) 
    {
        write(client_sock, "SUCCESS: File deleted from S1", 28);
        return 0;
    }
    
    // File not in S1 - check other servers based on extension
    char *ext = strrchr(filename, '.');
    if (ext == NULL) 
    {
        write(client_sock, "ERROR: File has no extension", 27);
        return -1;
    }
    
    int target_port = 0;
    if (strcmp(ext, ".pdf") == 0) 
    {
        target_port = S2_PORT;
    } 
    else if (strcmp(ext, ".txt") == 0) 
    {
        target_port = S3_PORT;
    } 
    else if (strcmp(ext, ".zip") == 0) 
    {
        target_port = S4_PORT;
    } 
    else 
    {
        write(client_sock, "ERROR: File not found", 21);
        return -1;
    }
    
    // Request deletion from appropriate server
    char command[MAX_PATH_LEN];
    snprintf(command, MAX_PATH_LEN, "removef %s", filename);
    
    char response[BUFFER_SIZE];
    if (send_to_server(target_port, command, response) < 0) 
    {
        write(client_sock, "ERROR: Failed to delete file from target server", 45);
        return -1;
    }
    
    write(client_sock, response, strlen(response));
    return 0;
}

// Function to download a tar file containing files of a specific type
// Handles .c files locally and forwards requests for other file types to the appropriate server.
int download_tar(int client_sock, char *filetype) 
{
    if (strcmp(filetype, ".c") == 0) 
    {
        // Handle .c files in S1
        char s1_dir[MAX_PATH_LEN];
        snprintf(s1_dir, MAX_PATH_LEN, "%s/S1", getenv("HOME"));

        // Find all .c files recursively and tar them
        char find_cmd[MAX_PATH_LEN * 2];
        snprintf(find_cmd, sizeof(find_cmd), "find %s -type f -name \"*.c\" | tar -cf /tmp/cfiles.tar -T -", s1_dir);

        if (system(find_cmd) != 0) 
        {
            write(client_sock, "ERROR: Failed to create tar file", 32);
            return -1;
        }

        // Send tar file to client
        struct stat st;
        if (stat("/tmp/cfiles.tar", &st) != 0) 
        {
            write(client_sock, "ERROR: Tar file not found", 25);
            return -1;
        }

        int fd = open("/tmp/cfiles.tar", O_RDONLY);
        if (fd < 0) 
        {
            write(client_sock, "ERROR: Failed to open tar file", 30);
            return -1;
        }

        // Send file size
        write(client_sock, &st.st_size, sizeof(off_t));

        // Send file data
        off_t remaining = st.st_size;
        while (remaining > 0) 
        {
            ssize_t sent = sendfile(client_sock, fd, NULL, remaining);
            if (sent <= 0) 
            {
                close(fd);
                write(client_sock, "ERROR: File transfer failed", 28);
                return -1;
            }
            remaining -= sent;
        }
        close(fd);

        // Clean up
        unlink("/tmp/cfiles.tar");
        return 0;

    } 
    else if (strcmp(filetype, ".pdf") == 0 || strcmp(filetype, ".txt") == 0) 
    {
        // Handle .pdf and .txt files from other servers
        int target_port = (strcmp(filetype, ".pdf") == 0) ? S2_PORT : S3_PORT;

        char command[BUFFER_SIZE];
        snprintf(command, BUFFER_SIZE, "downltar %s", filetype);

        int sockfd;
        struct sockaddr_in serv_addr;
        struct hostent *server;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) 
        {
            write(client_sock, "ERROR: Socket creation failed", 29);
            return -1;
        }

        server = gethostbyname("localhost");
        if (server == NULL) 
        {
            close(sockfd);
            write(client_sock, "ERROR: Host resolution failed", 29);
            return -1;
        }

        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(target_port);

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        {
            close(sockfd);
            write(client_sock, "ERROR: Connection to server failed", 34);
            return -1;
        }

        // Send command to target server
        if (write(sockfd, command, strlen(command)) < 0) 
        {
            close(sockfd);
            write(client_sock, "ERROR: Command send failed", 26);
            return -1;
        }

        // Read tar file size
        off_t filesize;
        if (read(sockfd, &filesize, sizeof(off_t)) != sizeof(off_t)) 
        {
            close(sockfd);
            write(client_sock, "ERROR: Failed to read file size", 31);
            return -1;
        }

        // Send file size to client
        write(client_sock, &filesize, sizeof(off_t));

        // Relay tar file content from target server to client
        off_t remaining = filesize;
        char buffer[1024];
        while (remaining > 0) 
        {
            ssize_t bytes_read = read(sockfd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break;
            write(client_sock, buffer, bytes_read);
            remaining -= bytes_read;
        }

        close(sockfd);
        return 0;
    } 
    else 
    {
        write(client_sock, "ERROR: Unsupported file type for tar", 36);
        return -1;
    }
}

// Function to display filenames from S1 and other servers
// Recursively lists files in S1 and sends requests to other servers for their file lists.
int display_filenames(int client_sock, char *pathname) 
{
    // Get the corresponding path in S1
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), 
             (strcmp(pathname, "~S1") == 0) ? "" : (pathname + 3)); // Handle root case

    // Check if path exists and is a directory
    struct stat st;
    if (stat(s1_path, &st) != 0 || !S_ISDIR(st.st_mode)) 
    {
        write(client_sock, "ERROR: Invalid directory path", 29);
        return -1;
    }

    // Get files from S1 (.c files) recursively
    char file_list[BUFFER_SIZE] = {0};
    DIR *dir;
    struct dirent *ent;

    // Function to recursively list files
    void list_files_recursive(const char *base_path, const char *relative_path) 
    {
        char path[MAX_PATH_LEN];
        struct dirent *dp;
        DIR *dir = opendir(base_path);

        if (!dir) return;

        while ((dp = readdir(dir)) != NULL) 
        {
            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                continue;

            snprintf(path, MAX_PATH_LEN, "%s/%s", base_path, dp->d_name);

            char new_relative_path[MAX_PATH_LEN];
            if (strcmp(relative_path, "") == 0) 
            {
                snprintf(new_relative_path, sizeof(new_relative_path), "%s", dp->d_name);
            } 
            else 
            {
                snprintf(new_relative_path, sizeof(new_relative_path), "%s/%s", relative_path, dp->d_name);
            }

            if (dp->d_type == DT_REG) {
                char *ext = strrchr(dp->d_name, '.');
                if (ext && strcmp(ext, ".c") == 0) 
                {
                    // Build ~S1-style path
                    char output_path[MAX_PATH_LEN];
                    snprintf(output_path, sizeof(output_path), "~S1/%s", new_relative_path);
                    strncat(file_list, output_path, BUFFER_SIZE - strlen(file_list) - 1);
                    strncat(file_list, "\n", BUFFER_SIZE - strlen(file_list) - 1);
                }
            } 
            else if (dp->d_type == DT_DIR) 
            {
                list_files_recursive(path, new_relative_path);
            }
        }
        closedir(dir);
    }

    // Start recursive traversal from the base path
    list_files_recursive(s1_path, "");

    // Get files from other servers
    char command[MAX_PATH_LEN];
    snprintf(command, MAX_PATH_LEN, "dispfnames %s", pathname);
    char response[BUFFER_SIZE];

    // Get PDF files from S2
    if (send_to_server(S2_PORT, command, response) == 0) 
    {
        strncat(file_list, response, BUFFER_SIZE - strlen(file_list) - 1);
    }

    // Get TXT files from S3
    if (send_to_server(S3_PORT, command, response) == 0) 
    {
        strncat(file_list, response, BUFFER_SIZE - strlen(file_list) - 1);
    }

    // Get ZIP files from S4
    if (send_to_server(S4_PORT, command, response) == 0) 
    {
        strncat(file_list, response, BUFFER_SIZE - strlen(file_list) - 1);
    }

    // Send the combined list to client
    write(client_sock, file_list, strlen(file_list));
    return 0;
}

// Function to send a command to another server and receive its response
// Establishes a connection to the target server, sends the command, and reads the response.
int send_to_server(int port, char *command, char *response) 
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        return -1;
    }
    
    server = gethostbyname("localhost");
    if (server == NULL) 
    {
        close(sockfd);
        return -1;
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        close(sockfd);
        return -1;
    }
    
    // Send command
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        close(sockfd);
        return -1;
    }
    
    // Read response
    bzero(response, BUFFER_SIZE);
    if (read(sockfd, response, BUFFER_SIZE - 1) < 0) 
    {
        close(sockfd);
        return -1;
    }
    
    close(sockfd);
    return 0;
}

// Function to create a directory tree for a given path
// Ensures that all intermediate directories in the path exist.
int create_directory_tree(char *path) 
{
    char *p;
    char tmp[MAX_PATH_LEN];
    
    snprintf(tmp, MAX_PATH_LEN, "%s", path);
    
    // Skip leading slash if present
    if (tmp[0] == '/') 
    {
        p = tmp + 1;
    } else {
        p = tmp;
    }
    
    // Create each directory in the path
    while ((p = strchr(p, '/'))) 
    {
        *p = '\0';
        if (mkdir(tmp, 0755) && errno != EEXIST) 
        {
            return -1;
        }
        *p = '/';
        p++;
    }
    
    // Create the final directory
    if (mkdir(tmp, 0755) && errno != EEXIST) 
    {
        return -1;
    }
    
    return 0;
}

// Function to handle errors
// Prints the error message and exits the program.
void error(const char *msg) 
{
    perror(msg);
    exit(1);
}