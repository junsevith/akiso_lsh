#include <stdio.h>
#include <stdlib.h>
#include "reader.h"
#include "executor.h"
#include <linux/limits.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void signal_handler(int no) {

    // Gdy dziecko kończy wykonywanie, wysyła SIGCHILD.
    // Parent wtedy wykonuje wait, aby usunąć dziecko,
    // normalnie jest to obsługiwanie przez wait w executor::lsh_launch,
    // ale gdy użyjemy &, lsh nie czeka na dziecko i SIGCHILD obsługiwany jest tutaj.
    if (no == 17) {
        waitpid(-1, NULL, WNOHANG);
        // Opcja WNOHANG sprawia, że nie czekamy, jeśli żadne dziecko nie zakończyło działania.
        // Patrz man waitpid
    } else if (no == 2) {
        // Gdy czekamy na dziecko, sygnały są przekierowywane do niego,
        // więc nie trzeba nic z tym robić.
        // Sygnał jest ignorowany, abyśmy przypadkowo nie wyszli z lsh
//        printf("\n");
    }
}

void lsh_loop(void) {
    char *line;
    char **args;
    int status, args_count;

    signal(SIGCHLD, signal_handler);
    signal(SIGINT, signal_handler);

    do {
        // currect directory
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));

        printf("\e[38;5;178m%s \e[38;5;39mLSH \e[0m>> ", cwd);
        line = lsh_read_line();
        args = lsh_split_line(line, &args_count);
        status = lsh_execute(args, args_count);

        free(line);
        free(args);
    } while (status);
}

int main() {
    lsh_loop();
    return 0;
}

