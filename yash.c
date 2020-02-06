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

#define MAX_CMD_LENGTH 2000
#define MAX_REDIR_TYPES 3
#define MAX_JOBS 20
#define CHILD 0
#define S_RALLWU 

typedef enum JobStatus {fg, bg} JobStatus;

typedef struct Job {
    char* name;
    JobStatus status;
} Job;

typedef enum fileR {outputR, inputR, errR} fileR;

typedef struct FCommand {
    char** command;
    int inFile;
    int outFile;
    int errFile;
} FCommand;

void tokenizeString(char* string, char** tokens);
FCommand* searchTokens(char** tokens);
void checkRedirects(FCommand* cmd);
void closeFileDescriptors(FCommand* cmd);
void copyTokens(char** destination, char** source);

int main() {
    int cPID;
    char* read;
    char** tokens;
    Job jobs[MAX_JOBS];

    while(1) {
        read = readline("# ");
        tokens = malloc(sizeof(char)*MAX_CMD_LENGTH);
        tokenizeString(read, tokens);
        FCommand* cmd = searchTokens(tokens); 
        cPID = fork();
        if (cPID == CHILD) {
            checkRedirects(cmd);
            char** prgm = cmd->command;
            execvp(*prgm, prgm);
            perror("Error");
        } else {
            waitpid(cPID, NULL, WUNTRACED);
            free(cmd);
        }
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

FCommand* searchTokens (char** tokens) {
    FCommand* cmd = malloc(sizeof(char)*MAX_CMD_LENGTH + sizeof(int)*MAX_REDIR_TYPES);
    cmd->outFile = 1;
    cmd->inFile = 0;
    cmd->errFile = 2;
    char** temp = tokens;
    int numBeforeRedir = 0; 
    
    while (*temp != NULL) {
        if (strcmp(*temp, ">") == 0) {
            *temp = NULL;
            copyTokens(cmd->command, tokens);
            *temp++;
            char* path = *temp;
            cmd->outFile = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
        
        if (strcmp(*temp, "<") == 0) {    
            // Find file/path token for redir
            // Pass fd to struct to replace stdin
        }
        
        if (strcmp(*temp, "2>") == 0) {
            // Find file/path token for redir
            // Pass fd to struct to replace stderr
        }
        
        numBeforeRedir++;
        *temp++;
    }

    return cmd;
}

void checkRedirects(FCommand* cmd) {
    if (cmd->outFile != STDOUT_FILENO) {
        dup2(cmd->outFile, STDOUT_FILENO);
    }
    if (cmd->inFile != STDIN_FILENO) {
        dup2(cmd->inFile, STDIN_FILENO);
    }
    if (cmd->errFile != STDERR_FILENO) {
        dup2(cmd->errFile, STDERR_FILENO);
    }
}

void closeFileDescriptors(FCommand* cmd) {
    if (cmd->outFile != STDOUT_FILENO) {
       close(cmd->outFile); 
    }
    if (cmd->inFile != STDIN_FILENO) {
        close(cmd->inFile);
    }
    if (cmd->errFile != STDERR_FILENO) {
        close(cmd->errFile);
    }
}

void copyTokens(char** destination, char** source) {
    while (*source != NULL) {
        strcpy(*destination, *source);
        *destination++;
        *source++;
    }
}
