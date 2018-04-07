#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define BUF_SZ 256
#define TIME_SZ 50
#define NORMAL_LENGTH 5
#define CLASSES 1
#define MEETING 2
#define GATHERING 3
#define N_CHILD 3 	// How many scheduler

typedef struct Extra {
    char name[20];
    struct Extra *next;
} Extra;

typedef struct Job {
    int ssType;
    char owner[20];
    int date; 		// YYYYMMDD
    int startTime; 	// hr only
    int endTime; 	// hr only
    Extra *remark; 	// NULL by default
    struct Job *next;
} Job;

typedef struct rbtnode{
    int color; 		//Red: 0 Black: 1
    char begin[15];
    char end[15];
    struct rbtnode *left;
    struct rbtnode *right;
    struct rbtnode *parent;
} rbtnode;

/* Prototype */
void debug_print        (Job*);
void readInput          (char(*));
int splitString         (char**, char (*));
int checkType           (char*);
void freeParticipantList(Extra*);
int findUser            (char*, int, char*[]);
char* dateToString      (int, int);
void addToList          (Job**, Job*);
void copyJob            (Job*, Job*);
int countTotalTime      (Job*);
void addParticipant     (Extra**, char*);
Job *getTail            (Job *);
Job *partition          (Job *, Job *, Job **, Job **);
Job *quickSortRecur     (Job *, Job *);
void quickSort          (Job **);

void parent_checkUserNum    (int, char*[]);
void parent_write           (char*, int[][2]);
void parent_handler         (char(*), char*[], int*, int[][2], int[][2]);
void parent_request         (char*, char*[], int[][2]);
int parent_validate         (char**, char*[], int);
int parent_checkUserExist   (char**, char*[], int);
int parent_verifyUser       (char*, char*[]);
int parent_verifyParticipant(char**, char*[]);
int parent_checkDuplicate   (Extra **head_ref, char *userName);
void parent_addBatch        (char*, char*[], int[][2]);
void parent_askSchd         (char*, char*[], int[][2]);

void scheduler_base         (int, int, int, int, char*[]);
void scheduler_initJob      (Job*, char**, int);
void scheduler_selector     (int, Job**, char**, int);
void scheduler_fcfs         (Job**, char**, int);
void scheduler_priority     (Job**, char**, int);
void scheduler_special      (Job**, char**, int);
void scheduler_print        (Job*, Job**, Job**, int, char*[], int);
void scheduler_exct         (int, int, Job*, char*[], char**, int);

void printer_userSchedule   (int, char *, char *, Job *);
void printer_report         (int, char *, int, Job *, Job *, int);
void printer_outputJob      (Job *, FILE *, char*);

rbtnode* leftRotate         (rbtnode *, rbtnode *);
rbtnode* rightRotate        (rbtnode *, rbtnode *);
int addable                 (rbtnode *, char *, char *);
char* minstr                (char *, char *);
char* maxstr                (char *, char *);
void conflict               (rbtnode *, char *, char *, char *);
rbtnode* insert             (rbtnode *, char *, char *);
rbtnode* insert_fixup       (rbtnode *, rbtnode *);
void freetree               (rbtnode *);
void grandchildProcess      (int, int[], int[]);

void deleteNode(Job **, Job *);
void reschd(Job **, Job **, int[][2], int[][2],int, char *[]);

