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
#define NOFILE -2

typedef enum JobStatus {fg, bg} JobStatus;

typedef struct Job {
    char* name;
    JobStatus status;
} Job;

void tokenizeString(char* string, char** tokens);
char** prepareRedirCommand(char** tokens, int* fileErr);
void signalHandler(int signo);
void resetStdFD(int in, int out, int err);

int main() {
    int pipefd[2];
    int status, fileErr=0;
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
            printf("Exiting yash...\n");
            exit(0);
        }
        
        command = malloc(strlen(read) + MAX_CMD_LENGTH/4);
        tokenizeString(read, command);
        command2 = prepareRedirCommand(command, &fileErr);

        if (command2 != NULL) {
            pipe(pipefd);
            pid = fork();
            if (pid == CHILD && fileErr != NOFILE) {
                close(pipefd[0]);
                if (stdout_cp == STDOUT_FILENO)
                    dup2(pipefd[1], STDOUT_FILENO);
                execvp(*command, command);
            } 
            
            pid = fork();
            resetStdFD(stdin_cp, stdout_cp, stderr_cp);
            prepareRedirCommand(command2, &fileErr);

            if (pid == CHILD && fileErr != NOFILE) {
                close(pipefd[1]);
                if (stdin_cp == STDIN_FILENO)
                    dup2(pipefd[0], STDIN_FILENO);
                execvp(*command2, command2);
            }
            close(pipefd[0]);
            close(pipefd[1]);

        } else {
            pid = fork();
            if (pid == CHILD && fileErr != NOFILE) {
                execvp(*command, command);
            }
        }   
                
        waitpid(pid, &status, WUNTRACED);
        
        free(command);
        free(read);
        
        resetStdFD(stdin_cp, stdout_cp, stderr_cp);
        fileErr = 0;

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

char** prepareRedirCommand(char** tokens, int* fileErr) {
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
            outFile = open(path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(outFile, STDOUT_FILENO);
        }
        
        if (strcmp(*topOfTokens, "<") == 0) {    
            *topOfTokens = NULL;
            *topOfTokens++;
            char* path = *topOfTokens;
            inFile = open(path, O_RDONLY);
            if (inFile != -1) {
                dup2(inFile, STDIN_FILENO);
                *fileErr = 0;
            } else {
                *fileErr = NOFILE;
                perror("yash");
            }
        }
        
        if (strcmp(*topOfTokens, "2>") == 0) {
            *topOfTokens = NULL;
            *topOfTokens++;
            char* path = *topOfTokens;
            errFile = open(path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(errFile, STDERR_FILENO);
        }

        if (strcmp(*topOfTokens, "|") == 0) {
            *topOfTokens = NULL;
            *topOfTokens++;
            temp = topOfTokens;
            return temp;
        }
        
        *topOfTokens++;
    }

    return temp;
}

void signalHandler(int signo){
    if (signo == SIGINT) {
        
    }
}

void resetStdFD(int in, int out, int err){
    if (STDIN_FILENO != in)
        dup2(in, STDIN_FILENO);
    if (STDOUT_FILENO != out)
        dup2(out, STDOUT_FILENO);
    if (STDERR_FILENO != err)
        dup2(err, STDERR_FILENO);
}
