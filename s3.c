// Distributed File System - S3 Server Implementation
// This file implements the server (S3) which handles TXT files.
// S3 receives commands from S1 and processes them accordingly.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <time.h>
#include <errno.h>

#define PORT 4309
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define MAX_PATH_LEN 1024

// Function prototypes
void handle_client(int client_sock);
int upload_file(int client_sock, char *filename, char *dest_path);
int download_file(int client_sock, char *filename);
int remove_file(int client_sock, char *filename);
int download_tar(int client_sock);
int display_filenames(int client_sock, char *pathname);
int create_directory_tree(char *path);
void error(const char *msg);

// Main function initializes the server and listens for connections from S1.
// It creates a child process for each connection to handle requests concurrently.
int main() 
{
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

    printf("S3 server (TXT files) started on port %d\n", PORT);

    // Main loop to accept connections from S1
    while (1) 
    {
        // Accept connection from S1
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
        {
            error("ERROR on accept");
        }

        // Create child process to handle the connection
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

    // Close the socket
    close(sockfd);
    return 0;
}

// Function to handle requests from S1
// Parses the command received from S1 and calls the appropriate function.
void handle_client(int client_sock) 
{
    char buffer[BUFFER_SIZE];
    int n;
    
    // Read command from client (S1)
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
        if (filename == NULL) {
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
        download_tar(client_sock);
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

// Function to upload a TXT file to S3
// Receives the file from S1 and stores it in the appropriate directory.
int upload_file(int client_sock, char *filename, char *dest_path) 
{
    // First, check if the file is a TXT file
    char *ext = strrchr(filename, '.');
    if (ext == NULL || strcmp(ext, ".txt") != 0) 
    {
        write(client_sock, "ERROR: S3 only handles TXT files", 31);
        return -1;
    }
    
    // Create destination path in S3
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), dest_path + 3); // +3 to skip "~S1"
    
    // Create directory tree if needed
    if (create_directory_tree(s3_path) < 0) 
    {
        write(client_sock, "ERROR: Failed to create directory", 32);
        return -1;
    }
    
    // Construct full file path
    char *base_name = basename(filename);
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", s3_path, base_name);
    
    // Rename/move the file from temporary location (sent by S1) to final destination
    if (rename(filename, full_path) < 0) 
    {
        write(client_sock, "ERROR: Failed to move file to destination", 38);
        return -1;
    }
    
    write(client_sock, "SUCCESS: TXT file stored in S3", 30);
    return 0;
}

// Function to download a TXT file from S3
// Sends the requested file to S1 if it exists.
int download_file(int client_sock, char *filename)
{
    // Check if file exists in S3
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), filename + 3); // +3 to skip "~S1"
    
    struct stat st;
    if (stat(s3_path, &st) != 0) 
    {
        write(client_sock, "ERROR: TXT file not found in S3", 30);
        return -1;
    }
    
    // Open file
    int fd = open(s3_path, O_RDONLY);
    if (fd < 0) 
    {
        write(client_sock, "ERROR: Failed to open TXT file", 29);
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

// Function to remove a TXT file from S3
// Deletes the specified file if it exists.
int remove_file(int client_sock, char *filename)
{
    // Check if file exists in S3
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), filename + 3); // +3 to skip "~S1"
    
    if (unlink(s3_path) == 0) 
    {
        write(client_sock, "SUCCESS: TXT file deleted from S3", 32);
        return 0;
    }
    
    write(client_sock, "ERROR: TXT file not found in S3", 30);
    return -1;
}

// Function to create a tar file containing all TXT files in S3
// Finds all .txt files and creates a tar archive to send to S1.
int download_tar(int client_sock) 
{
    char s3_dir[MAX_PATH_LEN];
    snprintf(s3_dir, MAX_PATH_LEN, "%s/S3", getenv("HOME"));
    
    // Create tar file for .txt files
    char tar_cmd[MAX_PATH_LEN + 50];
    snprintf(tar_cmd, sizeof(tar_cmd), "find %s -type f -name \"*.txt\" | tar -cf /tmp/txtfiles.tar -T -", s3_dir);

    // Execute the tar command
    if (system(tar_cmd) != 0) 
    {
        write(client_sock, "ERROR: Failed to create tar file", 30);
        return -1;
    }
    
    // Send tar file to client
    struct stat st;
    if (stat("/tmp/txtfiles.tar", &st) != 0)
    {
        write(client_sock, "ERROR: Tar file not found", 25);
        return -1;
    }
    
    int fd = open("/tmp/txtfiles.tar", O_RDONLY);
    if (fd < 0) 
    {
        write(client_sock, "ERROR: Failed to open tar file", 29);
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
            write(client_sock, "ERROR: File transfer failed", 27);
            return -1;
        }
        remaining -= sent;
    }
    close(fd);
    
    // Clean up
    unlink("/tmp/txtfiles.tar");
    
    return 0;
}

// Function to display filenames of TXT files in S3
// Recursively lists all .txt files in the S3 directory.
int display_filenames(int client_sock, char *pathname)
{
    // Get the corresponding path in S3
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), 
             (strcmp(pathname, "~S1") == 0) ? "" : (pathname + 3)); // Handle root case
    
    // Check if path exists and is a directory
    struct stat st;
    if (stat(s3_path, &st) != 0 || !S_ISDIR(st.st_mode)) 
    {
        write(client_sock, "", 0); // Send empty response if directory doesn't exist
        return 0;
    }
    
    // Get TXT files from S3 recursively
    char file_list[BUFFER_SIZE] = {0};
    
    // Recursive directory traversal function
    void list_txt_files(const char *base_path, const char *relative_path)
    {
        DIR *dir = opendir(base_path);
        if (!dir) return;
        
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) 
        {
            // Skip . and .. directories
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) 
            {
                continue;
            }
            
            char full_path[MAX_PATH_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, ent->d_name);
            
            char new_relative_path[MAX_PATH_LEN];
            if (strcmp(relative_path, "") == 0) 
            {
                snprintf(new_relative_path, sizeof(new_relative_path), "%s", ent->d_name);
            } 
            else 
            {
                snprintf(new_relative_path, sizeof(new_relative_path), "%s/%s", relative_path, ent->d_name);
            }
            
            if (ent->d_type == DT_REG) 
            {
                // Check if it's a TXT file
                char *ext = strrchr(ent->d_name, '.');
                if (ext && strcmp(ext, ".txt") == 0) 
                {
                    // Convert to ~S1-style path
                    char output_path[MAX_PATH_LEN];
                    snprintf(output_path, sizeof(output_path), "~S1/%s", new_relative_path);

                    
                    strncat(file_list, output_path, BUFFER_SIZE - strlen(file_list) - 1);
                    strncat(file_list, "\n", BUFFER_SIZE - strlen(file_list) - 1);
                }
            } else if (ent->d_type == DT_DIR)
            {
                // Recursively process subdirectory
                list_txt_files(full_path, new_relative_path);
            }
        }
        closedir(dir);
    }
    
    // Start recursive traversal from the base path
    list_txt_files(s3_path, "");
    
    // Send the list to S1
    write(client_sock, file_list, strlen(file_list));
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
    }
    else 
    {
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