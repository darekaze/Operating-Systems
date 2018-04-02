 #include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define NOR_INPUT_SZ 20
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

void parent_checkUser(int, char *[]);
void parent_write(char *, int [][2]);
void parent_handler(char (*), char *[], int *, int [][2]);
void parent_request(char *, char *[], int [][2]);
int parent_validate(int, char **, char *[]);
void parent_addBatch(char*, char*[], int [][2]);

void scheduler_base(int, int, char *[]);
void scheduler_initJob(Job *, char **, int, int);
void scheduler_selector(int, Job **, Job **, Job **, char **, int, int, int*, int*);
void scheduler_sample(Job **, char **, int, int);
void scheduler_fcfs(Job **, Job **, Job **, char **, int, int, int*, int*);
void scheduler_priority(Job **, Job **, Job **, char **, int, int, int*, int*);
void scheduler_special(Job **, Job **, Job **, char **, int, int, int*, int*);

void printSchd_selector(int schedulerID, Job **head_ref, char **wList, int length, int t);
void printSchd_sample(Job **head_ref, char **wList, int length, int t);
void printSchd_fcfs(Job **head_ref, char **wList, int length, int t);
void printSchd_priority(Job **head_ref, char **wList, int length, int t);
void printSchd_special(Job **head_ref, char **wList, int length, int t);

void printRepo_selector(int schedulerID, Job **acceptList, Job **rejectList, char **wList, int length, int t);
void printRepo_sample(Job **acceptList, Job **rejectList, char **wList, int length, int t);
void printRepo_fcfs(Job **acceptList, Job **rejectList, char **wList, int length, int t);
void printRepo_priority(Job **acceptList, Job **rejectList, char **wList, int length, int t);
void printRepo_special(Job **acceptList, Job **rejectList, char **wList, int length, int t);

void printJob(Job *curJob, FILE *file);

/*-------Main-------*/
int main(int argc, char *argv[]) {
    int toChild[N_CHILD][2], toParent[N_CHILD][2];
    pid_t shut_down[N_CHILD];
    int pid, pid_child, i, j;

    parent_checkUser(--argc, ++argv);
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
        else if (pid == 0) { /* child process */
            for (j = 0; j < N_CHILD; j++) {
                close(toChild[j][1]);
                close(toParent[j][0]);
                if (j != i) {
                    close(toChild[j][0]);
                    close(toParent[j][1]);
                }
            } // read: toChild[i][0] write: toParent[i][1]
            scheduler_base(i, toChild[i][0], argv);

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
            parent_handler(cmd, argv, &loop, toChild);
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
    int nSpace = 0;
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
void parent_checkUser(int num, char *users[]) {
    if(num < 3 || num > 9) {
        printf("You're trolling: Range should be 3~9 users");
        exit(1);
    }
}

void parent_write(char *str, int toChild[][2]) {
    int i;
    for(i = 0; i < N_CHILD; i++)
        write(toChild[i][1], str, MAX_INPUT_SZ);
}

void parent_handler(char (*cmd), char *users[], int *loop, int toChild[][2]) {
    char str[MAX_INPUT_SZ] = "";
    int t;

    strcpy(str, cmd);
    strtok(cmd," ");
    t = checkType(cmd);
    switch(t) {
        case 1: case 2: case 3:
            parent_request(str, users, toChild);
            break;
        case 4:
            parent_addBatch(str, users, toChild);
            break;
        case 5:
            printf("printSchedule: Only need to write to specified scheduler\n");
            break;
        case 6:
            printf("printReport: Take turn to call each scheduler\n");
            break;
        case 0:
            parent_write(str, toChild);
            *loop = 0;
            break;
        default:
            printf("Meh..\n");
    }
}

void parent_request(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ] = "";
    char **wList = malloc(sizeof(char*) * 1);
    int l;

    strcpy(str, cmd);
    strtok(cmd, " ");
    l = splitString(wList, cmd);
    if(parent_validate(l, wList, users))
        parent_write(str,toChild);
}

int parent_validate(int length, char **wList, char *users[]){
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
                ownerValid = 1; // printf("ok!\n"); // debug
                break;
            }
        }
        return ownerValid;
        // TODO: Add check other users validity (consider create a function to do it)
    }
    return 0;
}

