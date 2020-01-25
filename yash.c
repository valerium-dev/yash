#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <unistd.h>

char ** tokenizeString(char * string);

int main() {
    int cPID;
    char *cmd;
    char **tokens;

    while(1) {
	    cmd = readline("# ");
	    tokens = tokenizeString(cmd);
	    cPID = fork();
        if (cPID == 0) {
            execvp(*tokens, tokens);
	    } else {
            waitpid(cPID, NULL, 0);
        }
    }
}

char ** tokenizeString(char * string) {
    char ** tokens = malloc(sizeof(string));
    char ** temp = tokens;
    char * tokenStart = string;
    int numCharactersInToken = 0;

    while(*string != NULL) {
        if (*string == ' ') {
            *string = NULL;
            string++;
            *tokens = tokenStart;
            tokens++;
            tokenStart = string;
            numCharactersInToken = 0;
        } else {
            string++;
            numCharactersInToken++;
            if (*string == NULL) {
                *tokens = tokenStart;
            }
        }
    }

    return temp;
}