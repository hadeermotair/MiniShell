// Hadeer Motair
//I pledge my honor to have abided by the stevens honor system
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"

volatile sig_atomic_t interrupted = 0;

void sigint_handler(int sig) {
    interrupted = sig;
}

void print_pwd() {
    char *current_dir = getcwd(NULL, 0);
    if (current_dir != NULL) {
        printf("%s\n", current_dir);
        free(current_dir);
    } else {
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
    }
}

void list_files() {
    DIR *dir;
    struct dirent *entry;

    char *current_dir = getcwd(NULL, 0);
    if (current_dir == NULL) {
        fprintf(stderr, "Error: Cannot open current directory. %s.\n", strerror(errno));
        return;
    }

    dir = opendir(current_dir);
    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open directory '%s'. %s.\n", current_dir, strerror(errno));
        free(current_dir);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    free(current_dir);
}

void list_processes() {
    DIR *dir;
    struct dirent *entry;

    dir = opendir("/proc");
    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open /proc directory. %s.\n", strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strtol(entry->d_name, NULL, 10) > 0) {
            char cmdline[256];  // Declare cmdline here
            char path[512];
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                ssize_t bytes = read(fd, cmdline, sizeof(cmdline) - 1);
                if (bytes > 0) {
                    cmdline[bytes] = '\0';
                } else {
                    strcpy(cmdline, ""); // Set an empty string if command line couldn't be read
                }
                close(fd);
                
                // Continue to use cmdline below
                char status_path[512];
                sprintf(status_path, "/proc/%s/status", entry->d_name);
                FILE *status = fopen(status_path, "r");
                if (status != NULL) {
                    char line[256];
                    while (fgets(line, sizeof(line), status)) {
                        if (strncmp(line, "Uid:", 4) == 0) {
                            int uid;
                            sscanf(line, "Uid: %d", &uid);
                            struct passwd *pw = getpwuid(uid);
                            if (pw != NULL) {
                                printf("%s %s %s\n", entry->d_name, pw->pw_name, cmdline);
                            }
                            break;
                        }
                    }
                    fclose(status);
                }
            }
        }
    }

    closedir(dir);
}
        
      

int main() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    char command[100];
    char* current_dir;
    
    while (1) {
        interrupted = 0;
        
        current_dir = getcwd(NULL, 0);
        if (current_dir == NULL) {
            fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // Updated prompt format as per assignment requirements
        printf("%s[%s]%s> ", BLUE, current_dir, DEFAULT);
        fflush(stdout);
        free(current_dir);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            if (interrupted) {
                printf("\n");
                continue;
            }
            fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // Remove trailing newline character
        if (command[strlen(command) - 1] == '\n') {
            command[strlen(command) - 1] = '\0';
        }
        
        if (strcmp(command, "exit") == 0) {
            exit(EXIT_SUCCESS);
        } else if (strcmp(command, "pwd") == 0) {
            print_pwd();
        } else if (strcmp(command, "lf") == 0) {
            list_files();
        } else if (strcmp(command, "lp") == 0) {
            list_processes();
        } else if (strncmp(command, "cd", 2) == 0) {
            char* path = strtok(command + 3, " "); // Skip the "cd" part and extract the path
            if (path == NULL) {
                if (chdir(getenv("HOME")) != 0) {
                    fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", getenv("HOME"), strerror(errno));
                }
            } else {
                char* extra_args = strtok(NULL, " "); // Check for extra arguments
                if (extra_args != NULL) {
                    fprintf(stderr, "Error: Too many arguments to cd.\n");
                } else {
                    if (strcmp(path, "~") == 0) {
                        // Change to user's home directory
                        const char* home_dir = getenv("HOME");
                        if (chdir(home_dir) != 0) {
                            fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", home_dir, strerror(errno));
                        }
                    } else {
                        if (chdir(path) != 0) {
                            fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", path, strerror(errno));
                        }
                    }
                }
            }
        } else if (strncmp(command, "echo", 4) == 0) {
            // Echo command
            printf("%s\n", command + 5); // Print the text after "echo "
        } else if (strncmp(command, "touch", 5) == 0) {
            // Touch command
            char* filename = strtok(command + 6, " ");
            if (filename == NULL) {
                fprintf(stderr, "Error: Missing filename for touch command.\n");
            } else {
                int fd = open(filename, O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK, 0666);
                if (fd == -1) {
                    perror("touch"); // This will produce an error message similar to the system's
                } else {
                    close(fd);
                }
            }
        } else if (strncmp(command, "ls ", 3) == 0) {
            // LS command
            char* args = strtok(command + 3, " "); // Skip the "ls" part and extract the arguments
            char* filename = strtok(NULL, " "); // Get the filename
            if (args != NULL && strcmp(args, "-l") == 0) {
                if (filename != NULL) {
                    // ls -l with a specific file
                    char* argv[] = {"ls", "-l", filename, NULL};
                    pid_t pid = fork();
                    if (pid < 0) {
                        fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {
                        // Child process
                        execvp("ls", argv);
                        fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    } else {
                        // Parent process
                        int status;
                        if (waitpid(pid, &status, 0) == -1) {
                            fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }
                } else {
                    // ls -l without a specific file
                    char* argv[] = {"ls", "-l", NULL};
                    pid_t pid = fork();
                    if (pid < 0) {
                        fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {
                        // Child process
                        execvp("ls", argv);
                        fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    } else {
                        // Parent process
                        int status;
                        if (waitpid(pid, &status, 0) == -1) {
                            fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            } else {
                fprintf(stderr, "Error: Invalid ls command.\n");
            }
        } else if (strncmp(command, "rm ", 3) == 0) {
            // RM command
            char* filename = strtok(command + 3, " "); // Skip the "rm" part and extract the filename
            if (filename != NULL) {
                char* argv[] = {"rm", filename, NULL};
                pid_t pid = fork();
                if (pid < 0) {
                    fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
                    exit(EXIT_FAILURE);
                } else if (pid == 0) {
                    // Child process
                    execvp("rm", argv);
                    fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                    exit(EXIT_FAILURE);
                } else {
                    // Parent process
                    int status;
                    if (waitpid(pid, &status, 0) == -1) {
                        fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
            } else {
                fprintf(stderr, "Error: Invalid rm command.\n");
            }
        } else {
            // Execute other commands
            pid_t pid = fork();
            
            if (pid < 0) {
                fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                // Child process
                if (strncmp(command, "sleep", 5) == 0) {
                    // Sleep command
                    interrupted = 0;
                    sleep(atoi(command + 6));
                    if (interrupted) {
                        fprintf(stderr, "^C\n");
                        exit(EXIT_FAILURE);
                    } else {
                        exit(EXIT_SUCCESS);
                    }
                } else {
                    // Other commands
                    if (execlp(command, command, NULL) == -1) {
                        fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                    }
                    exit(EXIT_SUCCESS);
                }
            } else {
                int status;
                if (waitpid(pid, &status, 0) == -1) {
                    fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
          
        }
        
     
    }
}