void parent_addBatch(char *cmd, char *users[], int toChild[][2]) {
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
        parent_request(line, users, toChild);
    }
    fclose(fp);
    if (line) free(line);
}

/*------------Child_part------------*/
void scheduler_base(int schedulerID, int fromParent, char *wList[]) {
    int loop = 1, pid;
    Job *acceptList[4], *rejectList[4];//-------------------给这四块货每人一个list------------------ 
    int cToG[4][2], gToC[4][2];
    //-------------------开始创建管道----------------------------- 
	int i;
    for (i=0; i<4;i++){
		if (pipe(cToG[i]) < 0){
			printf("create pipe error\n");
			exit(1);
		}
		if (pipe(gToC[i]) < 0){
			printf("create pipe error\n");
			exit(1);
		}
	}
	//----------创建子进程, 关掉没用的pipe口--------------------------------
	for (i=0; i < n; i++){
		if (pid < 0) {
			printf("fork failed\n");
			exit(1);
		}
		if (pid==0) break;
		if (pid > 0) pid = fork();
	}
	i--;//--------------保持子进程从零开始--------------------------
	if (pid > 0){
		int k;
		for (k=0; k < n ;k++){
			close(cToG[k][0]);
			close(gToC[k][1]);
		}
	}
	else if (pid == 0){
		close(cToG[i][1]);
		close(gToC[i][0]);
	}
	//----------------创建结束--------------------------------- 
    
    
    //--------------------分进程开始讨论-------------------------- 
    else if(pid == 0){  /* grandchild process */
        char pipewrite[NOR_INPUT_SZ], piperead[NOR_INPUT_SZ];

        sprintf(pipewrite,"%s%s", wList[2], wList[3]);
        write(cToG[1], pipewrite, NOR_INPUT_SZ);
        grandchildProcess(cToG, gToC);
    } else{ /* child process */
        while(loop) {
            char cmdBuf[MAX_INPUT_SZ] = "";
            char **wList = malloc(sizeof(char*) * 1);
            int t, l;

            read(fromParent, cmdBuf, MAX_INPUT_SZ);
            strtok(cmdBuf, " ");
            l = splitString(wList, cmdBuf);
            t = checkType(wList[0]);

            switch(t) {
                case 1: case 2: case 3:
                    // TODO: add each scheduler (new function for caseHandler)
                    scheduler_selector(schedulerID,&acceptList,&rejectList,wList,l,t,cToG,gToC);
                    break;
                case 5:
                    printSchd_selector(schedulerID, &acceptList, wList, l, t);
                    break;
                case 6:
                    printRepo_selector(schedulerID, &acceptList, &rejectList, wList, l, t);
                    break;
                case 0:
                    loop = 0; break;
                default:
                    printf("Meh..\n");
                    break;
            }
        }
    }

    debug_print(jobList);
    free(jobList);
}

void scheduler_selector(int schedulerID, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t, int *cToG, int *gToC) {
    switch(schedulerID) {
        case 0: scheduler_sample(head_ref, achead_ref, rehaead_ref, wList, length, t, cToG, gToC); break;
        case 1: scheduler_fcfs(head_ref, achead_ref, rehaead_ref, wList, length, t, cToG, gToC); break;
        case 2: scheduler_priority(head_ref, achead_ref, rehaead_ref, wList, length, t, cToG, gToC); break;
        case 3: scheduler_special(head_ref, achead_ref, rehaead_ref, wList, length, t, cToG, gToC); break;
        default: break;
    }
}

void scheduler_initJob(Job *newJob, char **wList, int length, int t) {
    if (newJob == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }
    newJob->ssType = t;
    strcpy(newJob->owner, wList[1]);
    newJob->date = atoi(wList[2]);
    newJob->startTime = atoi(wList[3]) / 100;
    newJob->endTime = newJob->startTime + atoi(wList[4]);
    newJob->next = NULL;
    // TODO: add other user's who also join this event
}

