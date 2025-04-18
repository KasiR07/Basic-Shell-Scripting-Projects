// Distributed File System - Client Implementation (w25clients.c)

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <unistd.h> // for close()
#include <sys/types.h>
#include <sys/socket.h> // for socket()
#include <netinet/in.h>
#include <netdb.h> // for gethostbyname()
#include <arpa/inet.h> // for inet_ntoa()
#include <sys/stat.h>
#include <fcntl.h> // for open()
#include <libgen.h> // for basename()
#include <errno.h> // for errno

#define PORT 4307 // S1 server port
#define BUFFER_SIZE 1024 // Buffer size for file transfer
#define MAX_PATH_LEN 1024 // Maximum path length

// Function prototypes
void error(const char *msg); // Error handling function
int connect_to_server(); // Function to connect to the server
void handle_uploadf(int sockfd, char *filename, char *dest_path); // Function to handle file upload
void handle_downlf(int sockfd, char *filename);
void handle_removef(int sockfd, char *filename);
void handle_downltar(int sockfd, char *filetype);
void handle_dispfnames(int sockfd, char *pathname);
int send_file(int sockfd, char *filename);
int receive_file(int sockfd, char *filename);

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE]; // Buffer for user input
    
    printf("Distributed File System Client\n");
    printf("Available commands:\n");
    printf("  uploadf <filename> <destination_path> (example: uploadf test1.txt ~S1/folder1/)\n");
    printf("  downlf <filename> (example: downlf ~S1/folder1/test1.txt)\n");
    printf("  removef <filename> (example: removef ~S1/folder1/test1.txt)\n");
    printf("  downltar <filetype> (example: downltar .txt)\n");
    printf("  dispfnames <pathname> (example: dispfnames ~S1/)\n");
    printf("  exit\n\n");
    
    while (1) 
    {
        printf("w25clients$ ");
        bzero(buffer, BUFFER_SIZE); 
        fgets(buffer, BUFFER_SIZE - 1, stdin);
        
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Ignore empty commands
        if (strlen(buffer) == 0) 
        {
            continue;
        }
        
        // Check for exit command
        if (strcmp(buffer, "exit") == 0) 
        {
            break;
        }
        
        // Connect to server
        sockfd = connect_to_server();
        if (sockfd < 0) 
        {
            printf("Failed to connect to server\n");
            continue;
        }
        
        // Parse command
        char *cmd = strtok(buffer, " ");
        
        // task1 uplodf
		if (strcmp(cmd, "uploadf") == 0) 
        {
            char *filename = strtok(NULL, " ");
            char *dest_path = strtok(NULL, " ");
            if (filename == NULL || dest_path == NULL) 
            {
                printf("Invalid command format. Usage: uploadf <filename> <destination_path>\n"); // Example: uploadf test1.txt ~S1/folder1/
                close(sockfd);
                continue;
            }
            handle_uploadf(sockfd, filename, dest_path); // Upload file
        } 

        // task 2 downlf
		else if (strcmp(cmd, "downlf") == 0) 
        {
            char *filename = strtok(NULL, " ");
            if (filename == NULL) 
            {
                printf("Invalid command format. Usage: downlf <filename>\n");
                close(sockfd);
                continue;
            }
            handle_downlf(sockfd, filename);
        }
		// task 3 removef
        else if (strcmp(cmd, "removef") == 0) 
        {
            char *filename = strtok(NULL, " ");
            if (filename == NULL) 
            {
                printf("Invalid command format. Usage: removef <filename>\n");
                close(sockfd);
                continue;
            }
            handle_removef(sockfd, filename);
        } 
		// task 4 downltar
        else if (strcmp(cmd, "downltar") == 0) 
        {
            char *filetype = strtok(NULL, " ");
            if (filetype == NULL) 
            {
                printf("Invalid command format. Usage: downltar <filetype>\n");
                close(sockfd);
                continue;
            }
            handle_downltar(sockfd, filetype);
        }

		// task 5 dispfnames
        else if (strcmp(cmd, "dispfnames") == 0)
        {
            char *pathname = strtok(NULL, " ");
            if (pathname == NULL) 
            {
                printf("Invalid command format. Usage: dispfnames <pathname>\n");
                close(sockfd);
                continue;
            }
            handle_dispfnames(sockfd, pathname);
        } 
        else 
        {
            printf("Unknown command: %s\n", cmd);
        }
        
        close(sockfd); // Close the socket after each command
    }
    
    return 0;
}

