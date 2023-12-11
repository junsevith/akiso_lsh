//
// Created by pawel on 11.12.2023.
//

#include "piper.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>

// sets fds to files we specify
// modes:
// 0 - read
// 1 - write
// -1 - none
// 2 - all
void addRedirects(char **command, int length, int mode) {
    for (int j = 0; j < length; j++) {
        if (command[j][0] == '>' && (mode == 1 || mode == 2)) {
            char path[PATH_MAX];
            for (int k = 1; k < strlen(command[j]); k++) {
                path[k - 1] = command[j][k];
            }

            int file;
            if ((file = open(path, O_RDWR | O_CREAT | O_APPEND)) == -1) {
                perror("> error");
                _exit(1);
            }

            if (dup2(file, 1) == -1) {
                perror("dup2 error when tryging to redirect to file");
            }

            command[j] = NULL;

        } else if (command[j][0] == '2' && command[j][1] == '>') {
            char path[PATH_MAX];
            for (int k = 2; k < strlen(command[j]); k++) {
                path[k - 2] = command[j][k];
            }

            int file;
            if ((file = open(path, O_RDWR | O_CREAT | O_APPEND)) == -1) {
                perror("2> error");
                _exit(1);
            }
            if (dup2(file, 2) == -1) {
                perror("dup2 error when tryging to redirect to file");
            }

            command[j] = NULL;

        } else if (command[j][0] == '<' && (mode == 0 || mode == 2)) {
            char path[PATH_MAX];
            for (int k = 1; k < strlen(command[j]); k++) {
                path[k - 1] = command[j][k];
            }

            int file;
            if ((file = open(path, O_RDWR | O_CREAT | O_APPEND)) == -1) {
                perror("> error");
                _exit(1);
            }

            if (dup2(file, 0) == -1) {
                perror("dup2 error when tryging to redirect to file");
            }

            command[j] = NULL;

        }
    }
}

const int MAX_PIPES = 16;
int pipe_handler(char **args, int args_count){
    int waiting = 1;
    if(strcmp(args[args_count-1], "&") == 0){
        waiting = 0;
        args[args_count-1] = NULL;
        args_count--;
    }


    char ***commands = malloc(MAX_PIPES * sizeof(char **));
    int com_len[MAX_PIPES];
    int com_count = 1;

    commands[0] = args;
    com_len[0] = 0;

    for (int j = 0; j < args_count; ++j) {
        if (strcmp(args[j], "|") == 0) {
            args[j] = NULL;
            commands[com_count] = args + j + 1;
            if (com_count > 1) {
                com_len[com_count - 1] = j - com_len[com_count - 2] - 1;
            } else {
                com_len[com_count - 1] = j;
            }
            com_count++;
        }
    }

    if (com_count > 1) {
        com_len[com_count - 1] = args_count - com_len[com_count - 2] - 1;
    } else {
        com_len[0] = args_count;
    }

    int fd[com_count - 1][2];
    for (int k = 0; k < com_count - 1; k++) {
        if (pipe(fd[k]) == -1) {
            perror("pipe error");
        }
    }

    // we will store processes pids to wait for them later
    int pids[com_count];
//    int status, wpid;

    for (int l = 0; l < com_count; l++) {
        int pid = fork();
        if (pid == 0) {
            if (com_count > 1) {
                if (l == 0) {
                    // case for the LAST command in pipes
                    if (dup2(fd[l][0], 0) == -1) {
                        perror("dup2 error on l=0");
                    }

                    addRedirects(commands[l], com_len[l], 1);

                } else if (l != com_count - 1) {
                    // case for all the MIDDLE commands in pipes
                    if (dup2(fd[l - 1][1], 1) == -1) {
                        perror("dup2 error");
                    }
                    if (dup2(fd[l][0], 0) == -1) {
                        perror("dup2 error");
                    }

                    addRedirects(commands[l], com_len[l], -1);

                } else {
                    // case for the FRIST command in pipes
                    if (dup2(fd[l - 1][1], 1) == -1) {
                        perror("dup2 error on l=commands-1");
                    }

                    addRedirects(commands[l], com_len[l], 0);
                }
            } else {
                // if we have only one command, we add all redirects to it
                addRedirects(commands[l], com_len[l], 2);
            }

            // we close all unused fds - important!
            for (int j = 0; j < com_count - 1; j++) {
                close(fd[j][0]);
                close(fd[j][1]);
            }

            // we execute the command - process is replaced by it and dies off eventually, freeing resources
//            lsh_launch(commands[l], com_len[l]);
            execvp(commands[l][0], commands[l]);
            // if we get to this part, execvp failed
            perror("execvp error");
            _exit(1);

        } else if (pid < 0) {
            // Error forking
            perror("lsh");
        } else {
            // Parent process
            pids[l] = pid;
        }
    }

//     close fds in parent
    for (int m = 0; m < com_count - 1; m++) {
        close(fd[m][0]);
        close(fd[m][1]);
    }

    // if we decided to wait, wait for all
    if(waiting){
        for (int n = com_count - 1; n > -1; n--) {
            if (pids[n] > 0) {
                int status;
                waitpid(pids[n], &status, 0);
            }
        }
    }

    return 1;
}