/*---------------------------*/
/*-------Main-------*/
int main(int argc, char *argv[]) {
    int toChild[N_CHILD][2], toParent[N_CHILD][2];
    pid_t shut_down[N_CHILD];
    int pid, i, j;

    parent_checkUserNum(--argc, ++argv);
    for(i = 0; i < N_CHILD; i++) { // Pipes
        if(pipe(toChild[i]) < 0 || pipe(toParent[i]) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }
    }
    for(i = 0; i < N_CHILD; i++) { // Fork
        pid = fork();
        if(pid < 0) {
            printf("Error");
            exit(1);
        } 
        else if (pid == 0) { /* child */
            close(toChild[i][1]);
			close(toParent[i][0]);

            scheduler_base(i, toChild[i][0], toParent[i][1], argc, argv);
			
            close(toChild[i][0]);
            close(toParent[i][1]);
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
            parent_handler(cmd, argv, &loop, toChild, toParent);
        }
        for(j = 0; j < N_CHILD; j++) { 
			close(toChild[j][1]);
			close(toParent[j][0]);
		}
    }
    /* wait for all child */
    for(i = 0; i < N_CHILD; i++)
        waitpid(shut_down[i], NULL, 0);
    printf("-> Bye!\n");
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
        if(wList == NULL) exit(1);
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
	Job *cur = head;
    while(cur != NULL) {
        printf("%d %s %d %d %d ", cur->ssType, cur->owner, cur->date, cur->startTime, cur->endTime);
		Extra *tex = cur->remark;
		while(tex != NULL) {
			printf("%s ", tex->name);
			tex = tex->next; 
		}
		printf("\n");
		cur = cur->next;
    }
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

void freeJob(Job* node) {
	Job *temp;
	while(node != NULL){
		temp = node;
		node = node->next;
		freeParticipantList(temp->remark);
		free(temp);
	}
}

char* dateToString(int date, int time) {
    static char str[BUF_SZ];
    memset(str,0,sizeof(str));
    if(time<10) sprintf(str,"%d0%d00",date,time);
    else sprintf(str,"%d%d00",date,time);
    return str;
}

void addToList(Job **head, Job *node) {
    if(*head == NULL) {
        *head=node;
    } else {
        Job *cur=*head;
        while(cur->next != NULL)
            cur=cur->next;
        cur->next=node;
    }
}

int checkSchd(char *schd) {
    if      (strcmp(schd, "fcfs") == 0)     return 0;
    else if (strcmp(schd, "pr") == 0)       return 1;
    else if (strcmp(schd, "special") == 0)  return 2;
    return -1;
}

char *schdName(int schedulerID) {
    switch(schedulerID) {
        case 0: return "First Come First Serve"; break;
        case 1: return "Priority";               break;
        case 2: return "Special";                break;
        default: return NULL;
    }
}

int findUser(char *name, int users, char *userList[]) {
    int i;
    for(i=0;i<users;i++){
        if(strcmp(userList[i],name)==0)
            return i;
    }
    return -1;
}

void copyJob(Job *dest, Job *src) {
	dest->ssType=src->ssType;
	strcpy(dest->owner, src->owner);
	dest->date=src->date;
	dest->startTime=src->startTime;
	dest->endTime=src->endTime;
	dest->remark=NULL;
	if(src->remark!=NULL){
		Extra *cur = src->remark;
		while(cur != NULL) {
			addParticipant(&(dest->remark), cur->name);
			cur = cur->next;
		}
	}
	dest->next=NULL;
}

int countTotalTime(Job *event) {
    int eTime = event->endTime - event->startTime;
    int res = eTime;
    Extra *temp = event->remark;
    while(temp != NULL) {
        res = res + eTime;
        temp = temp->next;
    }
    return res;
}

Job *getTail(Job *cur) {
    while (cur != NULL && cur->next != NULL)
        cur = cur->next;
    return cur;
}

Job *partition(Job *head, Job *end, Job **newHead, Job **newEnd) {
    Job *pivot = end;
    Job *prev = NULL, *cur = head, *tail = pivot;
    while (cur != pivot) {
        if (cur->date < pivot->date || 
        (cur->date == pivot->date && cur->startTime < pivot->startTime) || 
        (cur->date == pivot->date && cur->startTime == pivot->startTime && cur->endTime < pivot->endTime)) {
            if ((*newHead) == NULL)
                (*newHead) = cur;
            prev = cur;  
            cur = cur->next;
        } else {
            if (prev) prev->next = cur->next;
            Job *tmp = cur->next;
            cur->next = NULL;
            tail->next = cur;
            tail = cur;
            cur = tmp;
        }
    }
    if ((*newHead) == NULL)
        (*newHead) = pivot;
    (*newEnd) = tail;
    return pivot;
}
 
Job *quickSortRecur(Job *head, Job *end) {
    if (!head || head == end) return head;
    Job *newHead = NULL, *newEnd = NULL;
    Job *pivot = partition(head, end, &newHead, &newEnd);
    if (newHead != pivot) {
        Job *tmp = newHead;
        while (tmp->next != pivot)
            tmp = tmp->next;
        tmp->next = NULL;
        newHead = quickSortRecur(newHead, tmp);
        tmp = getTail(newHead);
        tmp->next =  pivot;
    }
    pivot->next = quickSortRecur(pivot->next, newEnd);
    return newHead;
}
 
void quickSort(Job **headRef) {
    (*headRef) = quickSortRecur(*headRef, getTail(*headRef));
    return;
}

/*-------Parent_part-------*/
void parent_checkUserNum(int num, char *users[]) {
    if(num < 3 || num > 9) {
        printf("E: Range should be 3~9 users");
        exit(1);
    }
}

void parent_write(char *str, int toChild[][2]) {
    int i;
    for(i = 0; i < N_CHILD; i++)
        write(toChild[i][1], str, MAX_INPUT_SZ);
}

void parent_repo(char *cmd, int toChild[][2], int toParent[][2]) {
	int i;
	char temp[BUF_SZ];
	char str[MAX_INPUT_SZ];
	memset(str,0,sizeof(str));
	strcpy(str, cmd);
    strtok(cmd," ");
	cmd = strtok(NULL, " ");
	if(cmd == NULL) {
		printf("Error: Invalid Argument Number\n");
		return;
	}
	
	for(i=0;i<N_CHILD;i++){
        write(toChild[i][1], str, MAX_INPUT_SZ);
		read(toParent[i][0], temp, MAX_INPUT_SZ);
	}
}

void parent_handler(char (*cmd), char *users[], int *loop, int toChild[][2], int toParent[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str, 0, sizeof(str));
    int t;
	
    strcpy(str, cmd);
    strtok(cmd," ");
    t = checkType(cmd);
    switch(t) {
        case 1: case 2: case 3:
            parent_request(str, users, toChild);  break;
        case 4:
            parent_addBatch(str, users, toChild); break;
        case 5:
            parent_askSchd(str, users, toChild);        break;
        case 6:
            parent_repo(str, toChild, toParent);        break;
        case 0: 
            parent_write(str, toChild); *loop = 0;      break;
        default:
            printf("Unrecognized command.\n");          break;
    }
}

void parent_askSchd(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str,0,sizeof(str));
    char **wList = malloc(sizeof(char*) * 1);
    int t, m, l;

    strcpy(str, cmd);
    strtok(cmd, " ");
    l = splitString(wList, cmd);
	if(l < 4){
		printf("Error: Invalid Argument Number\n");
        free(wList[l]);
		return; 
	}
    m = parent_verifyUser(wList[1], users);
    t = checkSchd(wList[2]);
    if(m > 0 && t >= 0)
        write(toChild[t][1], str, MAX_INPUT_SZ);
    free(wList[l]);
}

