#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define CLASSES 1
#define MEETING 2
#define GATHERING 3
#define N_CHILD 2 // How many scheduler

typedef struct extra {
    char name[20];
    struct extra *next;
} extra;

typedef struct Job {
    int ssType;
    char owner[20];
    int date; // YYYYMMDD
    int startTime; // hr only
    int endTime; // hr only
    extra remark;
    struct Job *next;
} Job;


/* Prototype */
void readInput(char (*));
int splitString(char**, char (*));
int checkType(char*);
void debug_print(Job*);

void checkUser(int, char *[]);
void handleCmd(char (*), char *[], int *, int [][2]);
void addRequest(char *, char *[], int [][2]);
int validateAdd(int, char **, char *[]);
void addBatchFile(char*, char*[], int [][2]);

void scheduleHandler(int, int, char *[]);
void add_fcfsByInput(int, char**, Job**, int);


/*-------Main-------*/
int main(int length, char *wList[]) {
    int toChild[N_CHILD][2], toParent[N_CHILD][2];
    pid_t shut_down[N_CHILD];
    int pid, i, j;

    checkUser(--length, ++wList);
    // Pipes
    for(i = 0; i < N_CHILD; i++) {
        if(pipe(toChild[i]) < 0 || pipe(toParent[i]) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }
    }
    // Fork
    for(i = 0; i < N_CHILD; i++) {
        pid = fork();
        if(pid < 0) {
            printf("Error");
            exit(1);
        } 
        else if (pid == 0) { /* child */
            for(j = 0; j < N_CHILD; j++){
                close(toChild[j][1]);
                close(toParent[j][0]);
                if(j != i) {
                    close(toChild[j][0]);
                    close(toChild[j][1]);
                } 
            } // read: toChild[i][0] write: toParent[i][1]
            scheduleHandler(i, toChild[i][0], wList);

            close(toChild[i][0]);
            close(toParent[j][1]);
            exit(0);
        }
        else shut_down[i] = pid;
    }

    if(pid > 0) { /* parent */
        int loop = 1;
        char cmd[MAX_INPUT_SZ];
        for(i = 0; i < N_CHILD; i++) close(toChild[i][0]);
        while(loop) {
            readInput(cmd);
            handleCmd(cmd, wList, &loop, toChild);
        }
        for(j = 0; j < N_CHILD; j++) close(toChild[j][1]);
    }
    /* prevent zombie */
    for(i = 0; i < N_CHILD; i++)
        waitpid(shut_down[i], NULL, 0);
    
    printf("-> Bye!!!!!!\n");
    return 0;
}

/*---------------Functions------------------*/
/*-------Util_part-------*/
void readInput(char (*cmd)) {
    printf("Please enter ->\n");
    fgets(cmd, MAX_INPUT_SZ, stdin);
    /* Remove trailing newline, if there. */
    if ((strlen(cmd) > 0) && (cmd[strlen(cmd) - 1] == '\n'))
        cmd[strlen(cmd) - 1] = '\0';
}

int splitString(char **wList, char (*p)) {
    int i, nSpace = 0;
    while(p) {
        wList = realloc(wList, sizeof(char*) * ++nSpace);
        if(wList == NULL)
            exit(1);
        wList[nSpace-1] = p;
        p = strtok(NULL, " ");
    }
    wList = realloc(wList, sizeof(char*) * (nSpace+1));
    wList[nSpace] = 0;
    return nSpace;
}

int checkType(char *cmd) {
    if      (strcmp(cmd, "endProgram") == 0)    return 0;
    else if (strcmp(cmd, "addClass") == 0)      return 1;
    else if (strcmp(cmd, "addMeeting") == 0)    return 2;
    else if (strcmp(cmd, "addGathering") == 0)  return 3;
    else if (strcmp(cmd, "addBatch") == 0)      return 4;
    else if (strcmp(cmd, "printSchd") == 0)     return 5;
    else if (strcmp(cmd, "printReport") == 0)   return 6;
    return -1;
}

void debug_print(Job *head) {
    printf("Debug-log\n");
    while(head) {
        printf("%d %s %d %d %d\n", 
            head->ssType, head->owner, head->date, head->startTime, head->endTime);
        head = head->next;
    }
    printf("\n");
}

/*-------Parent_part-------*/
void checkUser(int num, char *users[]) {
    if(num < 3 || num > 9) {
        printf("You're trolling: Range should be 3~9 users");
        exit(1);
    }
}

