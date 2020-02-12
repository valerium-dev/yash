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

typedef enum JobStatus {FG, BGSTOP, BGRUN, DONE} JobStatus;

typedef struct processGroup {
    pid_t id;
    char* proc;
    JobStatus status;
} procGroup;

typedef procGroup ListEntry;

typedef struct ListNode {
    int nodeID;
    ListEntry pg;
    struct ListNode *next;
    struct ListNode *prev;
} ListNode;
ListNode* addEntry(ListEntry entry, ListNode* list);
ListNode* removeEntry(ListNode* list, pid_t pid);
void printList(ListNode* list);
ListNode* findFGEntry(ListNode* list);
ListNode* findBGEntry(ListNode* list);

void tokenizeString(char* string, char** tokens);
void prepareRedirCommand(char** tokens, int* fileErr);
void signalHandler(int signo);
void resetStdFD(int in, int out, int err);
char** searchPipe(char** tokens);
JobStatus searchAmper(char* command);
void killChildren(ListNode* list);
void cleanMemory(ListNode* list);
void markDone(int pid, ListNode* list);
void printDone(ListNode* list);
void purgeDone(ListNode* list);

ListNode* globalJobs = NULL;

int main() {
    int pipefd[2];
    int status, fileErr=0;
    pid_t pid;
    char* read;
    char** command; char** command2;

    int stdin_cp = dup(STDIN_FILENO);
    int stdout_cp = dup(STDOUT_FILENO);
    int stderr_cp = dup(STDERR_FILENO);

    ListNode* jobs = malloc(sizeof(ListNode));
    jobs->nodeID = 0;
    globalJobs = jobs;
    
    while(1) {
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, &signalHandler);
    
        read = readline("# ");
        if (read == NULL) {
            printf("\nExiting yash...\n");
            killChildren(jobs);
            cleanMemory(jobs);
            free(command);
            free(read);
            exit(0);
        }

        char* promptCP = malloc(strlen(read) + 5);
        strcpy(promptCP, read);
        procGroup group = {0, promptCP, searchAmper(promptCP)};

        command = malloc(strlen(read) + MAX_CMD_LENGTH/4);
        tokenizeString(read, command);
        command2 = searchPipe(command);
        
        if (*command != NULL) {
            if (strcmp(*command, "jobs") == 0) {
                if (jobs->next != NULL) {
                    printList(jobs);
                    purgeDone(jobs);
                }
            } else if (strcmp(*command, "fg") == 0) {
                if (jobs->next != NULL) {
                    purgeDone(jobs);
                    ListNode* job = findFGEntry(jobs);
                    if (job->nodeID != 0) {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);

                        if (job->pg.status == BGSTOP) {
                            kill(-1*job->pg.id, SIGCONT);
                            job->pg.status = FG;
                        
                            printf("%s\n", job->pg.proc);
                            tcsetpgrp(0, job->pg.id);
                            waitpid(-1 * job->pg.id, &status, WUNTRACED);
                        
                            if (WIFSTOPPED(status)) {
                                if (WSTOPSIG(status) == SIGTSTP) {
                                    job->pg.status = BGSTOP;
                                } 
                            }
                        } else if (job->pg.status == BGRUN) {
                            job->pg.status = FG;
                            char* tmp = strstr(job->pg.proc, " &");
                            *tmp = '\0';

                            printf("%s\n", job->pg.proc);
                            tcsetpgrp(0, job->pg.id);
                            waitpid(-1 * job->pg.id, &status, WUNTRACED);

                            if (WIFSTOPPED(status)) {
                                if(WSTOPSIG(status) == SIGTSTP) {
                                    job->pg.status = BGSTOP;
                                } 
                            } 
                        }

                        tcsetpgrp(0, getpgid(getpid()));
                        
                        removeEntry(jobs, job->pg.id);
                        printDone(jobs);
                        purgeDone(jobs);

                        resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                        fileErr = 0;
            
                        free(command);
                        free(read);
                        free(job); 
                    } else {
                        printf("yash: fg: no jobs available\n");
                    }
                } else {
                    printf("yash: fg: current: no such job\n");
                }
            } else if (strcmp(*command, "bg") == 0) {
                if (jobs->next != NULL) {
                    ListNode* job = findBGEntry(jobs);
                    if (job->nodeID != 0) {
                        kill(-1*job->pg.id, SIGCONT);
                        job->pg.status = BGRUN;
                        if (strstr(job->pg.proc, " &") == NULL) {
                            strcat(job->pg.proc, " &");
                        }
                        printf("[%d]+ \t%s\n", job->nodeID, job->pg.proc);
                
                        tcsetpgrp(0, getpgid(getpid()));

                        printDone(jobs);
                        purgeDone(jobs);
                    
                        resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                        fileErr = 0;
            
                        free(command);
                        free(read);
                    } else {
                        printf("yash: bg: no stopped job available\n");
                    }
                } else { 
                    printf("yash: bg: current: no such job\n");
                }
            } else if (command2 != NULL) {
                pipe(pipefd);
                pid = fork();
                
                setpgid(pid, group.id);
                group.id = getpgid(pid);

                if (pid == CHILD) {
                    resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    prepareRedirCommand(command, &fileErr);
                    if (fileErr != NOFILE) {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);
                        execvp(*command, command);
                    }
                    exit(0);
                } 
            
                pid = fork();
                setpgid(pid, group.id);

                if (pid == CHILD) {
                    resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                    close(pipefd[1]);
                    dup2(pipefd[0], STDIN_FILENO);
                    prepareRedirCommand(command2, &fileErr);
                    if (fileErr != NOFILE) {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);
                        execvp(*command2, command2);
                    }
                    exit(0);
                }
                close(pipefd[0]);
                close(pipefd[1]);
            
                if (group.status != BGRUN) { 
                    tcsetpgrp(0, group.id);
                    waitpid(-1 * group.id, &status, WUNTRACED);
                    waitpid(-1 * group.id, &status, WUNTRACED);
                    if (WIFSTOPPED(status)) {
                        if(WSTOPSIG(status) == SIGTSTP) {
                            group.status = BGSTOP;
                            addEntry(group, jobs);
                        } else if (WSTOPSIG(status) == SIGCHLD) {
                            group.status = DONE;
                            addEntry(group, jobs);
                        }
                    } 
                } else {
                    addEntry(group, jobs);
                }

                tcsetpgrp(0, getpgid(getpid()));
        
                printDone(jobs);
                purgeDone(jobs);
    
                resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                fileErr = 0;
            
                free(command);
                free(read);
            } else {
                pid = fork();
                setpgid(pid, group.id);
                group.id = getpgid(pid); 

                if (pid == CHILD) {
                    prepareRedirCommand(command, &fileErr);
                    if (fileErr != NOFILE) {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);
                        execvp(*command, command);
                    }
                    exit(0);
                }
                
                if (group.status != BGRUN) {
                    tcsetpgrp(0, group.id);
                    waitpid(-1 * group.id, &status, WUNTRACED);
                    if (WIFSTOPPED(status)) {
                        if (WSTOPSIG(status) == SIGTSTP) {
                            group.status = BGSTOP;
                            addEntry(group, jobs);
                        } else if (WSTOPSIG(status) == SIGCHLD){
                            group.status = DONE;
                            addEntry(group, jobs);
                        }
                    }
                } else {
                    addEntry(group, jobs);
                }
                            
                tcsetpgrp(0, getpgid(getpid()));

                printDone(jobs);
                purgeDone(jobs);
            
                resetStdFD(stdin_cp, stdout_cp, stderr_cp);
                fileErr = 0;
            
                free(command);
                free(read);
            }
        } else {
            printDone(jobs);
            purgeDone(jobs);
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

        if (strcmp(*topOfTokens, "&") == 0) {
            *topOfTokens = NULL;
            return;
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

JobStatus searchAmper(char* command) {
    if (strstr(command, "&") != NULL)
        return BGRUN;
    return FG;
}

void signalHandler(int signo){
    int status;
    int pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
    int pid2 = waitpid(-1, &status, WNOHANG | WUNTRACED);
    
    if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGINT) {
            kill(-1 * getpgid(pid), SIGINT);
        } else if (WSTOPSIG(status) == SIGTSTP) {
            kill(-1 * getpgid(pid), SIGTSTP);
        }
    } else if (WIFEXITED(status)) {
        markDone(pid, globalJobs);
        markDone(pid2, globalJobs);
    }
}