void parent_request(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str,0,sizeof(str));
    char **wList = malloc(sizeof(char*) * 1);
    int t, l;

    strcpy(str, cmd);
    strtok(cmd, " ");
    l = splitString(wList, cmd);
    t = checkType(cmd);
	if(l < NORMAL_LENGTH) {
		printf("Error: Invalid Argument Number\n");
        free(wList[l]);
		return;
	}
    else if(parent_validate(wList, users, t)) 
        parent_write(str,toChild);
    free(wList[l]);
}

int parent_validate(char **wList, char *users[], int t) {
    int tStart = atoi(wList[3]) / 100;
    int cStart = atoi(wList[3]) % 100;
	
    if(atoi(wList[2]) < 20180401 || atoi(wList[2]) > 20180414)      printf("Error: Invalid Date\n");
	else if(cStart != 0 || (tStart < 8 || tStart > 17))             printf("Error: Invalid Starting Time\n");
    else if (atoi(wList[4]) < 1 || (atoi(wList[4]) + tStart) > 18)  printf("Error: Invalid Duration\n");
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
        if (strlen(line) > 0 && (line[strlen(line) - 1] == '\n')) // LF only
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
        char cmdBuf[MAX_INPUT_SZ];
		memset(cmdBuf, 0, sizeof(cmdBuf));
        char **wList = malloc(sizeof(char*) * 1);
        int t, l;

        read(fromParent, cmdBuf, MAX_INPUT_SZ);
        strtok(cmdBuf, " ");
        l = splitString(wList, cmdBuf);
        t = checkType(wList[0]);
		switch(t) {
            case 1: case 2: case 3:
                scheduler_selector(schedulerID, &jobList, wList, t);
                break;
            case 5: case 6:
                scheduler_exct(schedulerID, users, jobList, userList, wList, toParent);
                break;
            case 0:
                loop = 0; break;
            default:
                printf("Child: Unknown input\n"); // loop = 0;
                break;
        }
        free(wList[l]);
    }
    freeJob(jobList);
}