void handleCmd(char (*cmd), char *users[], int *loop, int toChild[][2]) {
    char str[MAX_INPUT_SZ] = "";
    int t;

    strcpy(str, cmd);
    strtok(cmd," ");
    t = checkType(cmd);
    switch(t) {
        case 1: case 2: case 3:
            addRequest(str, users, toChild);
            break;
        case 4:
            addBatchFile(str, users, toChild);
            break;
        case 5:
            printf("printSchedule: Only need to write to specified scheduler\n");
            break;
        case 6:
            printf("printReport: Take turn to call each scheduler\n");
            break;
        case 0:
            *loop = 0;
            break;
        default:
            printf("Meh..\n");
    }
}

void addRequest(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ] = "";
    char **wList = malloc(sizeof(char*) * 1);
    int i, l;

    strcpy(str, cmd);
    strtok(cmd, " ");
    l = splitString(wList, cmd);
    if(validateAdd(l, wList, users)) 
        for(i = 0; i < N_CHILD; i++)
            write(toChild[i][1], str, MAX_INPUT_SZ);
}

int validateAdd(int length, char **wList, char *users[]){
    int tStart = atoi(wList[3]) / 100;
    int userLen = -1;
    int ownerValid = 0;

	if(length < 5) printf("Error: Unvalid Argument Number\n");
    else if(atoi(wList[2]) < 20180401 || atoi(wList[2]) > 20180414) printf("Error: Unvalid Date\n");
	else if(tStart < 8 || tStart > 17)                              printf("Error: Unvalid Starting Time\n");
    else if (atoi(wList[4]) < 1 || (atoi(wList[4]) + tStart) > 18)  printf("Error: Unvalid Duration\n");
    else {
        /* temp method */
        while(users[++userLen] != NULL) {
            if(strcmp(users[userLen], wList[1]) == 0) {
                printf("ok!\n");
                ownerValid = 1;
                break;
            }
        }
        return ownerValid;
        // TODO: Add check other users validity (consider create a function to do it)
    }
    return 0;
}

void addBatchFile(char *cmd, char *users[], int toChild[][2]) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    strtok(cmd, " ");
    line = strtok(NULL, " \n");
    fp = fopen(line,"r");
    if (fp == NULL) {
        printf("Error: No such file\n");
        return;
    }
    while (getline(&line, &len, fp) != -1) {
        if ((strlen(line) > 0) && (line[strlen(line) - 1] == '\n'))
            line[strlen(line) - 1] = '\0';
        addRequest(line, users, toChild);
    }
    fclose(fp);
    if (line) free(line);
}

/*------------Child_part------------*/
void scheduleHandler(int schedulerID, int fromParent, char *wList[]) {
    int loop = 1;
    Job *jobList = NULL;

    while(loop) {
        char cmdBuf[MAX_INPUT_SZ] = "";
        char **wList = malloc(sizeof(char*) * 1);
        char *p;
        int t, l;

        read(fromParent, cmdBuf, MAX_INPUT_SZ);
        p = strtok(cmdBuf, " ");
        l = splitString(wList, p);
        t = checkType(wList[0]);

        switch(t) {
            case 1: case 2: case 3:
                // TODO:  add each scheduler (new function for caseHandler)
                printf("scheduler: %s %s %s %s %s\n",wList[0], wList[1], wList[2], wList[3], wList[4]);
                break;
            case 5:
                printf("for a user's schedule\n");
                break;
            case 6:
                printf("for a report of all users' schedule\n");
                break;
            default:
                printf("Meh..\n");
                break;
        }
        // printf("fcfs: %s\n", wList[2]);
        // loop = 0;
    }
    // debug_print(jobList); // Debug
    // free(jobList);
}

void add_fcfsByInput(int length, char **wList, Job **head_ref, int t) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));
    if (newJob == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }
	
    // input data into list
    newJob->ssType = t;
    strcpy(newJob->owner, wList[1]);
    newJob->date = atoi(wList[2]);
    newJob->startTime = atoi(wList[3]) / 100;
    newJob->endTime = newJob->startTime + atoi(wList[4]);
    newJob->next = NULL;

    if(*head_ref == NULL) {
        *head_ref = newJob;
    } else {
        temp = *head_ref;
        while(temp->next != NULL){
            temp = temp->next;
        }
        temp->next = newJob;
    }
    // printf("%d %s %d %d %d\n", 
    //         newJob->ssType, newJob->owner, newJob->date, newJob->startTime, newJob->duration);
}

/* TODO: add each type of scheduler */
