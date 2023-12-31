//
// Created by pawel on 11.12.2023.
//

#include "piper.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>

// ustawia odpowiednie przekierowania dla komendy
// mode:
// 0 - read
// 1 - write
// -1 - none
// 2 - all
void redirect_handler(char **command, int length, int mode) {
    //sprawdza po kolei wszystkie słowa czy nie zaczynają się od symbolu przekierowania
//    fprintf(stderr, "%d ", length);
    for (int j = 0; j < length; j++) {
//        fprintf(stderr, "%s ", command[j]);
        if (command[j][0] == '>' && (mode == 1 || mode == 2)) {
            //bierze resztę znalezionego słowa jako ścieżkę do pliku
            char path[PATH_MAX];
            for (int k = 1; k < strlen(command[j]); k++) {
                path[k - 1] = command[j][k];
            }

            int file;
            if ((file = open(path, O_RDWR | O_CREAT | O_APPEND)) == -1) {
                perror("> error");
                _exit(1);
            }

            //dup2 - zastępuje deskryptor pliku podany jako drugi argument pierwszym argumentem
            // 0 - stdin, 1 - stdout, 2 - stderr
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
//    fprintf(stderr, " koniec kom\n");
}

const int MAX_PIPES = 16;

int pipe_handler(char **args, int args_count) {
    int waiting = 1;

    // sprawdzamy, czy ostatnim argumentem jest &
    // jeśli tak, to usuwamy go i nie czekamy na procesy potomne
    if (strcmp(args[args_count - 1], "&") == 0) {
        waiting = 0;
        args[args_count - 1] = NULL;
        args_count--;
    }


    // tablica z pointerami do komend (komenda to tablica słów, pojedyńcze słowo to char* (string))
    char **commands[MAX_PIPES];

    // długość poszczególnych komend
    int com_len[MAX_PIPES];

    // licznik komend
    int com_count = 1;

    //ustawiany na pierwszą komendę
    commands[0] = args;

    //miejsce w tablicy args, gdzie zaczęła się ostatnia komenda
    int last_com = 0;

    // dzielimy input na poszczególne komendy rozdzielone |
    for (int j = 0; j < args_count; ++j) {
        if (strcmp(args[j], "|") == 0) {
            args[j] = NULL;                             //zamieniamy | na NULL, żeby komenda się kończyła w tym miejscu
            commands[com_count] = args + j + 1;         //ustawiamy wskaźnik na kolejną komendę
            com_len[com_count - 1] = j - last_com;      //ustawiamy długość poprzedniej komendy
            last_com = j + 1;                           //ustawiamy miejsce, gdzie zaczęła się nowa komenda
            com_count++;
        }
    }

    if (com_count > 1) {
        // ustawiamy długość ostatniej komendy
        com_len[com_count - 1] = args_count - last_com;
    } else {
        // w wypadku gdy nie ma żadnych pipe pierwsza komenda to cały input
        com_len[0] = args_count;
    }

    // tworzymy pipe dla komend i zapisujemy file descriptor do arraya
    int fd[com_count - 1][2];
    for (int k = 0; k < com_count - 1; k++) {
        if (pipe(fd[k]) == -1) {
            perror("pipe error");
        }
    }

    // zapisujemy pid-y procesów potomnych, żeby móc na nich poczekać
    int pids[com_count];
//    int status, wpid;

    // dla każdej komendy tworzymy nowy proces
    for (int l = 0; l < com_count; l++) {
        int pid = fork();
        if (pid == 0) {
            if (com_count == 1) {
                // w przypadku gdy nie ma pipe
                redirect_handler(commands[l], com_len[l], 2);
            } else {
                if (l == 0) {
                    // pierwszy proces w pipe, zmieniamy tylko output
                    if (dup2(fd[l][1], 1) == -1) {
                        perror("dup2 error on l=commands-1");
                    }

                    redirect_handler(commands[l], com_len[l], 0);

                } else if (l != com_count - 1) {
                    // środkowe procesy w pipe, zmieniamy input i output
                    if (dup2(fd[l][1], 1) == -1) {
                        perror("dup2 error");
                    }
                    if (dup2(fd[l - 1][0], 0) == -1) {
                        perror("dup2 error");
                    }

                    redirect_handler(commands[l], com_len[l], -1);

                } else {
                    // ostatni proces w pipe, zmieniamy tylko input
                    if (dup2(fd[l - 1][0], 0) == -1) {
                        perror("dup2 error on l=0");
                    }

                    redirect_handler(commands[l], com_len[l], 1);
                }
            }

            // zamykamy wszystkie deskryptory w potomnym procesie
            for (int j = 0; j < com_count - 1; j++) {
                close(fd[j][0]);
                close(fd[j][1]);
            }

            // uruchamiamy komendę
            execvp(commands[l][0], commands[l]);

            // kod poniżej wykona się tylko, jeśli execvp zwróci błąd
            perror("execvp error");
            _exit(1);

        } else if (pid < 0) {
            // Błąd przy tworzeniu procesu potomnego
            perror("lsh");
        } else {
            // Proces główny
            pids[l] = pid;
//            sleep(1);
        }
    }

    // zamykamy wszystkie deskryptory w procesie głównym
    for (int m = 0; m < com_count - 1; m++) {
        close(fd[m][0]);
        close(fd[m][1]);
    }

    // jeśli waiting to true, czekamy na wszystkie procesy potomne
    if (waiting) {
        for (int n = com_count - 1; n > -1; n--) {
            if (pids[n] > 0) {
                int status;
                waitpid(pids[n], &status, 0);
            }
        }
    }

    return 1;
}