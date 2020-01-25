#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <unistd.h>

char ** tokenizeString(char * string);

int main() {
    int cPID;
    char * cmd;
    char ** tokens;

    while(1) {
	cmd = readline("# ");
	tokens = tokenizeString(cmd);
        cPID = fork();
        if (cPID == 0) {
            execvp(cmd, cmd);
	}
    }
}

char ** tokenizeString(char * string) {
    char ** tokens = malloc(sizeof(string));
    char currentChar;

    while(currentChar != '\n') {
        currentChar = *string;
    }

    return tokens;    
}