void scheduler_selector(int schedulerID, Job **head_ref, char **wList, int t) {
    switch(schedulerID) {
        case 0: scheduler_fcfs(head_ref, wList, t); break;
        case 1: scheduler_priority(head_ref, wList, t); break;
        case 2: scheduler_special(head_ref, wList, t); break;
        default: break;
    }
}

void scheduler_initJob(Job *newJob, char **wList, int t) {
    int l = NORMAL_LENGTH -1;
    if (newJob == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }
    
    newJob->ssType = t;
    strcpy(newJob->owner, wList[1]);
    newJob->date = atoi(wList[2]);
    newJob->startTime = atoi(wList[3]) / 100;
    newJob->endTime = newJob->startTime + atoi(wList[4]);
    newJob->remark = NULL;
    while(wList[++l] != NULL) {
        addParticipant(&(newJob->remark), wList[l]);
    }
    newJob->next = NULL;
}

void scheduler_fcfs(Job **head_ref, char **wList, int t) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));
    
    scheduler_initJob(newJob, wList, t);
    if(*head_ref == NULL) {
        *head_ref = newJob;
    } else {
		if((*head_ref)->date > newJob->date ||
        ((*head_ref)->date == newJob->date && (*head_ref)->startTime > newJob->startTime)) {
			newJob->next = *head_ref;
			*head_ref = newJob;
		} else {
			temp = *head_ref;
			while(temp->next != NULL && 
            ((temp->next->date < newJob->date) || 
            (temp->next->date == newJob->date && temp->next->startTime <= newJob->startTime)) ){
				temp = temp->next;
			}
			newJob->next = temp->next;
			temp->next = newJob;
		}
	}
}

void scheduler_priority(Job **head_ref, char **wList, int t) {
	Job *temp, *newJob = (Job*)malloc(sizeof(Job));
    
    scheduler_initJob(newJob, wList, t); 
    if(*head_ref == NULL || (*head_ref)->ssType > newJob->ssType) {
        newJob->next = *head_ref;
        *head_ref = newJob;
    } else {
        temp = *head_ref;
        while(temp->next != NULL && temp->next->ssType <= newJob->ssType){
            temp = temp->next;
        }
        newJob->next = temp->next;
        temp->next = newJob;
    }
}

void scheduler_special(Job **head_ref, char **wList, int t) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));
    int tNew;
    
    scheduler_initJob(newJob, wList, t);
    if(*head_ref == NULL) 
        *head_ref = newJob;
    else {
        tNew = countTotalTime(newJob);
        if(countTotalTime(*head_ref) < tNew) {
            newJob->next = *head_ref;
            *head_ref = newJob;
        } else {
            temp = *head_ref;
            while(temp->next != NULL && 
            countTotalTime(*head_ref) >= tNew){
                temp = temp->next;
            }
            newJob->next = temp->next;
            temp->next = newJob;
        }
    }
}

