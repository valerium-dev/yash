#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    int cPID;
    char *cmd;
    while(1) {
        cmd = readline("#");
        cPID = fork();
        if (cPID == 0) {
             execvp();
        }
    }
}