/* TODO: add each type of scheduler (use scheduler_fcfs_sample() as your template) */
void scheduler_sample(Job **head_ref, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t, int *cToG, int *gToC) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));

    scheduler_initJob(newJob, wList, length, t); // input data into list

    /* ---Start implement how to insert your schedule--- */
    if(*head_ref == NULL) {
        *head_ref = newJob;
    } else {
        temp = *head_ref;
        while(temp->next != NULL){
            temp = temp->next;
        }
        temp->next = newJob;
    }
    /* ---end here--- */

    // Just for debug
    // printf("%d %s %d %d %d\n",
    //         newJob->ssType, newJob->owner, newJob->date, newJob->startTime, newJob->endTime);
}

void scheduler_fcfs(Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t, int *cToG, int *gToC) {
    Job *temp, *newJob = (Job *)malloc(sizeof(Job));
    char piperead[NOR_INPUT_SZ], pipewrite[NOR_INPUT_SZ];

    scheduler_initJob(newJob, wList, length, t);
    read(cToG[0], piperead, NOR_INPUT_SZ);
    if(strcmp(piperead, "Y") == 0){
        if(*achead_ref == NULL){
            *achead_ref = newJob;
        }
        else{
            temp = *achead_ref;
            while (temp->next != NULL){
                temp = temp->next;
            }
            temp->next = newJob;
        }
    }
    else if(strcmp(piperead, "N") == 0){
        if(*rehead_ref == NULL){
            *rehead_ref = newJob;
        }
        else{
            temp = *rehead_ref;
            while (temp->next != NULL){
                temp = temp->next;
            }
            temp->next = newJob;
        }
    }
}

void scheduler_priority(Job **head_ref, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t, int *cToG, int *gToC) {
	 

}

void scheduler_special(Job **head_ref, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t, int *cToG, int *gToC) {

}

//print the schedule
void printSchd_selector(int schedulerID, Job **head_ref, char **wList, int length, int t){
    switch(schedulerID) {
        case 0: printSchd_sample(head_ref, wList, length, t);  break;
        case 1: printSchd_fcfs(head_ref, wList, length, t);  break;
        case 2: printSchd_priority(head_ref, wList, length, t);  break;
        case 3: printSchd_special(head_ref, wList, length, t);  break;
    }
}

void printSchd_sample(Job **head_ref, char **wList, int length, int t){
    File *file;
    char filename[MAX_INPUT_SZ];

    strcpy(filename, wList[4]);
    file = fopen(filename, "w");
}

void printSchd_fcfs(Job **head_ref, char **wList, int length, int t){
    File *file;
    char filename[MAX_INPUT_SZ], name[MAX_INPUT_SZ];
    int Jobcount=0, Hour=0;

    //Count the job
    Job *temp = malloc(sizeof(Job) * 1);
    temp = *head_ref;
    while (temp != NULL){
        Hour += temp->endTime - temp->startTime;
        Jobcount++;
        temp = temp->next;
    } temp = *head_ref;

    strcpy(name, wList[1]);
    strcpy(filename, wList[4]);
    file = fopen(filename, "w");
    fprintf(file, "Personal Organizer\n***Appointment Schedule***\n\n");
    if(Jobcount > 1)fprintf(file, "%s, you have %d appointments.\n", name, Jobcount);
    else fprintf(file, "%s, you have %d appointment.\n", name, Jobcount);
    fprintf(file, "Algorithm used: First come first served\n");
    fprintf(file, "%d timeslots occupied.\n\n", Hour);
    fprintf(file, "Date         Start   End     Type         Remarks\n");
    fprintf(file, "=========================================================================\n");
    while (temp != NULL){
        printJob(temp, file);
        temp = temp->next;
    }
    fprintf("   - END -\n=========================================================================\n");
    free(temp);
}

void printSchd_priority(Job **head_ref, char **wList, int length, int t){

}

void printSchd_special(Job **head_ref, char **wList, int length, int t){

}

