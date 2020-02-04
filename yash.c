#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

enum redirOpts{input, output, error, inout, none};

struct RCommand{
    char ** command1;
    char ** command2;
    int redirect;
};

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
            perror("Error");
        } else {
            waitpid(cPID, NULL, WUNTRACED);
        }
    }
}

// TODO: Return flag instead of pointer. Pass pointer to func.
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

    *tokens++;
    *tokens = NULL;

    return temp;
}


