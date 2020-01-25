#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <unistd.h>

int main() {
    int cPID;
    char* cmd;
    while(1) {
	cmd = readline("# ");
        cPID = fork();
        if (cPID == 0) {
            execvp(cmd, cmd);
	}
    }
}