void printRepo_selector(int schedulerID, Job **acceptList, Job **rejectList, char **wList, int length, int t){
    switch(schedulerID){
        case 0: printSchd_sample(acceptList, rejectList, wList, length, t); break;
        case 1: printSchd_fcfs(acceptList, rejectList, wList, length, t); break;
        case 2: printSchd_priority(acceptList, rejectList, wList, length, t); break;
        case 3: printSchd_special(acceptList, rejectList, wList, length, t); break;
    }
}

void printRepo_sample(Job **acceptList, Job **rejectList, char **wList, int length, int t){

}

void printRepo_fcfs(Job **acceptList, Job **rejectList, char **wList, int length, int t){
    File *file;
    char filename[MAX_INPUT_SZ];
    int acJobcount=0, reJobcount=0, achour=0, rehour=0;

    //Count the job
    Job *temp = malloc(sizeof(Job) * 1);
    temp = *acceptList;
    while (temp != NULL){
        achour += temp->endTime - temp->startTime;
        acJobcount++;
        temp = temp->next;
    }
    temp = *rejectList;
    while (temp != NULL){
        rehour += temp->endTime - temp->startTime;
        reJobcount++;
        temp = temp->next;
    }

    strcpy(filename, wList[1]);
    file = fopen(filename, "w");
    fprintf(file, "Personal Organizer\n***Schedule Report***\n");
    fprintf(file, "Algorithm used: First come first served\n\n");
    fprintf(file, "***Accepted List***\nThere are %d requests accepted.\n\n", acJobcount);
    fprintf(file, "Date         Start   End     Type         Remarks\n");
    fprintf(file, "=========================================================================\n");
    temp = acceptList;
    while (temp != NULL){
        printJob(temp, file);
        temp = temp->next;
    }
    fprintf(file, "=========================================================================\n");
    fprintf(file, "=========================================================================\n");
    fprintf(file, "***Rejected List***\nThere are %d requests accepted.\n\n", reJobcount);
    fprintf(file, "Date         Start   End     Type         Remarks\n");
    fprintf(file, "=========================================================================\n");
    temp = acceptList;
    while (temp != NULL){
        printJob(temp, file);
        temp = temp->next;
    }
    fprintf(file, "=========================================================================\n\n");
    fprintf(file, "Total number of request:     %d\n", achour+rehour);
    fprintf(file, "Timeslot in use:             %d hours\n", achour);
    fprintf(file, "Timeslot not in use:         %d\n", rehour);
    fprintf(file, "hours Utilization:                 %d %%\n", achour*1000(achour+rehour));
    fprintf("   - END -\n");
    free(temp);
}

void printRepo_priority(Job **acceptList, Job **rejectList, char **wList, int length, int t){

}

void printRepo_special(Job **acceptList, Job **rejectList, char **wList, int length, int t){

}

void printJob(Job *curJob, FILE *file){
    char YY[NOR_INPUT_SZ], MM[NOR_INPUT_SZ], DD[NOR_INPUT_SZ], type[NOR_INPUT_SZ];
    char starttime[NOR_INPUT_SZ], endtime[NOR_INPUT_SZ], remark[MAX_INPUT_SZ];
    fprintf(YY, "%d%d%d%d", curJob->date/10000000, (curJob->date/1000000)%10, (curJob->date/100000)%10, (curJob->date/10000)%10);
    fprintf(MM, "%d%d", (curJob->date/1000)%10, (curJob->date/100)%10);
    fprintf(DD, "%d%d", (curJob->date/10)%10, curJob->date%10);
    if(curJob->startTime < 10) fprintf(starttime, "0%d:00", curJob->startTime);
    else fprintf(starttime, "%d:00", curJob->startTime);
    if(curJob->endTime < 10) fprintf(starttime, "0%d:00", curJob->endTime);
    else fprintf(endtime, "%d:00", curJob->endTime);
    if(curJob->ssType == 0) strcpy(type, "Class");
    else if(curJob->ssType == 1) strcpy(type, "Meeting");
    else if(curJob->ssType == 2) strcpy(type, "Gathering");
    strcpy(remark, curJob->remark.name);

    fprintf("%s-%s-%s   %s   %s   %s      %s\n", YY, MM, DD, starttime, endtime, type, remark);
}
