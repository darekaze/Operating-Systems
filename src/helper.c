#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define CLASSES 1
#define MEETING 2
#define GATHERING 3

typedef struct {
    char *name;
    struct extra *next;
} extra;

typedef struct {
    int ssType;
    char *owner;
    int date; // YYYYMMDD
    int startTime;
    int duration;
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
    if ((strlen(cmd) > 0) && (cmd[strlen (cmd) - 1] == '\n'))
        cmd[strlen (cmd) - 1] = '\0';
}

void splitString(char **wList, char (*p)) {
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

void handleCmd(char (*cmd), int *loop) {
    char **wList = malloc(sizeof(char*) * 1);
    char *p = strtok(cmd," ");

    splitString(wList, p);
    switch(checkType(wList[0])) { // TODO: implement functions
        case 1:
            printf("la\n");
            break;
        case 2:
            printf("lala\n");
            break;
        case 3:
            printf("lalala\n");
            break;
        case 4:
            printf("lol\n");
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


/*-------Main-------*/
int main(int argc, char *argv[]) {
    int loop = 1;
    char cmd[MAX_INPUT_SZ];
    // Initialize
    readUsers(--argc, ++argv);
    // Looping
    while(loop) {
        readInput(cmd);
        handleCmd(cmd, &loop);
    }
    printf("-> Bye!!!!!!\n");
    return 0;
}