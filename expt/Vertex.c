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
#define NORMAL_LENGTH 5
#define N_CHILD 6 // How many scheduler

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
    extra *remark;
    struct Job *next;
    struct Job *acnext;
    struct Job *renext;
} Job;

/*Red Black Tree*/
typedef struct rbtnode{
    int color; //Red: 0 Black: 1
    char begin[15];
    char end[15];
    struct rbtnode *left;
    struct rbtnode *right;
    struct rbtnode *parent;
}rbtnode;

/*Left Rotate*/
/*
 *        px                       px
 *        /                        /
 *       x                        y
 *      / \       --left-->      / \
 *     lx  y                    x  ry
 *        / \                  / \
 *       lx ly                lx ly
 */

/* Prototype */
void readInput(char (*));
int splitString(char**, char (*));
int checkType(char*);
void debug_print(Job*);
void freeParticipantList(Extra*);

void parent_checkUserNum(int, char *[]);
void parent_write(char *, int [][2]);
void parent_handler(char (*), char *[], int *, int [][2]);
void parent_request(char *, char *[], int [][2]);
int parent_validate(char **, char *[], int);
int parent_checkUserExist(char **, char* [], int);
int parent_verifyUser(char *, char* []);
int parent_verifyParticipant(char **, char *[]);
int parent_checkDuplicate(Extra **, char *);
void parent_addBatch(char*, char*[], int [][2]);

void scheduler_base(int, int, char *[]);
void scheduler_initJob(Job *, char **, int, int);
void scheduler_selector(int, Job **, Job **, Job **, char **, int, int, int*, int*);
void scheduler_sample(Job **, char **, int, int);
void scheduler_fcfs(Job **, Job **, Job **, char **, int, int, int*, int*);
void scheduler_priority(Job **, Job **, Job **, char **, int, int, int*, int*);
void scheduler_special(Job **, Job **, Job **, char **, int, int, int*, int*);

void printSchd(int schedulerID, Job **head_ref, char **wList, int length, int t);
void printRepo_selector(int schedulerID, Job **acceptList, Job **rejectList, char **wList, int length, int t);
void printJob(Job *curJob, FILE *file);

void leftRotate(rbtnode *root, rbtnode *x);
void rightRotate(rbtnode *root, rbtnode *y);
int addable(rbtnode *root, char *begin, char *end);
char* minstr(char *a, char *b);
char* maxstr(char *a, char *b);
void conflict(rbtnode *node, char *begin, char *end, char *period);
void insert(rbtnode *root, char *begin, char *end);
void insert_fixup(rbtnode *root, rbtnode *node);
void freetree(rbtnode *node);
void grandchildProcess(int cToG[2], int gToC[2]);