void resetStdFD(int in, int out, int err){
    dup2(in, STDIN_FILENO);
    dup2(out, STDOUT_FILENO);
    dup2(err, STDERR_FILENO);
}

ListNode* addEntry(ListEntry entry, ListNode* list) {
    ListNode* temp = list;
    ListNode* prev;
    
    while (temp->next != NULL) {
        temp = temp->next;
    }

    prev = temp;
    temp->next = malloc(sizeof(ListNode));
    temp = temp->next;
    temp->next = NULL;
    temp->prev = prev;
    temp->pg = entry;
    temp->nodeID = prev->nodeID + 1;
    return temp;
}

ListNode* removeEntry(ListNode* list, pid_t pid) {
    ListNode* temp = list;
    ListNode* prev;
    ListNode* next;
    if (temp->nodeID == 0) {
        if (temp->next == NULL) {
            return NULL;
        }
        temp = temp->next;
    }

    while (temp->pg.id != pid) {
        if (temp->next == NULL) {
            return NULL;
        }
        temp = temp->next;
    }

    prev = temp->prev;
    next = temp->next;
    

}

void printList(ListNode* list) {
    ListNode* temp = list;

    while (temp != NULL) {
        if (temp->nodeID != 0) {
            char fgStat = '-';
            if (temp->next == NULL) {
                fgStat = '+';
            }

            char* status = "Stopped";
            if (temp->pg.status == BGRUN) {
                status = "Running";
            } else if (temp->pg.status == DONE) {
                fgStat = '+';
                status = "Done";
            }
            
            printf("[%d]%c %s\t\t%s\n", temp->nodeID, fgStat, status, temp->pg.proc);
        }
        temp = temp->next;
    }
}