void scheduler_exct(int schedulerID, int users, Job *jobList, char *userList[], char **wList, int toParent) {
    Job *acceptList=NULL;
	Job *rejectList=NULL;
	int t = checkType(wList[0]);
	int i=0, flag=1;
    while(wList[++i]!=NULL){
		if(strcmp(wList[i], "reschedule")==0){
			scheduler_print(jobList, &acceptList, &rejectList, users, userList, 1); // Reschedule
			flag = 0;
			break;
		}
	}
    if(flag) scheduler_print(jobList, &acceptList, &rejectList, users, userList, 0); 
	
    quickSort(&acceptList);
    quickSort(&rejectList);
	
	if(t == 5)  printer_userSchedule(schedulerID, wList[1], wList[3], acceptList);
	else        printer_report(schedulerID, wList[1], toParent, acceptList, rejectList, users);
	freeJob(acceptList);
	freeJob(rejectList);
}

void scheduler_print(Job *jobList, Job **acceptList, Job **rejectList, int users, char *userList[], int rescheduler) {
	int i, j;
	int cToG[users][2],gToC[users][2];
	/* grandChild process */
	for(i=0;i<users;i++){
		if(pipe(cToG[i])<0 || pipe(gToC[i])<0) {
			printf("Create pipe error...\n");
			exit(1);
		}
		int pid=fork();
		if(pid==0){
			grandchildProcess(i, cToG[i], gToC[i]);
		}
	}
	
	//child to grandchild process
	for(i=0;i<users;i++){
		close(cToG[i][0]);
		close(gToC[i][1]);
	}
	
	//get acceptList and rejectList
	Job *cur=jobList;
	while(cur!=NULL){
		char se[BUF_SZ], buf[1000], start[BUF_SZ], end[BUF_SZ];
		memset(se,0,sizeof(se));
		strcpy(start, dateToString(cur->date, cur->startTime));
		strcpy(end, dateToString(cur->date, cur->endTime));
		sprintf(se,"%s %s",start,end);
		
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		int n=1;
		Extra *now=cur->remark;
		while(now != NULL){
			userId[n++]=findUser(now->name, users, userList);
			now=now->next;
		}
		for(i=0;i<n;i++){
			write(cToG[userId[i]][1], se, BUF_SZ);
			memset(buf, 0, sizeof(buf));
			read(gToC[userId[i]][0], buf, BUF_SZ);
			if(buf[0]=='Y'){
				if(i == n-1) { // The job can be accepted
					for(j=0;j<n;j++){
						write(cToG[userId[j]][1], "Y", BUF_SZ);
					}
					Job* node = (Job*)malloc(sizeof(Job)*1);
					copyJob(node, cur);
					addToList(acceptList, node);
				}
			}
			else { // The job cannot be accepted
				for(j=0;j<=i;j++){
					write(cToG[userId[j]][1], "N", BUF_SZ);
				}
				Job *node = (Job*)malloc(sizeof(Job)*1);
				copyJob(node, cur);
				addToList(rejectList, node);
				break;
			}
		}
		cur=cur->next;
	}
	
	if(rescheduler){
		reschd(rejectList, acceptList, cToG, gToC, users, userList);
	}	
	//close grandchild process
	for(i=0;i<users;i++){
		write(cToG[i][1], "c", BUF_SZ);
	}
	
	for(i=0; i<users; i++) wait(NULL);
	
	for(i=0;i<users;i++){
		close(cToG[i][1]);
		close(gToC[i][0]);
	}
}

