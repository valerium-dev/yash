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
void prepareRedirCommand(char** tokens, int* fileErr);
void signalHandler(int signo);
void resetStdFD(int in, int out, int err);
char** searchPipe(char** tokens);

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
            printf("\nExiting yash...\n");
            exit(0);
        }
        
        command = malloc(strlen(read) + MAX_CMD_LENGTH/4);
        tokenizeString(read, command);
        command2 = searchPipe(command);

        if (command2 != NULL) {
            pipe(pipefd);
            pid = fork();
            if (pid == CHILD) {
                resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                prepareRedirCommand(command, &fileErr);
                if (fileErr != NOFILE)
                    execvp(*command, command);
                exit(0);
            } 
            
            pid = fork();
            if (pid == CHILD) {
                resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                prepareRedirCommand(command2, &fileErr);
                if (fileErr != NOFILE)
                    execvp(*command2, command2);
                exit(0);
            }
            close(pipefd[0]);
            close(pipefd[1]);
            
            waitpid(-1, &status, WUNTRACED);
            waitpid(-1, &status, WUNTRACED);
            
            resetStdFD(stdin_cp, stdout_cp, stderr_cp);
            fileErr = 0;
            
            free(command);
            free(read);
        } else {
            pid = fork();
            if (pid == CHILD) {
                prepareRedirCommand(command, &fileErr);
                if (fileErr != NOFILE)
                    execvp(*command, command);
                exit(0);
            }
            waitpid(pid, &status, WUNTRACED);
            
            resetStdFD(stdin_cp, stdout_cp, stderr_cp);
            fileErr = 0;
            
            free(command);
            free(read);
        }
    }
}

void tokenizeString(char* string, char** tokens) {
    char** temp = tokens;
    char* tokenStart = string;

    while(*string != '\0') {
        if (*string == ' ') {
            *string = '\0';
            string++;
            *tokens = tokenStart;
            tokens++;
            tokenStart = string;
        } else {
            string++;
            if (*string == '\0') {
                *tokens = tokenStart;
            }
        }
    }

    *tokens++;
    *tokens = NULL;
    tokens = temp;
}

void prepareRedirCommand(char** tokens, int* fileErr) {
    int outFile;
    int inFile;
    int errFile;

    char** topOfTokens = tokens;

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
            return;
        }

        *topOfTokens++;
    }
}

char** searchPipe(char** tokens){
    char** temp = tokens;
    while(*temp != NULL) {
        if (strcmp(*temp, "|") == 0) {
            *temp = NULL;
            *temp++;
            return temp;
        }

       *temp++;
    }
    return NULL;
}

void signalHandler(int signo){
    if (signo == SIGINT) {
        
    }
}

void resetStdFD(int in, int out, int err){
    dup2(in, STDIN_FILENO);
    dup2(out, STDOUT_FILENO);
    dup2(err, STDERR_FILENO);
}
