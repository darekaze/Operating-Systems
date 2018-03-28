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

/*-------Functions-------*/
void checkUser(int num, char *users[]) {
    if(num < 3 || num > 9) {
        printf("You're trolling: Range should be 3~9 users");
        exit(1);
    }
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

int validateInput(int argc, char **argv,int userNum){
    int tStart = atoi(argv[3]) / 100;
	if(argc < 5 || argc > 4+userNum) {
        printf("Error: Unvalid Argument Number\n");
        return 1;
    }
	if(atoi(argv[2]) < 20180401 || atoi(argv[2]) > 20180414){
		printf("Error: Unvalid Date\n");
		return 1;
	}
	if(tStart < 8 || tStart > 17){
		printf("Error: Unvalid Starting Time\n");
		return 1;
	}
	if (atoi(argv[4]) < 1 || (atoi(argv[4]) + tStart) > 18){
		printf("Error: Unvalid Duration\n");
		return 1;
	}

    return 0;
    // TODO: add user validation (i.e. ignored user not in list)
}

void addSession(int argc, char **argv, Job **head_ref, int t,int userNum) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));

    if (newJob == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }

    if(validateInput(argc,argv,userNum))
        return;
	
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

void addBatchFile(char *cmd, int toChild[][2]) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    int i;

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
        for(i = 0; i < N_CHILD; i++)
            write(toChild[i][1], line, MAX_INPUT_SZ);
    }
    fclose(fp);
    if (line) free(line);
}

// void childCmd(char (*cmd), Job **jobList, int *loop, char *users[]) {
//     char **wList = malloc(sizeof(char*) * 1);
//     char *p = strtok(cmd, " ");
//     int t, l;

//     l = splitString(wList, p);
//     t = checkType(wList[0]);
//     switch(t) { // TODO: implement functions
//         case 1: case 2: case 3:
//             // addSession(l, wList, jobList, t);
//             break;
//         case 4:
//             // addBatchFile(wList[1], jobList);
//             break;
//         case 5:
//             printf("ahhh\n");
//             break;
//         case 6:
//             printf("wut\n");
//             break;
//         case 0:
//             *loop = 0;
//             break;
//         default:
//             printf("Meh..\n");
//     }

//     free(wList);
// }

void handleCmd(char (*cmd), char *users[], int *loop, int toChild[][2]) {
    char str[MAX_INPUT_SZ] = "";
    int i, t;

    strcpy(str, cmd);
    strtok(cmd," ");
    t = checkType(cmd);
    switch(t) {
        case 1: case 2: case 3: case 6:
            for(i = 0; i < N_CHILD; i++)
                write(toChild[i][1], str, MAX_INPUT_SZ);
            break;
        case 4:
            addBatchFile(str, toChild);
            break;
        case 5:
            printf("ahhh\n");
            break;
        case 0:
            *loop = 0;
            break;
        default:
            printf("Meh..\n");
    }
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

void fcfsScheduler(int fromParent, char *argv[]) {
    int loop = 1;
    Job *accepted = NULL;
    Job *rejected = NULL;

    while(loop) {
        char cmdBuf[MAX_INPUT_SZ] = "";
        read(fromParent, cmdBuf, MAX_INPUT_SZ);
        printf("fcfs: %s\n", cmdBuf);
        loop = 0;
    }
    // debug_print(jobList); // Debug
    // free(jobList);
}

void prScheduler(int fromParent, char *argv[]) {
    // TODO: implement it
}

/*-------Main-------*/
int main(int argc, char *argv[]) {
    int toChild[N_CHILD][2];
    pid_t shut_down[N_CHILD];
    int pid, i, j;

    checkUser(--argc, ++argv);
    // Pipes
    for(i = 0; i < N_CHILD; i++) {
        if(pipe(toChild[i]) < 0) {
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
                if(j != i) close(toChild[j][0]);
            }
            switch(i) { // read: toChild[i][0] write: toParent[i][1]
                case 0:
                    fcfsScheduler(toChild[i][0], argv);
                    break;
                case 1:
                    prScheduler(toChild[i][0], argv);
                    break;
                default:
                    printf("Here goes nothing\n");
                    break;
            }
            close(toChild[i][0]);
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
            handleCmd(cmd, argv, &loop, toChild);
        }
        for(j = 0; j < N_CHILD; j++) close(toChild[j][1]);
    }
    /* prevent zombie */
    for(i = 0; i < N_CHILD; i++)
        waitpid(shut_down[i], NULL, 0);
    
    printf("-> Bye!!!!!!\n");
    return 0;
}