/* Printer part */
void printer_userSchedule(int schedulerID, char *userName, char *fileName, Job *acceptList) {
    FILE *f;
    int slotUsed = 0, eventCount = 0, isExist = 0;
    char *schedulerName;
    Job *temp;

    f = fopen(fileName, "w");
    if (f == NULL) {
        printf("Error opening file\n"); return;
    }
    schedulerName = schdName(schedulerID);
    temp = acceptList;
    while(temp != NULL) {
		isExist = 0;
        if(strcmp(temp->owner, userName) == 0) isExist = 1;
        else {
            Extra *tex = temp->remark;
            while(tex != NULL) {
                if(strcmp(tex->name,userName) == 0) {
                    isExist = 1; break;
                }
                tex = tex->next;
            }
        }
        if(isExist) {
            eventCount++;
            slotUsed = temp->endTime - temp->startTime + slotUsed;
        }
        temp = temp->next;
    }
    fprintf(f, "Personal Organizer\n***Appointment Schedule***\n\n"
        "%s, you have %d appointments\n"
        "Algorithm used: %s\n"
        "%d timeslots occupied.\n\n"
        "   Date      Start    End     Type      Remarks\n"
        "=========================================================\n"
    ,userName, eventCount, schedulerName, slotUsed);

    temp = acceptList;
    while(temp != NULL) {
		isExist = 0;
        if(strcmp(temp->owner, userName) == 0) isExist = 1;
        else {
            Extra *tex = temp->remark;
            while(tex != NULL) {
                if(strcmp(tex->name,userName) == 0) {
                    isExist = 1; break;
                }
                tex = tex->next; 
            }
        } 
        if(isExist) printer_outputJob(temp, f, userName); 
        temp = temp->next;
    }
    fprintf(f, "\n=========================================================\n"
        "\n\t- End -\n");
    fclose(f);
}

void printer_report(int schedulerID, char *fileName, int toParent, Job *acceptList, Job *rejectList, int num) {
    FILE *f;
    int acceptCount = 0, rejectCount = 0;
    int slotUsed = 0, maxSlot = num * 14 * 10;
    char *schedulerName;
    Job *temp = NULL;
	
    f = fopen(fileName, "a");
    if (f == NULL) {
        printf("Error opening file\n"); return;
    }
    schedulerName = schdName(schedulerID);
    temp = acceptList;
    while(temp != NULL) {
        acceptCount++;
        temp = temp->next;
    }
    temp = rejectList;
    while(temp != NULL) {
        rejectCount++;
        temp = temp->next;
    }
    fprintf(f, "Personal Organizer\n***Schedule Report***\n\n"
        "Algorithm used: %s\n"
        "***Accept List***\n"
        "There are %d requests accepted.\n"
        "   Date      Start    End     Type      Remarks\n"
        "=========================================================\n"
    , schedulerName, acceptCount);
    temp = acceptList;
    while(temp != NULL) {
        Extra *tex = temp->remark;
        int thour = temp->endTime - temp->startTime;
        slotUsed += thour; // owner time
        while(tex != NULL) {
			slotUsed += thour; // participant time
			tex = tex->next;
		}
        printer_outputJob(temp, f, NULL);
        temp = temp->next;
    }
    fprintf(f, "\n"
        "=========================================================\n\n"
        "***Reject List***\n"
        "There are %d requests rejected.\n"
        "   Date      Start    End     Type      Remarks\n"
        "=========================================================\n"
    , rejectCount);
    temp = rejectList;
    while(temp != NULL) {
        printer_outputJob(temp, f, NULL);
        temp = temp->next;
    }
    fprintf(f, "\n"
        "=========================================================\n"
        "Total number of request:    %d\n"
        "Timeslot in use:            %d\n"
        "Timeslot not in use:        %d\n"
        "Utilization:                %d%%\n"
        "\n\t- End -\n\n\n"
    , acceptCount + rejectCount,slotUsed, maxSlot-slotUsed, (slotUsed*100)/maxSlot);
    fclose(f);
    write(toParent, "Finished", BUF_SZ); // fin
}

