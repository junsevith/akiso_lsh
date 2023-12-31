//
// Created by pawel on 10.12.2023.
//

#include "executor.h"
#include "piper.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

// stara wersja uruchamiania komend
int lsh_launch(char **args, int args_count) {
    pid_t pid, wpid;
    int status;
    int wait = 1;

    if (args_count > 1 && strcmp(args[args_count - 1], "&") == 0) {
        // jesli ostatni argument to & to nie czekaj na zakonczenie procesu
        args[args_count - 1] = NULL;
        wait = 0;
    }

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("lsh");
    } else {
        // Parent process

        if (wait) {
//            signal(SIGINT, redirect);
            do {
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }

    return 1;
}

/*
  Deklaracje wbudowanych funkcji
 */
int lsh_cd(char **args);

int lsh_help(char **args);

int lsh_exit(char **args);

/*
  Array zawierający nazwy wbudowanych funkcji
 */
char *builtin_str[] = {
        "cd",
        "help",
        "exit"
};

/*
  Tablica wskaźników do wbudowanych funkcji (magia, czary)
 */
int (*builtin_func[])(char **) = {
        &lsh_cd,
        &lsh_help,
        &lsh_exit
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

/*
  Wbudowane funkcje
*/
int lsh_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_help(char **args) {
    int i;
    printf("LSH by Juno\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args) {
    return 0;
}

int lsh_execute(char **args, int args_count) {
    int i;

    if (args[0] == NULL) {
        // Wprowadzono pustą linię
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

//    return lsh_launch(args, args_count);
    return pipe_handler(args, args_count);
}
