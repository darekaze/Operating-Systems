#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define CLASSES 1
#define MEETING 2
#define GATHERING 3

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

/*-------Functions-------*/
void readUsers(int num, char *users[]) {
    if(num < 3 || num > 9) {
        printf("You're trolling: Range should be 3~9 users");
        exit(1);
    }
    // TODO: Initialize Structure of User (Their Schedule)
}

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
}

void addSession(int argc, char **argv, Job **head_ref, int t) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));

    // TODO: add check users and other format
    if (newJob == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }
    if(argc < 5) {
        printf("Error: Not enough argument...\n");
        return;
    }
    // input data into list
    newJob->ssType = t;
    strcpy(newJob->owner, argv[1]);
    newJob->date = atoi(argv[2]);
    newJob->startTime = atoi(argv[3]) / 100;
    newJob->endTime = newJob->startTime + atoi(argv[4]);
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

void addBatchFile(char *fname, Job **jobList) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    fp = fopen(fname,"r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
        
    while (getline(&line, &len, fp) != -1) {
        char **chunks = malloc(sizeof(char*) * 1);
        char *p;
        int l, t;
        
        if ((strlen(line) > 0) && (line[strlen(line) - 1] == '\n'))
            line[strlen(line) - 1] = '\0';
        p = strtok(line, " ");
        l = splitString(chunks, p);
        t = checkType(chunks[0]);
        printf("%s\n", chunks[0]);
        addSession(l, chunks, jobList, t);
        free(chunks);
    }
    fclose(fp);
    if (line) free(line);
}

void handleCmd(char (*cmd), Job **jobList, int *loop) {
    char **wList = malloc(sizeof(char*) * 1);
    char *p = strtok(cmd, " ");
    int t, l;

    l = splitString(wList, p);
    t = checkType(wList[0]);
    switch(t) { // TODO: implement functions
        case 1: case 2: case 3:
            addSession(l, wList, jobList, t);
            break;
        case 4:
            addBatchFile(wList[1], jobList);
            break;
        case 5:
            printf("ahhh\n");
            break;
        case 6:
            printf("wut\n");
            break;
        case 0:
            *loop = 0;
            break;
        default:
            printf("Meh..\n");
    }

    free(wList);
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


/*-------Main-------*/
int main(int argc, char *argv[]) {
    int loop = 1;
    char cmd[MAX_INPUT_SZ];
    Job *jobList = NULL;

    readUsers(--argc, ++argv); // Initialize
    while(loop) {
        readInput(cmd);
        handleCmd(cmd, &jobList, &loop);
    }
    printf("-> Bye!!!!!!\n");
    // Debug
    debug_print(jobList);
    free(jobList);
    return 0;
}