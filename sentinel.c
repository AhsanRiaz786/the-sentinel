#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    int pipe_fd[2];
    pid_t pid;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int status;

    // 1. Compile the user code
    printf("[Sentinel] Compiling user code...\n");
    int compile_status = system("gcc user_code.c -o user_exec");
    if (compile_status != 0) {
        printf("[Sentinel] Compilation failed.\n");
        return 1;
    }
    printf("[Sentinel] Compilation successful.\n");

    // 2. Create Pipe
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return 1;
    }

    // 3. Fork
    pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // --- Child Process ---
        
        // Close read end, we only need to write
        close(pipe_fd[0]);

        // Redirect stdout to pipe
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        
        // Also redirect stderr to pipe to capture errors
        if (dup2(pipe_fd[1], STDERR_FILENO) == -1) {
           perror("dup2 stderr");
           exit(1);
        }

        // Close write end (it's now duplicated to stdout/stderr)
        close(pipe_fd[1]);

        // Execute the user program
        execl("./user_exec", "./user_exec", NULL);

        // If execl returns, it failed
        perror("execl");
        exit(1);

    } else {
        // --- Parent Process ---

        // Close write end, we only need to read
        close(pipe_fd[1]);

        printf("[Sentinel] User code running...\n");
        printf("--- Captured Output ---\n");

        // Read from pipe until EOF
        while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate
            printf("%s", buffer);
        }
        
        printf("-----------------------\n");

        // Close read end
        close(pipe_fd[0]);

        // Wait for child to finish
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("[Sentinel] Child exited with status %d\n", WEXITSTATUS(status));
        } else {
            printf("[Sentinel] Child did not exit normally.\n");
        }
        
        // Cleanup
        remove("user_exec");
    }

    return 0;
}