int connect_to_server() // Function to connect to the server
{
    int sockfd; // Socket file descriptor
    struct sockaddr_in serv_addr; // Server address structure
    struct hostent *server; // Host information structure
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create socket
    if (sockfd < 0) 
    {
        error("ERROR opening socket"); // Socket creation failed
        return -1;
    }
    
    // Get server address
    server = gethostbyname("localhost");
    if (server == NULL) 
    {
        fprintf(stderr, "ERROR, no such host\n"); // Host resolution failed
        return -1;
    }
    
    // Initialize server address structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // Address family
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length); // Copy host address
    serv_addr.sin_port = htons(PORT); // Port number
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        error("ERROR connecting");
        return -1;
    }
    
    return sockfd; // Return the socket file descriptor
}

// Error handling function
void handle_uploadf(int sockfd, char *filename, char *dest_path) 
{
    // Verify file exists
    struct stat st;
    if (stat(filename, &st) != 0) 
    {
        printf("ERROR: File '%s' not found\n", filename);
        return;
    }
    
    // Check if destination path starts with ~S1/
    if (strncmp(dest_path, "~S1/", 4) != 0) 
    {
        printf("ERROR: Destination path must start with ~S1/\n");
        return;
    }
    
    // Check file extension
    char *ext = strrchr(filename, '.');
    if (ext == NULL) 
    {
        printf("ERROR: File has no extension\n");
        return;
    }
    
    // Check if file type is supported
    if (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && strcmp(ext, ".txt") != 0 && strcmp(ext, ".zip") != 0) 
    {
        printf("ERROR: Unsupported file type. Only .c, .pdf, .txt, .zip allowed\n");
        return;
    }
    
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "uploadf %s %s", filename, dest_path);
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        error("ERROR writing to socket");
        return;
    }
    
    // Wait for server response
    char response[BUFFER_SIZE];
    bzero(response, BUFFER_SIZE);
    if (read(sockfd, response, BUFFER_SIZE - 1) < 0) 
    {
        error("ERROR reading from socket");
        return;
    }
    
    // Check server response
    if (strcmp(response, "READY") == 0) 
    {
        // Server is ready to receive file
        if (send_file(sockfd, filename) == 0) 
        {
            // Wait for final response
            bzero(response, BUFFER_SIZE);
            if (read(sockfd, response, BUFFER_SIZE - 1) < 0) 
            {
                error("ERROR reading from socket");
                return;
            }
            printf("%s\n", response);
        }
    } 
    else 
    {
        printf("%s\n", response);
    }
}

// Error handling function
void handle_downlf(int sockfd, char *filename) 
{
    // Check if filename starts with ~S1/
    if (strncmp(filename, "~S1/", 4) != 0) 
    {
        printf("ERROR: Filename must start with ~S1/\n");
        return;
    }
    
    // Check file extension
    char *ext = strrchr(filename, '.');
    if (ext == NULL) 
    {
        printf("ERROR: File has no extension\n");
        return;
    }
    
    // Check if file type is supported
    if (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && 
        strcmp(ext, ".txt") != 0 && strcmp(ext, ".zip") != 0) 
        {
        printf("ERROR: Unsupported file type. Only .c, .pdf, .txt, .zip allowed\n");
        return;
    }
    
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downlf %s", filename);
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        error("ERROR writing to socket");
        return;
    }
    
    // Get the base name for saving locally
    char *base_name = basename(filename);
    
    // Receive file from server
    if (receive_file(sockfd, base_name) == 0) 
    {
        printf("File '%s' downloaded successfully\n", base_name);
    }
}

// Error handling function
void handle_removef(int sockfd, char *filename) 
{
    // Check if filename starts with ~S1/
    if (strncmp(filename, "~S1/", 4) != 0) 
    {
        printf("ERROR: Filename must start with ~S1/\n");
        return;
    }
    
    // Check file extension
    char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        printf("ERROR: File has no extension\n");
        return;
    }
    
    // Check if file type is supported
    if (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && strcmp(ext, ".txt") != 0 && strcmp(ext, ".zip") != 0)
    {
        printf("ERROR: Unsupported file type. Only .c, .pdf, .txt, .zip allowed\n");
        return;
    }
    
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "removef %s", filename);
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        error("ERROR writing to socket");
        return;
    }
    
    // Get server response
    char response[BUFFER_SIZE];
    bzero(response, BUFFER_SIZE);
    if (read(sockfd, response, BUFFER_SIZE - 1) < 0) 
    {
        error("ERROR reading from socket");
        return;
    }
    
    printf("%s\n", response);
}

