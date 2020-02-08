#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define MAX_CMD_LENGTH 2000
#define MAX_REDIR_TYPES 3
#define MAX_JOBS 20
#define CHILD 0

typedef enum JobStatus {fg, bg} JobStatus;

typedef struct Job {
    char* name;
    JobStatus status;
} Job;

void tokenizeString(char* string, char** tokens);
char** prepareRedirCommand(char** tokens);
void signalHandler(int signo);

int main() {
    int pipefd[2], status;
    pid_t pid;
    char* read;
    char** command; char** command2;
    Job jobs[MAX_JOBS];

    while(1) {
        int stdin_cp = dup(STDIN_FILENO);
        int stdout_cp = dup(STDOUT_FILENO);
        int stderr_cp = dup(STDERR_FILENO);

        read = readline("# ");
        if (read == NULL) {
            if (pid != 0) {
                printf("Exiting yash...\n");
                exit(0);
            }
        }
        
        command = malloc(strlen(read) + MAX_CMD_LENGTH/4);
        tokenizeString(read, command);
        command2 = prepareRedirCommand(command);

        if (command2 != NULL) {
            pipe(pipefd);
            pid = fork();
            if (pid == CHILD) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                execvp(*command, command);
            } 

            prepareRedirCommand(command2);
            pid = fork();
            if (pid == CHILD) {
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                execvp(*command2, command2);
            }
            close(pipefd[0]);
            close(pipefd[1]);
        } else {
            pid = fork();
            if (pid == CHILD) {
                execvp(*command, command);
            }
        }   
                
        waitpid(pid, &status, WUNTRACED);
        
        free(command);
        free(read);

        if (STDIN_FILENO != stdin_cp)
        dup2(stdin_cp, STDIN_FILENO);
        if (STDOUT_FILENO != stdout_cp)
        dup2(stdout_cp, STDOUT_FILENO);
        if (STDERR_FILENO != stderr_cp)
        dup2(stderr_cp, STDERR_FILENO);

    }
}

void tokenizeString(char* string, char** tokens) {
    char** temp = tokens;
    char* tokenStart = string;
    int numCharactersInToken = 0;

    while(*string != '\0') {
        if (*string == ' ') {
            *string = '\0';
            string++;
            *tokens = tokenStart;
            tokens++;
            tokenStart = string;
            numCharactersInToken = 0;
        } else {
            string++;
            numCharactersInToken++;
            if (*string == '\0') {
                *tokens = tokenStart;
            }
        }
    }

    *tokens++;
    *tokens = NULL;
    tokens = temp;
}

char** prepareRedirCommand(char** tokens) {
    int outFile;
    int inFile;
    int errFile;

    char** topOfTokens = tokens;
    char** temp = NULL;

    while (*topOfTokens != NULL) {
        if (strcmp(*topOfTokens, ">") == 0) {
            *topOfTokens = NULL;
            *topOfTokens++;
            char* path = *topOfTokens;
            outFile = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(outFile, STDOUT_FILENO);
        }
        
        if (strcmp(*topOfTokens, "<") == 0) {    
            *topOfTokens = NULL;
            *topOfTokens++;
            char* path = *topOfTokens;
            inFile = open(path, O_RDONLY);
            if (inFile != -1) {
                dup2(inFile, STDIN_FILENO);
            }
        }
        
        if (strcmp(*topOfTokens, "2>") == 0) {
            *topOfTokens = NULL;
            *topOfTokens++;
            char* path = *topOfTokens;
            errFile = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(errFile, STDERR_FILENO);
        }

        if (strcmp(*topOfTokens, "|") == 0) {
            *topOfTokens = NULL;
            *topOfTokens++;
            temp = topOfTokens;
        }
        
        *topOfTokens++;
    }

    return temp;
}

void signalHandler(int signo){
    if (signo == SIGINT) {
        
    }
}
