#!/bin/bash

# Get the absolute path of the script's directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Function to wait for user input
wait_for_enter() {
    echo "Press Enter to continue..."
    read
}

# Function to run client command
run_client_command() {
    echo -e "\n\033[1;33m==== Running command: $1 ====\033[0m"
    # Create a temporary file with the command
    echo "$1" > "$SCRIPT_DIR/cmd.txt"
    # Add exit command to quit after the command
    echo "exit" >> "$SCRIPT_DIR/cmd.txt"
    # Run client with input from the file
    (cd "$SCRIPT_DIR" && ./w25clients 9000 < "$SCRIPT_DIR/cmd.txt")
    rm "$SCRIPT_DIR/cmd.txt"
    wait_for_enter
}

# Function to check if a file exists
check_file_exists() {
    if [ ! -f "$SCRIPT_DIR/$1" ]; then
        echo "Error: File $1 does not exist in $SCRIPT_DIR"
        exit 1
    fi
}

# Function to start a server and wait for it to be ready
start_server() {
    local port=$1
    local server_name=$2
    echo "Starting $server_name on port $port..."
    
    # Kill any existing process on the port
    lsof -ti:$port | xargs kill -9 2>/dev/null
    sleep 1
    
    # Start the server
    (cd "$SCRIPT_DIR" && "./$server_name") &
    local pid=$!
    
    # Wait for the server to start
    local attempts=0
    while ! lsof -ti:$port >/dev/null && [ $attempts -lt 10 ]; do
        sleep 1
        attempts=$((attempts + 1))
    done
    
    if ! lsof -ti:$port >/dev/null; then
        echo "Error: $server_name failed to start on port $port"
        exit 1
    fi
    
    echo "$server_name started successfully"
    return $pid
}

# Function to kill existing server processes
kill_existing_servers() {
    echo "Checking for existing server processes..."
    for port in 9000 9001 9002 9003; do
        # Kill processes using the port
        lsof -ti:$port | xargs kill -9 2>/dev/null
        # Wait for the port to be released
        while lsof -ti:$port >/dev/null; do
            sleep 1
        done
    done
    sleep 2
}

# Create necessary directories
echo "Creating server directories..."
mkdir -p ~/S1 ~/S2 ~/S3 ~/S4