// Error handling function
void handle_downltar(int sockfd, char *filetype) 
{
    // Check file type
    if (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".pdf") != 0 && strcmp(filetype, ".txt") != 0) 
    {
        printf("ERROR: Unsupported file type for tar. Only .c, .pdf, .txt allowed\n");
        return;
    }
    
    // Determine output filename
    char output_file[50];
    if (strcmp(filetype, ".c") == 0) 
    {
        strcpy(output_file, "cfiles.tar");
    } 
    else if (strcmp(filetype, ".pdf") == 0) 
    {
        strcpy(output_file, "pdfiles.tar");
    } else 
    {
        strcpy(output_file, "txtfiles.tar");
    }
    
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downltar %s", filetype);
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        error("ERROR writing to socket");
        return;
    }
    
    // Receive tar file from server
    if (receive_file(sockfd, output_file) == 0) 
    {
        printf("Tar file '%s' downloaded successfully\n", output_file);
    }
}

// Error handling function
void handle_dispfnames(int sockfd, char *pathname) 
{
    // Check if pathname starts with ~S1/
    if (strncmp(pathname, "~S1/", 4) != 0) 
    {
        printf("ERROR: Pathname must start with ~S1/\n");
        return;
    }
    
    // Send command to server
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "dispfnames %s", pathname);
    if (write(sockfd, command, strlen(command)) < 0) 
    {
        error("ERROR writing to socket");
        return;
    }
    
    // Get server response
    char response[BUFFER_SIZE];
    bzero(response, BUFFER_SIZE);
    if (read(sockfd, response, BUFFER_SIZE - 1) < 0) 
    {
        error("ERROR reading from socket");
        return;
    }
    
    printf("Files in %s:\n%s", pathname, response);
}

// Function to send a file to the server
int send_file(int sockfd, char *filename) 
{
    int fd;
    char buffer[BUFFER_SIZE];
    ssize_t n;
    
    // Open file
    fd = open(filename, O_RDONLY);
    if (fd < 0) 
    {
        printf("ERROR: Failed to open file '%s'\n", filename);
        return -1;
    }
    
    // Get file size
    struct stat st;
    if (fstat(fd, &st) < 0) 
    {
        printf("ERROR: Failed to get file size\n");
        close(fd);
        return -1;
    }
    
    // Send file size
    if (write(sockfd, &st.st_size, sizeof(off_t)) < 0) 
    {
        error("ERROR writing file size to socket");
        close(fd);
        return -1;
    }
    
    // Send file data
    off_t remaining = st.st_size;
    while (remaining > 0) 
    {
        n = read(fd, buffer, BUFFER_SIZE);
        if (n <= 0) // Error or end of file
        {
            printf("ERROR: Failed to read from file\n");
            close(fd);
            return -1;
        }
        // Send data to server
        if (write(sockfd, buffer, n) < 0) 
        {
            error("ERROR writing to socket");
            close(fd);
            return -1;
        }
        
        remaining -= n;
    }
    
    close(fd);
    return 0;
}
// Function to receive a file from the server
int receive_file(int sockfd, char *filename) 
{
    int fd;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    // Peek into the socket to check if response starts with "ERROR"
    char peek_buf[6] = {0}; // 5 + null terminator
    n = recv(sockfd, peek_buf, 5, MSG_PEEK);  // Look at first 5 bytes without removing them from buffer

    // Check for error in peeking
    if (n <= 0) 
    {
        printf("ERROR: Failed to read from socket\n");
        return -1;
    }

    // Check if the response starts with "ERROR"
    if (strncmp(peek_buf, "ERROR", 5) == 0) 
    {
        // Read full error message now
        char error_msg[BUFFER_SIZE] = {0};
        read(sockfd, error_msg, BUFFER_SIZE - 1);
        printf("%s\n", error_msg);
        return -1;
    }

    // Read file size
    off_t file_size;
    if (read(sockfd, &file_size, sizeof(off_t)) != sizeof(off_t)) 
    {
        printf("ERROR: Failed to read file size\n");
        return -1;
    }

    // Check if file size is valid
    if (file_size <= 0) 
    {
        printf("ERROR: Invalid file size received: %ld\n", (long)file_size);
        return -1;
    }

    // Create file
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("ERROR: Failed to create file '%s'\n", filename);
        return -1;
    }

    // Receive file data
    off_t remaining = file_size;
    while (remaining > 0) 
    {
        // Read data from socket
        n = read(sockfd, buffer, (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
        if (n <= 0) 
        {
            printf("ERROR: File transfer failed\n");
            close(fd);
            unlink(filename);  // Delete partially written file
            return -1;
        }

        // Write data to file
        if (write(fd, buffer, n) < 0) 
        {
            printf("ERROR: Failed to write to file\n");
            close(fd);
            unlink(filename);
            return -1;
        }

        remaining -= n; // Update remaining bytes
    }

    close(fd); // Close the file
    return 0; // Success
}

// Error handling function
void error(const char *msg) 
{
    perror(msg);
}