void printer_outputJob(Job *curr, FILE *f, char *userName) {
    char YY[5], MM[3], DD[3], type[20];
    char start[6], end[6], remark[MAX_INPUT_SZ] = "";
    Extra *tex = curr->remark;

    sprintf(YY, "%d%d%d%d", curr->date/10000000, (curr->date/1000000)%10,
             (curr->date/100000)%10, (curr->date/10000)%10);
    sprintf(MM, "%d%d", (curr->date/1000)%10, (curr->date/100)%10);
    sprintf(DD, "%d%d", (curr->date/10)%10, curr->date%10);

    if(curr->startTime < 10) sprintf(start, "0%d:00", curr->startTime);
    else sprintf(start, "%d:00", curr->startTime);
    if(curr->endTime < 10) sprintf(end, "0%d:00", curr->endTime);
    else sprintf(end, "%d:00", curr->endTime);
    switch(curr->ssType) {
        case CLASSES:   strcpy(type, "Class    ");      break;
        case MEETING:   strcpy(type, "Meeting  ");      break;
        case GATHERING: strcpy(type, "Gathering");      break;
    }
    if(userName == NULL) { // Report
        sprintf(remark, "%s", curr->owner);
        while(tex != NULL) {
            sprintf(remark, "%s %s", remark, tex->name);
            tex = tex->next;
        }
    } else if(strcmp(userName, curr->owner) == 0) { // Owner
        if(tex == NULL) strcpy(remark, "-");
        else {
            sprintf(remark, "%s", tex->name);
            tex = tex->next;
            while(tex != NULL) {
                sprintf(remark, "%s %s", remark, tex->name);
                tex = tex->next;
            }  
        }
    } else { // Participant
        sprintf(remark, "%s", curr->owner);
        while(tex != NULL) {
            if(strcmp(userName, tex->name) != 0)
                sprintf(remark, "%s %s", remark, tex->name);
            tex = tex->next;
        }  
    }
    fprintf(f, "%s-%s-%s   %s   %s   %s\t%s\n", YY, MM, DD, start, end, type, remark);
}

/*-------------Grandchild process-------------*/
rbtnode* leftRotate(rbtnode *root, rbtnode *x) {
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
	return root;
}

rbtnode* rightRotate(rbtnode *root, rbtnode *y) {
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
	return root;
}

/*Check whether the time can be added*/
int addable(rbtnode *root, char *begin, char *end) {
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

char* minstr(char *a, char *b) {
    static char *ans;
    if(strcmp(a,b)<0) strcpy(ans,a);
    else strcpy(ans,b);
    return ans;
}

char* maxstr(char *a, char *b) {
    static char *ans;
    if(strcmp(a,b)>0) strcpy(ans,a);
    else strcpy(ans,b);
    return ans;
}

void conflict(rbtnode *node, char *begin, char *end, char *period) {
	if(node==NULL) return ;
    rbtnode *cur=node;
	if(strcmp(end, cur->begin)<=0){
		conflict(cur->left, begin, end, period);
	}
	else if(strcmp(cur->end, begin)<=0){
		conflict(cur->right, begin, end, period);
	}
	else{
		conflict(cur->left, begin, end, period);
		if(strlen(period)==0){
			strcat(period, cur->begin);
			strcat(period, " ");
			strcat(period, cur->end);
			strcat(period, " ");
		}
		else{
			//check whether the endtime is equal to start time
			int len=strlen(period);
			int i;
			int flag=1;
			for(i=0;i<12;i++){
				if(period[len-13+i]!=(cur->begin)[i]){
					flag=0;
					break;
				}
			}
			if(flag){
				for(i=0;i<12;i++) period[len-13+i]=(cur->end)[i];
			}
			else{
				strcat(period, cur->begin);
				strcat(period, " ");
				strcat(period, cur->end);
				strcat(period, " ");
			}
		}
		conflict(cur->right, begin, end, period);
	}
}

rbtnode* insert(rbtnode *root, char *begin, char *end) {
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
    root=insert_fixup(root,node);
	return root;
}

rbtnode* insert_fixup(rbtnode *root, rbtnode *node) {
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
                root=leftRotate(root,parent);
                temp=parent;
                parent=node;
                node=temp;
            }
            parent->color=1;
            gparent->color=0;
            root=rightRotate(root,gparent);
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
                root=rightRotate(root,parent);
                temp=parent;
                parent=node;
                node=temp;
            }
            parent->color=1;
            gparent->color=0;
            root=leftRotate(root,gparent);
        }
    }
    root->color=1;
	return root;
}

void freetree(rbtnode *node) {
    if(node==NULL) return;
    freetree(node->left);
    freetree(node->right);
    free(node);
}

