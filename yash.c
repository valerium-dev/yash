#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

struct Job {
    int status;
};

void tokenizeString(char * string, char ** tokens);

int main() {
    int cPID;
    char *cmd;
    char **tokens;
    struct Job jobs[20];

    while(1) {
        cmd = readline("# ");
        tokens = malloc(sizeof(cmd));
        tokenizeString(cmd, tokens);
        cPID = fork();
        if (cPID == 0) {
            execvp(*tokens, tokens);
            perror("Error");
        } else {
            waitpid(cPID, NULL, WUNTRACED);
        }
    }
}

void tokenizeString(char * string, char ** tokens) {
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
    tokens = temp;
}