# Clean up any existing files from previous tests
echo "Cleaning previous test files..."
rm -rf ~/S1/* ~/S2/* ~/S3/* ~/S4/*

# Kill any existing server processes
kill_existing_servers

# Start all servers ---------------------------------------------------------------------------------------------------------------------
start_server 9000 "S1"
S1_PID=$!

start_server 9001 "S2"
S2_PID=$!

start_server 9002 "S3"
S3_PID=$!

start_server 9003 "S4"
S4_PID=$!

echo -e "\n\033[1;32m=============================================\033[0m"
echo -e "\033[1;32m=== Comprehensive File Operations Testing ===\033[0m"
echo -e "\033[1;32m=============================================\033[0m\n"

# Create test files in the script's directory --------------------------------------------------------------------------------------------
echo "Creating test files..."
echo "This is a test PDF file" > "$SCRIPT_DIR/test.pdf"
echo "This is a second PDF file" > "$SCRIPT_DIR/test2.pdf"
echo "This is a test TXT file" > "$SCRIPT_DIR/test.txt"
echo "This is a second TXT file" > "$SCRIPT_DIR/test2.txt"
echo "This is a test ZIP file" > "$SCRIPT_DIR/test.zip"
echo "This is a second ZIP file" > "$SCRIPT_DIR/test2.zip"
echo "This is a test C file" > "$SCRIPT_DIR/test.c"
echo "This is a second C file" > "$SCRIPT_DIR/test2.c"

# Verify test files were created --------------------------------------------------------------------------------------------------------------
check_file_exists "test.pdf"
check_file_exists "test2.pdf"
check_file_exists "test.txt"
check_file_exists "test2.txt"
check_file_exists "test.zip"
check_file_exists "test2.zip"
check_file_exists "test.c"
check_file_exists "test2.c"

echo -e "\n\033[1;34m=== TEST 1: Upload Files - Basic Tests ===\033[0m"
# Test uploading to default location  ----------------------------------------------------------------------------------------------------------------
run_client_command "uploadf test.c"
run_client_command "uploadf test.pdf"
run_client_command "uploadf test.txt"
run_client_command "uploadf test.zip"

echo -e "\n\033[1;34m=== TEST 2: Upload Files - With Directory Creation ===\033[0m"
# Test uploading to nested directories (will create directories if they don't exist) -----------------------------------------------------------------
run_client_command "uploadf test2.c ~/S1/folder1/folder2"
run_client_command "uploadf test2.pdf ~/S1/folder1/folder2"
run_client_command "uploadf test2.txt ~/S1/folder1/folder2"
run_client_command "uploadf test2.zip ~/S1/folder1/folder2"

echo -e "\n\033[1;34m=== TEST 3: Display All Filenames ===\033[0m"
# Display all filenames to verify uploads --------------------------------------------------------------------------------------------
run_client_command "dispfnames"

echo -e "\n\033[1;34m=== TEST 4: Download Files from Root ===\033[0m"
# Test downloading files from the root directories -------------------------------------------------------------------------------------------
# Rename downloaded files to avoid conflicts with original test files
run_client_command "downlf test.c"
mv "$SCRIPT_DIR/test.c" "$SCRIPT_DIR/downloaded_test.c"
run_client_command "downlf test.pdf"
mv "$SCRIPT_DIR/test.pdf" "$SCRIPT_DIR/downloaded_test.pdf"
run_client_command "downlf test.txt"
mv "$SCRIPT_DIR/test.txt" "$SCRIPT_DIR/downloaded_test.txt"
run_client_command "downlf test.zip"
mv "$SCRIPT_DIR/test.zip" "$SCRIPT_DIR/downloaded_test.zip"

echo -e "\n\033[1;34m=== TEST 5: Download Files from Nested Directories ===\033[0m"
# Test downloading files from nested directories ----------------------------------------------------------------------------------------
run_client_command "downlf ~/S1/folder1/folder2/test2.c"
mv "$SCRIPT_DIR/test2.c" "$SCRIPT_DIR/downloaded_test2.c"
run_client_command "downlf ~/S1/folder1/folder2/test2.pdf"
mv "$SCRIPT_DIR/test2.pdf" "$SCRIPT_DIR/downloaded_test2.pdf"
run_client_command "downlf ~/S1/folder1/folder2/test2.txt"
mv "$SCRIPT_DIR/test2.txt" "$SCRIPT_DIR/downloaded_test2.txt"
run_client_command "downlf ~/S1/folder1/folder2/test2.zip"
mv "$SCRIPT_DIR/test2.zip" "$SCRIPT_DIR/downloaded_test2.zip"

echo -e "\n\033[1;34m=== TEST 6: Create and Download Tar Files ===\033[0m"
# Test creating and downloading tar files ------------------------------------------------------------------------------------------
run_client_command "downltar c"
run_client_command "downltar pdf"
run_client_command "downltar txt"

echo -e "\n\033[1;34m=== TEST 7: Remove Files from Root ===\033[0m"
# Test removing files from the root directories ---------------------------------------------------------------------------------------------------
run_client_command "removef test.c"
run_client_command "removef test.pdf"
run_client_command "removef test.txt"
run_client_command "removef test.zip"

echo -e "\n\033[1;34m=== TEST 8: Remove Files from Nested Directories ===\033[0m"
# Test removing files from nested directories ------------------------------------------------------------------------------------------------------
run_client_command "removef ~/S1/folder1/folder2/test2.c"
run_client_command "removef ~/S1/folder1/folder2/test2.pdf"
run_client_command "removef ~/S1/folder1/folder2/test2.txt"
run_client_command "removef ~/S1/folder1/folder2/test2.zip"


echo -e "\n\033[1;34m=== TEST 9: Advanced Tests - Multiple Directory Levels ===\033[0m"
# Create new test files -------------------------------------------------------------------------------------------------------------------
echo "This is a multi-level test C file" > "$SCRIPT_DIR/multilevel.c"
echo "This is a multi-level test PDF file" > "$SCRIPT_DIR/multilevel.pdf"
echo "This is a multi-level test TXT file" > "$SCRIPT_DIR/multilevel.txt"
echo "This is a multi-level test ZIP file" > "$SCRIPT_DIR/multilevel.zip"

# Test uploading to multiple directory levels
run_client_command "uploadf multilevel.c ~/S1/level1/level2/level3"
run_client_command "uploadf multilevel.pdf ~/S1/level1/level2/level3"
run_client_command "uploadf multilevel.txt ~/S1/level1/level2/level3"
run_client_command "uploadf multilevel.zip ~/S1/level1/level2/level3"

# Cleanup
echo -e "\n\033[1;34m=== Cleaning up... ===\033[0m"
kill_existing_servers

echo -e "\n\033[1;32m========================================\033[0m"
echo -e "\033[1;32m=== All tests completed successfully ===\033[0m"
echo -e "\033[1;32m========================================\033[0m"