/*-------Main-------*/
int main(int argc, char *argv[]) {
    int toChild[N_CHILD][2], toParent[N_CHILD][2];
    pid_t shut_down[N_CHILD];
    int pid, i, j;

    parent_checkUser(--argc, ++argv);
    // Pipes
    for(i = 0; i < N_CHILD; i++) {
        if(pipe(toChild[i]) < 0 || pipe(toParent[i]) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }
    }
    // Fork
    for(i = 0; i < N_CHILD; i++) {  //N_CHILD means the number of scheduler
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
            scheduler_base(i, toChild[i][0], toParent[i][1], argc, argv);

            close(toChild[i][0]);
            close(toParent[j][1]);
            exit(0);
        }
        else shut_down[i] = pid;
    }

    if(pid > 0) { /* parent */
        int loop = 1;
        char cmd[MAX_INPUT_SZ];
        for(i = 0; i < N_CHILD; i++) {
            close(toChild[i][0]);
            close(toParent[i][1]);
        }
        while(loop) {
            readInput(cmd);
            parent_handler(cmd, argv, &loop, toChild);
        }
        for(j = 0; j < N_CHILD; j++) {
            close(toChild[j][1]);
            close(toParent[j][0]);
        }
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

void initParticipant(Extra *newUser, char *userName) {
    if (newUser == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }
    strcpy(newUser->name, userName);
    newUser->next = NULL;
}

void addParticipant(Extra **head_ref, char *userName) {
    Extra *temp, *newUser = (Extra*)malloc(sizeof(Extra));

    initParticipant(newUser, userName);
    if(*head_ref == NULL) {
        *head_ref = newUser;
    } else {
        temp = *head_ref;
        while(temp->next != NULL){
            temp = temp->next;
        }
        temp->next = newUser;
    }
}

void freeParticipantList(Extra *head_ref) {
    Extra *temp;
    while (head_ref != NULL) {
        temp = head_ref;
        head_ref = head_ref->next;
        free(temp);
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

void parent_handler(char (*cmd), char *users[], int *loop, int toChild[][2], int toParent[][2]) {
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
    memset(str, 0, sizeof(str));
    char **wList = malloc(sizeof(char*) * 1);
    int t;

    strcpy(str, cmd);
    strtok(cmd, " ");
    splitString(wList, cmd);
    t = checkType(cmd);
    if(t>0 && t<4 && parent_validate(wList, users, t))
        parent_write(str,toChild);
}

int parent_validate(char **wList, char *users[], int t) {
    int tStart = atoi(wList[3]) / 100;
    int cStart = atoi(wList[3]) % 100;
    int length = -1;

    while(wList[++length] != NULL) {}
    if(length < NORMAL_LENGTH) printf("Error: Unvalid Argument Number\n");
    else if(atoi(wList[2]) < 20180401 || atoi(wList[2]) > 20180414) printf("Error: Unvalid Date\n");
    else if(cStart != 0 || (tStart < 8 || tStart > 17))             printf("Error: Unvalid Starting Time\n");
    else if (atoi(wList[4]) < 1 || (atoi(wList[4]) + tStart) > 18)  printf("Error: Unvalid Duration\n");
    else return parent_checkUserExist(wList, users, t);
    return 0;
}

int parent_checkUserExist(char **wList, char* users[], int t) {
    int ownerValid, usersValid = 1;
    /* check owner's and participants' names and avoid duplicate */
    ownerValid = parent_verifyUser(wList[1], users);
    if(ownerValid == 1 && t != 1)
        usersValid = parent_verifyParticipant(wList, users);
    else if(wList[NORMAL_LENGTH] != NULL)
        usersValid = 0;

    return (ownerValid && usersValid);
}

int parent_verifyUser(char* name, char* users[]) {
    int userLen = -1;
    while(users[++userLen] != NULL)
        if(strcmp(users[userLen], name) == 0) return 1;
    return 0;
}

int parent_verifyParticipant(char **wList, char* users[]) {
    int l = NORMAL_LENGTH -1;
    int res = 1;
    Extra *usersList = NULL;
    while(wList[++l] != NULL) {
        if(strcmp(wList[1], wList[l]) == 0
           || parent_verifyUser(wList[l], users) == 0
           || parent_checkDuplicate(&usersList, wList[l]) == 0)
            res = 0;
    }
    if(l <= NORMAL_LENGTH) res = 0; // no participant means useless
    freeParticipantList(usersList); // free the list
    return res;
}

int parent_checkDuplicate(Extra **head_ref, char *userName) {
    Extra *prev, *curr, *newUser = (Extra*)malloc(sizeof(Extra));

    initParticipant(newUser, userName);
    if(*head_ref == NULL) {
        *head_ref = newUser;
    } else {
        curr = *head_ref;
        while(curr != NULL){
            if(strcmp(curr->name, newUser->name) == 0) return 0;
            prev = curr;
            curr = curr->next;
        }
        prev->next = newUser;
    }
    return 1;
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
void scheduler_base(int schedulerID, int fromParent, int toParent, int users, char *userList[]) {
    int loop = 1;
    Job *jobList = NULL;

    while(loop) {
        char cmdBuf[MAX_INPUT_SZ] = "";
        char **wList = malloc(sizeof(char*) * 1);
        int t;

        read(fromParent, cmdBuf, MAX_INPUT_SZ);
        strtok(cmdBuf, " ");
        splitString(wList, cmdBuf);
        t = checkType(wList[0]);

        switch(t) {
            case 1: case 2: case 3:
                // TODO: add each scheduler (new function for caseHandler)
                scheduler_selector(schedulerID, &jobList, wList, t);
                break;
            case 5:
                printf("for a user's schedule\n");
                break;
            case 6:
                printf("for a report of all users' schedule\n");
                break;
            case 0:
                loop = 0; break;
            default:
                printf("Meh..child\n"); loop = 0;
                break;
        }
    }
    debug_print(jobList);
    free(jobList);
}

void scheduler_selector(int schedulerID, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t, int *cToG, int *gToC) {
    Job *newJob = (Job *)malloc(sizeof(Job));
    scheduler_initJob(newJob, wList, t);
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

void add_sample(Job *head, Job *node) {
}

void add_fcfs(Job *head, Job *node) {
    //add to the linked list based on the time order
    if(head == NULL) {
        head=node;
    }
    else{
        Job *last=NULL;
        Job *cur=head;
        while(cur != NULL && (node->date > cur->date) || (node->date == cur->date && node->startTime >= cur->startTime)) {
            last=cur;
            cur=cur->next;
        }
        if(last!=NULL){
            last->next=node;
        }
        node->cur=cur;
    }
}

void add_priority(Job *head, Job *node) {
    //add to the linked list based on the priority and the time order
    if(head == NULL) {
        head=node;
    }
    else{
        Job *last=NULL;
        Job *cur=head;
        while(cur != NULL && (node->date > cur->date) || (node->date == cur->date && node->startTime >= cur->startTime)) {
            last=cur;
            cur=cur->next;
        }
        if(last!=NULL){
            last->next=node;
        }
        node->cur=cur;
    }
}

void add_special(Job *head, Job *node) {

}

//print the schedule
void printSchd(int schedulerID, Job **head_ref, char *wList[], int length, int t, int N_User, char *Users[]){
    int cToG[N_User][2], gToC[N_User][2], pid;
    Job *acceptList = NULL;
    File *file;
    char pipewrite[NOR_INPUT_SZ], piperead[NOR_INPUT_SZ];
    char filename[MAX_INPUT_SZ], name[MAX_INPUT_SZ];
    char starttime[NOR_INPUT_SZ], endtime[NOR_INPUT_SZ];

    int Jobcount=0, Hour=0;

    int i;
    for (i=0; i<N_User; i++){
        if (pipe(cToG[i])<0 || pipe(gToC[i]) < 0){
            printf("create pipe error\n");
            exit(1);
        }
    }
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
    if(schedulerID==0) fprintf(file, "Algorithm used: Sample\n");
    if(schedulerID==1) fprintf(file, "Algorithm used: First come first served\n");
    if(schedulerID==2) fprintf(file, "Algorithm used: Priority\n");
    if(schedulerID==3) fprintf(file, "Algorithm used: Special\n");
    fprintf(file, "%d timeslots occupied.\n\n", Hour);
    fprintf(file, "Date         Start   End     Type         Remarks\n");
    fprintf(file, "=========================================================================\n");

    for (i=0; i<N_User;i++) {
        pid = fork();
        if (pid < 0) {
            printf("error");
            exit(1);
        } else if (pid == 0)  grandchildProcess(cToG[i], gToC[i]);
    }

    if(pid > 0){
        temp = *head_ref;
        while(temp != NULL){
            int check=0;
            char piperead[NOR_INPUT_SZ];
            if(temp->startTime < 10) sprintf(starttime, "%d0%d00", temp->date, temp->startTime);
            else sprintf(starttime, "%d%d00", temp->date, temp->startTime);
            if(temp->endTime < 10) sprintf(starttime, "%d0%d00", temp->date, temp->endTime);
            else sprintf(starttime, "%d%d00", temp->date, temp->endTime);
            sprintf(pipewrite, "%s %s",starttime, endtime);
            write(cToG[i][1], pipewrite, NOR_INPUT_SZ);

            for(i=0; i<N_User; i++){
                read(gToC[i], piperead, NOR_INPUT_SZ);
                if(strcmp(piperead, "N") == 0) check = 1;
            }

            if(check == 0)printJob(temp, file);
            temp = temp->next;
            if(temp == NULL){
                sprintf(pipewrite, "c");
                write(cToG[i][1], pipewrite, NOR_INPUT_SZ);
            }
        }
    }
    fprintf("   - END -\n=========================================================================\n");
    free(temp);
}

//print the report
void printRepo(int schedulerID, Job **jobList, Job **acceptList, Job **rejectList, char **wList, int length, int t, int N_User, char *Users[]){
    int cToG[N_User][2], gToC[N_User][2], pid;
    File *file;
    Job *acceptList = NULL, *rejectList = NULL;
    char pipewrite[NOR_INPUT_SZ], piperead[NOR_INPUT_SZ];
    char filename[MAX_INPUT_SZ];
    char starttime[NOR_INPUT_SZ], endtime[NOR_INPUT_SZ];
    int acJobcount=0, reJobcount=0, acJobhour=0, reJobhour=0, i;

    for(i=0; i<N_User; i++){
        if (pipe(cToG[i])<0 || pipe(gToC[i]) < 0){
            printf("create pipe error\n");
            exit(1);
        }
    }

    for (i=0; i<N_User;i++) {
        pid = fork();
        if (pid < 0) {
            printf("error");
            exit(1);
        } else if (pid == 0)  grandchildProcess(cToG[i], gToC[i]);
    }

    if(pid > 0){
        Job *temp = (Job *)malloc(sizeof(Job));
        temp = *head_ref;
        while(temp != NULL){
            int check=0;
            char piperead[NOR_INPUT_SZ];
            if(temp->startTime < 10) sprintf(starttime, "%d0%d00", temp->date, temp->startTime);
            else sprintf(starttime, "%d%d00", temp->date, temp->startTime);
            if(temp->endTime < 10) sprintf(starttime, "%d0%d00", temp->date, temp->endTime);
            else sprintf(starttime, "%d%d00", temp->date, temp->endTime);
            sprintf(pipewrite, "%s %s",starttime, endtime);
            write(cToG[i][1], pipewrite, NOR_INPUT_SZ);

            for(i=0; i<N_User; i++){
                read(gToC[i], piperead, NOR_INPUT_SZ);
                if(strcmp(piperead, "N") == 0) check = 1;
            }

            if(check == 0){
                if(acceptList == NULL)acceptList = temp;
                else{
                    Job *curJob = (Job *)malloc(sizeof(Job));
                    curJob = acceptList;
                    while (curJob->acnext != NULL) curJob=curJob->acnext;
                    curJob->acnext = temp;
                }
                acJobcount += 1;
                acJobhour += temp->endTime - temp->startTime;
            }
            else if(check == 1){
                if(rejectList == NULL)rejectList = temp;
                else{
                    Job *curJob = (Job *)malloc(sizeof(Job));
                    curJob = rejectList;
                    while (curJob->renext != NULL) curJob=curJob->renext;
                    curJob->renext = temp;
                }
                reJobcount += 1;
                reJobhour += temp->endTime - temp->startTime;
            }
            temp = temp->next;
            if(temp == NULL){
                sprintf(pipewrite, "c");
                write(cToG[i][1], pipewrite, NOR_INPUT_SZ);
            }
        }
    }
    strcpy(filename, wList[1]);
    file = fopen(filename, "w");
    fprintf(file, "Personal Organizer\n***Schedule Report***\n");
    if(schedulerID==0) fprintf(file, "Algorithm used: Sample\n");
    if(schedulerID==1) fprintf(file, "Algorithm used: First come first served\n");
    if(schedulerID==2) fprintf(file, "Algorithm used: Priority\n");
    if(schedulerID==3) fprintf(file, "Algorithm used: Special\n");
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
    fprintf(file, "Total number of request:     %d\n", acJobhour+reJobhour);
    fprintf(file, "Timeslot in use:             %d hours\n", acJobhour);
    fprintf(file, "Timeslot not in use:         %d\n", rehour);
    fprintf(file, "hours Utilization:                 %d %%\n", achour*1000(acJobhour+reJobhour));
    fprintf("   - END -\n");
    free(temp);
}

//print the job
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


/* Grandchild process */
void leftRotate(rbtnode *root, rbtnode *x)
{
    rbtnode *y=x->right;
    x->right=y->left;
    if(y->left!=NULL) y->left->parent=x;
    y->parent=x->parent;
    if(x->parent==NULL){
        root=y;
    }
    else{
        if(x->parent->left==x) x->parent->left=y;
        else x->parent->right=y;
    }
    y->left=x;
    x->parent=y;
}

/*Right Rotate*/
/*
 *       py                     py
 *       /                      /
 *      y                      x
 *     / \     --right-->     / \
 *    x  ry                  lx  y
 *   / \                        / \
 *  lx rx                      rx ry
 */
void rightRotate(rbtnode *root, rbtnode *y){
    rbtnode *x=y->left;
    y->left=x->right;
    if(x->right!=NULL) x->right->parent=y;
    x->parent=y->parent;
    if(y->parent==NULL){
        root=x;
    }
    else{
        if(y==y->parent->right) y->parent->right=x;
        else y->parent->left=x;
    }
    x->right=y;
    y->parent=x;
}

/*Check whether the time can be added*/
int addable(rbtnode *root, char *begin, char *end)
{
    rbtnode *cur=root;
    while(cur!=NULL){
        if(strcmp(end,cur->begin)<=0){
            cur=cur->left;
        }
        else if(strcmp(cur->end,begin)<=0){
            cur=cur->right;
        }
        else return 0;
    }
    return 1;
}

char* minstr(char *a, char *b)
{
    static char *ans;
    if(strcmp(a,b)<0) strcpy(ans,a);
    else strcpy(ans,b);
    return ans;
}

char* maxstr(char *a, char *b)
{
    static char *ans;
    if(strcmp(a,b)>0) strcpy(ans,a);
    else strcpy(ans,b);
    return ans;
}

void conflict(rbtnode *node, char *begin, char *end, char *period)
{
    if(strcmp(begin,end)<=0) return ;
    rbtnode *cur=node;
    while(cur!=NULL){
        if(strcmp(end,cur->begin)<=0){
            cur=cur->left;
        }
        else if(strcmp(cur->end,begin)<=0){
            cur=cur->right;
        }
        else{
            strcat(period, maxstr(begin,cur->begin));
            strcat(period, " ");
            strcat(period, minstr(end,cur->end));
            strcat(period, " ");
            conflict(cur, begin, maxstr(begin, cur->begin), period);
            conflict(cur, minstr(end, cur->end), end, period);
            return ;
        }
    }
}

void insert(rbtnode *root, char *begin, char *end)
{
    rbtnode *node=(rbtnode*)malloc(sizeof(rbtnode));
    strcpy(node->begin,begin);
    strcpy(node->end,end);
    node->left=NULL;
    node->right=NULL;
    rbtnode *y=NULL;
    rbtnode *x=root;

    while(x!=NULL){
        y=x;
        if(strcmp(node->end,x->begin)<=0) x=x->left;
        else x=x->right;
    }
    node->parent=y;
    if(y!=NULL){
        if(strcmp(node->end,y->begin)<=0) y->left=node;
        else y->right=node;
    }
    else root=node;
    node->color=0;
    insert_fixup(root,node);
}

void insert_fixup(rbtnode *root, rbtnode *node)
{
    rbtnode *parent,*gparent;
    while((parent=node->parent) && (parent->color==0)){
        gparent=parent->parent;
        if(parent==gparent->left){
            rbtnode *uncle=gparent->right;
            if(uncle && uncle->color==0){
                uncle->color=1;
                parent->color=1;
                gparent->color=0;
                node=gparent;
                continue;
            }
            if(parent->right==node){
                rbtnode *temp;
                leftRotate(root,parent);
                temp=parent;
                parent=node;
                node=temp;
            }
            parent->color=1;
            gparent->color=0;
            rightRotate(root,gparent);
        }
        else{
            rbtnode *uncle=gparent->left;
            if(uncle && uncle->color==0){
                uncle->color=1;
                parent->color=1;
                gparent->color=0;
                node=gparent;
                continue;
            }
            if(parent->left==node){
                rbtnode *temp;
                rightRotate(root,parent);
                temp=parent;
                parent=node;
                node=temp;
            }
            parent->color=1;
            gparent->color=0;
            leftRotate(root,gparent);
        }
    }
    root->color=1;
}

void freetree(rbtnode *node)
{
    if(node==NULL) return;
    freetree(node->left);
    freetree(node->right);
    free(node);
}

/*---Grandchild---*/
void grandchildProcess(int cToG[2], int gToC[2])
{
    rbtnode *root=NULL;
    close(cToG[1]);
    close(gToC[0]);
    while(1){
        char se[80];
        memset(se,0,sizeof(se));
        n = read(cToG[0], se, 80);
        se[n] = 0;
        if(se[0] == 'c'){
            // close the process
            freetree(root);
            close(cToG[0]);
            close(gToC[0]);
            exit(0);
        }
        char *seq = strtok(se, " "), begin[80], end[80];
        strcpy(begin, seq[0]);
        strcpy(end, seq[1]);
        if(addable(root,begin,end)){
            write(gToC[1],"Y",3); //I have spare time to join
        }
        else{
            char period[80];
            memset(period,0,sizeof(period));
            conflict(root, begin, end, period);
            char con[90];
            sprintf(con,"N %s",period);
            write(gToC[1],con,strlen(con)); //I do not have time
        }
        char buf[80];
        read(cToG[0],buf,80);
        if(buf[0]=='Y'){
            //add it to my time schedule
            insert(root,begin,end);
        }
    }
}