void printDone(ListNode* list) {
    ListNode* temp = list;
    while (temp != NULL) {
        if (temp->pg.status == DONE) {
            printf("[%d]+ Done \t\t%s\n", temp->nodeID, temp->pg.proc);
        }
        temp = temp->next;
    }
}

ListNode* findFGEntry(ListNode* list) {
    ListNode* temp = list->next;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    while (temp->pg.status == DONE) {
        temp = temp->prev;
    }
    return temp;
}
ListNode* findBGEntry(ListNode* list) {
    ListNode* temp = list->next;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    while (temp->pg.status == DONE || temp->pg.status == BGRUN) {
        temp = temp->prev;
    }
    return temp;
}

void killChildren(ListNode* list) {
    ListNode* temp = list;
    
    while (temp != NULL) {
        if (temp->nodeID != 0) {
            kill(-1*temp->pg.id, SIGINT);
        }
        temp = temp->next;
    }
}

void cleanMemory(ListNode* list) {
    while (list != NULL) {
        ListNode* temp = list;
        list = list->next;
        free(temp);
    }
}

void markDone(int pid, ListNode* list) {
    ListNode* temp = list;
    
    if (temp->nodeID == 0) {
        if (temp->next == NULL) {
            return;
        }
        temp = temp->next;
    }

    while (temp->pg.id != pid) {
        if (temp->next == NULL) {
            return;
        }
        temp = temp->next;
    }
    
    temp->pg.status = DONE;
}

void purgeDone(ListNode* list) {
    ListNode* temp = list;

    while (temp->next != NULL) {
        if (temp->pg.status == DONE) {
            removeEntry(list, temp->pg.id);
        }
        temp = temp->next;
    }    
}