void grandchildProcess(int a, int cToG[2], int gToC[2]) {
    rbtnode *root=NULL;
    close(cToG[1]);
    close(gToC[0]);
    while(1){
        char se[BUF_SZ];
        int l;
        memset(se,0,sizeof(se));
        read(cToG[0], se, BUF_SZ);
        if(se[0] == 'c'){ // close the process
            freetree(root);
            close(cToG[0]);
            close(gToC[0]);
            exit(0);
        }
        char **wList = malloc(sizeof(char*) * 1);
        strtok(se," ");
        l = splitString(wList,se);
        char begin[BUF_SZ], end[BUF_SZ];
		memset(begin,0,sizeof(begin));
		memset(end,0,sizeof(end));
        strcpy(begin, wList[0]);
        strcpy(end, wList[1]);
        if(addable(root,begin,end)){
            write(gToC[1],"Y",BUF_SZ); // I have spare time to join
        }
        else{
            char period[1000];
            memset(period,0,sizeof(period));
            conflict(root, begin, end, period);
            char con[1000];
			memset(con, 0, sizeof(con));
            sprintf(con,"N %s",period);
			con[strlen(con)-1]=0;
            write(gToC[1],con,BUF_SZ); // I do not have time
        }
        char buf[BUF_SZ];
		memset(buf, 0, sizeof(0));
        read(cToG[0],buf,BUF_SZ);
        if(buf[0]=='Y'){ // add it to my time schedule
            root=insert(root,begin,end);
        }
        free(wList[l]);
    }
}

/*---rescheduler---*/
void deleteNode(Job **head, Job *node) {
	Job *cur=*head;
	if(cur==node){
		Job *temp=*head;
		*head=(*head)->next;
		free(temp);
	}
	else{
		while(cur->next!=node) cur=cur->next;
		cur->next=node->next;
		free(node);
	}
}

void reschd(Job **rejectList, Job **acceptList, int cToG[][2], int gToC[][2],int users, char *userList[]) {
	Job *cur=*rejectList;
	while(cur!=NULL){
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		int n=1;
		Extra *now=cur->remark;
		while(now!=NULL){
			userId[n++]=findUser(now->name, users, userList);
			now=now->next;
		}
		int d=cur->endTime-cur->startTime;
		int i,j;
		int flag=0;
		for(i=20180401; i<=20180414; i++){
			for(j=8;j<=18-d;j++){
				flag=0;
				char start[BUF_SZ],end[BUF_SZ];
				memset(start, 0, sizeof(start));
				memset(end, 0, sizeof(end));
				strcpy(start, dateToString(i, j));
				strcpy(end, dateToString(i, j+d));
				char se[BUF_SZ];
				memset(se, 0, sizeof(se));
				sprintf(se,"%s %s",start, end);
				
				int k;
				for(k=0;k<n;k++){
					write(cToG[userId[k]][1], se, BUF_SZ);
					char buf[BUF_SZ];
					memset(buf, 0, sizeof(buf));
					read(gToC[userId[k]][0], buf, BUF_SZ);
					if(buf[0]=='Y'){
						if(k==n-1){
							int m;
							for(m=0;m<n;m++){
								write(cToG[userId[m]][1], "Y", BUF_SZ);
							}
							cur->date=i;
							cur->startTime=j;
							cur->endTime=j+d;
							Job *node = (Job*)malloc(sizeof(Job)*1);
							copyJob(node, cur);
							addToList(acceptList, node); 
							deleteNode(rejectList, cur);
							flag=1;
						}
					}
					else{
						//buf[0]='N' with the possible number
						char **wList=malloc(sizeof(char*)*1);
						strtok(buf, " ");
						int tot = splitString(wList, buf);
						j=wList[tot-1][8]*10+wList[tot-1][9];
						int m;
						for(m=0;m<=k;m++){
							write(cToG[userId[m]][1], "N", BUF_SZ);
						}
                        free(wList[tot]);
						break;
					}
				}
				if(flag) break;
			}
			if(flag) break;
		}
		cur=cur->next;
	}
}