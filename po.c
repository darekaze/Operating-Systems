#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define BUF_SZ 80
#define TIME_SZ 50
#define NORMAL_LENGTH 5

#define CLASSES 1
#define MEETING 2
#define GATHERING 3
#define N_CHILD 3 // How many scheduler

typedef struct Extra {
    char name[20];
    struct Extra *next;
} Extra;

typedef struct Job {
    int ssType;
    char owner[20];
    int date; // YYYYMMDD
    int startTime; // hr only
    int endTime; // hr only
    Extra *remark; // NULL by default
    struct Job *next;
} Job;

typedef struct rbtnode{
    int color; //Red: 0 Black: 1
    char begin[15];
    char end[15];
    struct rbtnode *left;
    struct rbtnode *right;
    struct rbtnode *parent;
}rbtnode;

/* Prototype */
void readInput(char (*));
int splitString(char**, char (*));
int checkType(char*);
void debug_print(Job*);
void freeParticipantList(Extra*);
int findUser(char *, int , char *[]);
char* dateToString(int date, int time);
void addToList(Job **head, Job *node);
void copyJob(Job *, Job *);

void parent_checkUserNum(int, char *[]);
void parent_write(char *, int [][2]);
void parent_handler(char (*), char *[], int *, int [][2], int [][2]);
void parent_request(char *, char *[], int [][2]);
int parent_validate(char **, char *[], int);
int parent_checkUserExist(char **, char* [], int);
int parent_verifyUser(char *, char* []);
int parent_verifyParticipant(char **, char *[]);
int parent_checkDuplicate(Extra **, char *);
void parent_addBatch(char*, char*[], int [][2]);
void parent_askSchd(char *, char *[], int [][2]);

void scheduler_base(int, int, int, int, char *[]);
void scheduler_initJob(Job *, char **, int);
void scheduler_selector(int, Job **, char **, int);
void scheduler_fcfs(Job **, char **, int);
void scheduler_priority(Job **, char **, int);
void scheduler_special(Job **, char **, int);
void scheduler_print(Job *, Job **, Job **, int, char *[]);
void scheduler_exct(int, int, Job *, char *[], char **, int);

void printer_userSchedule(int, char *, char *, Job *);
void printer_report(int, char *, int, Job *, Job *, int);
void printer_outputJob(Job *, FILE *, char*);

rbtnode* leftRotate(rbtnode *root, rbtnode *x);
rbtnode* rightRotate(rbtnode *root, rbtnode *y);
int addable(rbtnode *root, char *begin, char *end);
char* minstr(char *a, char *b);
char* maxstr(char *a, char *b);
void conflict(rbtnode *node, char *begin, char *end, char *period);
rbtnode* insert(rbtnode *root, char *begin, char *end);
rbtnode* insert_fixup(rbtnode *root, rbtnode *node);
void freetree(rbtnode *node);
void grandchildProcess(int a, int cToG[2], int gToC[2]);

//rescheduler
void rescheduler(Job *jobList, Job **acceptList, Job **rejectList, int users, char *userList[]);



/*-------Main-------*/
int main(int argc, char *argv[]) {
    int toChild[N_CHILD][2], toParent[N_CHILD][2];
    pid_t shut_down[N_CHILD];
    int pid, i, j;

    parent_checkUserNum(--argc, ++argv);
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
            close(toChild[i][1]);
			close(toParent[i][0]);
			
			// read: toChild[i][0] write: toParent[i][1]
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
			//printf("parent: the user input is [%s]. \n", cmd);
            parent_handler(cmd, argv, &loop, toChild, toParent);
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

void freeJob(Job* node)
{
	Job *temp;
	while(node!=NULL){
		temp=node;
		node=node->next;
		freeParticipantList(temp->remark);
		free(temp);
	}
}

void debug_print(Job *head) {
	Job *cur=head;
    //printf("Debug-log\n");
    while(cur != NULL) {
        printf("%d %s %d %d %d\n", cur->ssType, cur->owner, cur->date, cur->startTime, cur->endTime);
        cur = cur->next;
    }
    printf("\n");
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
    }
    else{
        Job *cur=*head;
        while(cur->next != NULL) {
            cur=cur->next;
        }
        cur->next=node;
    }
}

int checkSchd(char *schd) {
    if (strcmp(schd, "fcfs") == 0)      return 0;
    else if (strcmp(schd, "pr") == 0)    return 1;
    else if (strcmp(schd, "special") == 0)    return 2;
    return -1;
}

char *schdName(int schedulerID) {
    switch(schedulerID) {
        case 0: return "First Come First Serve"; break;
        case 1: return "Priority"; break;
        case 2: return "Special"; break;
        default: return NULL;
    }
}

int findUser(char *name, int users, char *userList[]) {
    int i;
    for(i=0;i<users;i++){
        if(strcmp(userList[i],name)==0){
            return i;
        }
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
		Extra *now=dest->remark;
		Extra *cur=src->remark;
		while(cur!=NULL){
			Extra *node=(Extra*)malloc(sizeof(Extra)*1);
			strcpy(node->name, cur->name);
			node->next=NULL;
			if(now==NULL){
				now=node;
			}
			else{
				now->next=node;
				now=now->next;
			}
			cur=cur->next;
		}
	}
	dest->next=NULL;
}

/*-------Parent_part-------*/
void parent_checkUserNum(int num, char *users[]) {
    if(num < 3 || num > 9) {
        printf("You're trolling: Range should be 3~9 users");
        exit(1);
    }
}

void parent_write(char *str, int toChild[][2]) {
    int i;
    for(i = 0; i < N_CHILD; i++) {
        //printf("P: send [%s] to child %d\n", str, i);
        write(toChild[i][1], str, MAX_INPUT_SZ);
    }
}

void parent_repo(char *str, int toChild[][2], int toParent[][2]) {
	int i;
	char temp[BUF_SZ];
	for(i=0;i<N_CHILD;i++){
		//printf("P: send [%s] to child %d\n", str, i);
        write(toChild[i][1], str, MAX_INPUT_SZ);
		read(toParent[i][0], temp, MAX_INPUT_SZ);
		//printf("P: receive [%s] from child %d\n", temp, i);
	}
}

void parent_handler(char (*cmd), char *users[], int *loop, int toChild[][2], int toParent[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str, 0, sizeof(str));
    int t;
	
    strcpy(str, cmd);
    strtok(cmd," ");
    t = checkType(cmd);
	//printf("parent: I am in the handler. %d\n", t);
    switch(t) {
        case 1: case 2: case 3:
            parent_request(str, users, toChild);    break;
        case 4:
            parent_addBatch(str, users, toChild);   break;
        case 5:
            parent_askSchd(str, users, toChild);    break;
        case 6:
            parent_repo(str, toChild, toParent);    break;
        case 0: 
            parent_write(str, toChild); *loop = 0;  break;
        default:
            printf("Unrecognized command.\n");      break;
    }
}

void parent_askSchd(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str,0,sizeof(str));
    char **wList = malloc(sizeof(char*) * 1);
    int t, m, n;

    strcpy(str, cmd);
    strtok(cmd, " ");
    n = splitString(wList, cmd);
    m = parent_verifyUser(wList[1], users);
    t = checkSchd(wList[2]);
    printf("%d %d %d\n",n,m,t);
    if(n > 2 && m > 0 && t > 0)
        write(toChild[t][1], str, MAX_INPUT_SZ);
}

void parent_request(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str,0,sizeof(str));
    char **wList = malloc(sizeof(char*) * 1);
    int t;

    strcpy(str, cmd);
    strtok(cmd, " ");
    splitString(wList, cmd);
    t = checkType(cmd);
    if(parent_validate(wList, users, t)) 
        parent_write(str,toChild);
}

int parent_validate(char **wList, char *users[], int t) {
    int tStart = atoi(wList[3]) / 100;
    int cStart = atoi(wList[3]) % 100;
    int length = -1;

    while(wList[++length] != NULL);
	if(length < NORMAL_LENGTH) printf("Error: Invalid Argument Number\n");
    else if(atoi(wList[2]) < 20180401 || atoi(wList[2]) > 20180414) printf("Error: Invalid Date\n");
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
        char cmdBuf[MAX_INPUT_SZ];
		memset(cmdBuf, 0, sizeof(cmdBuf));
        char **wList = malloc(sizeof(char*) * 1);
        char ty[20] = "";
        int t;
		//printf("		child %d: I am waiting to read\n", schedulerID);
        read(fromParent, cmdBuf, MAX_INPUT_SZ);
		//printf("   child %d: read %s\n", schedulerID, cmdBuf);
		
        strtok(cmdBuf, " ");
        int n=splitString(wList, cmdBuf);
		
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
                printf("Meh..child\n"); loop = 0;
                break;
        }
		//printf("		child %d: I am in the scheduler base loop: %d\n", schedulerID, loop);
    }
    debug_print(jobList);
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
        temp = *head_ref;
        Job *pre = (Job*)malloc(sizeof(Job));
        
        while(newJob->date > temp->date || 
          (newJob->date == temp->date && 
          newJob->startTime >= temp->startTime)){
        	if (temp->next == NULL) break;
        	pre=temp;
            temp = temp->next;
        }
        if (temp == *head_ref) {
			*head_ref=newJob;
			newJob->next=temp;
		}
		else if (temp->next == NULL && 
          !(newJob->date > temp->date || 
          (newJob->date == temp->date && 
          newJob->startTime >= temp->startTime))){
			  temp->next = newJob;
		}
		else{
			pre->next=newJob;
			newJob->next=temp;
		}
    }
}

void scheduler_priority(Job **head_ref, char **wList, int t) {
	Job *temp, *newJob = (Job*)malloc(sizeof(Job));
    
    scheduler_initJob(newJob, wList, t); // input data 

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
    
    scheduler_initJob(newJob, wList, t); // input data 

    if(*head_ref == NULL 
    || ((*head_ref)->ssType > newJob->ssType &&
        (*head_ref)->endTime - (*head_ref)->startTime < newJob->endTime - newJob->startTime)) {
        newJob->next = *head_ref;
        *head_ref = newJob;
    } else {
        temp = *head_ref;
        while(temp->next != NULL && (temp->next->ssType < newJob->ssType) ){
            temp = temp->next;
        }
        while(temp->next != NULL &&
         (temp->next->ssType == newJob->ssType &&
          temp->endTime - temp->startTime > newJob->endTime - newJob->startTime)){
            temp = temp->next;
        }
        newJob->next = temp->next;
        temp->next = newJob;
    }
}

void scheduler_exct(int schedulerID, int users, Job *jobList, char *userList[], char **wList, int toParent) {
    Job *acceptList=NULL;
	Job *rejectList=NULL;
	int t = checkType(wList[0]);
    scheduler_print(jobList, &acceptList, &rejectList, users, userList);
    // No need to check schd as already done in parent
	if(t == 5){
        //printf("S %d: Begin print. \n", schedulerID);
        printer_userSchedule(schedulerID, wList[1], wList[3], acceptList);
        //printf("S %d: Finished. \n", schedulerID);
	} else {
		//printf("S %d: Begin print. \n", schedulerID);
        printer_report(schedulerID, wList[1], toParent, acceptList, rejectList, users);
		//printf("S %d: Finished. \n", schedulerID);
	}
	freeJob(acceptList);
	freeJob(rejectList);
}

int getTimeslot(char *buf)
{
	char **wList=malloc(sizeof(char*)*1);
	strtok(buf, " ");
	splitString(wList, buf);
	int i=0;
	int ans=0;
	while(wList[++i]!=NULL){
		ans=ans+(wList[i+1][8]-wList[i][8])*10+(wList[i+1][9]-wList[i][9]);
		i++;
	}
	return ans;
}

void scheduler_print(Job *jobList, Job **acceptList, Job **rejectList, int users, char *userList[]) {
	printf("The job list is: \n");
	debug_print(jobList); 
	
	int i, j;
	int cToG[users][2],gToC[users][2];
	// grandChild process
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
	//printf("child %d: begin to get ac and re. \n", getpid());
	Job *cur=jobList;
	while(cur!=NULL){
		printf("child %d: current job is %s\n",getpid(),cur->owner);
		char se[BUF_SZ], buf[1000], start[BUF_SZ], end[BUF_SZ];
		memset(se,0,sizeof(se));
		strcpy(start, dateToString(cur->date, cur->startTime));
		strcpy(end, dateToString(cur->date, cur->endTime));
		sprintf(se,"%s %s",start,end);
		
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		int n=1;
		Extra *now=cur->remark;
		while(now!=NULL){
			userId[n++]=findUser(now->name, users, userList);
			now=now->next;
		}
		//printf("child %d: i will tell %d grandchild.\n",getpid(),n);
		for(i=0;i<n;i++){
			//printf("child: i send [%s] to %d\n",se, userId[i]);
			write(cToG[userId[i]][1], se, BUF_SZ);
			memset(buf, 0, sizeof(buf));
			read(gToC[userId[i]][0], buf, 1000);
			//printf("child %d: i receive [%s] from %d\n", getpid(), buf, userId[i]);
			if(buf[0]=='Y'){
				if(i==n-1){
					//The job can be accepted
					for(j=0;j<n;j++){
						//printf("child %d: i send [Y] to %d\n", getpid(), userId[j]);
						write(cToG[userId[j]][1], "Y", BUF_SZ);
					}
					Job* node = (Job*)malloc(sizeof(Job)*1);
					copyJob(node, cur);
					addToList(acceptList, node);
					printf("Accepted! \n");
				}
			}
			else{
				//The job cannot be accepted
				for(j=0;j<=i;j++){
					write(cToG[userId[j]][1], "N", BUF_SZ);
				}
				Job *node = (Job*)malloc(sizeof(Job)*1);
				copyJob(node, cur);
				addToList(rejectList, node);
				printf("Rejected! \n");
				break;
			}
		}
		cur=cur->next;
	}
	//printf("child %d: end of getting ac and re. \n", getpid());
	
	int tot=0;
	//get total time slot
	for(i=0;i<users;i++){
		char buf[1000];
		write(cToG[i][1], "201804010700 201804141900", BUF_SZ);
		memset(buf, 0, sizeof(buf));
		read(gToC[i][0], buf, 1000);
		//printf("child: i receive [%s] from %d\n", buf, i);
		write(cToG[i][1], "N", BUF_SZ);
		tot+=getTimeslot(buf);
		//printf("child: current total time slots is %d. \n", tot);
	}
	
	//close grandchild process
	for(i=0;i<users;i++){
		//printf("child %d: i send [c] to %d\n", getpid(), i);
		write(cToG[i][1], "c", BUF_SZ);
	}
	
	for(i=0; i<users; i++) wait(NULL);
	
	for(i=0;i<users;i++){
		close(cToG[i][1]);
		close(gToC[i][0]);
	}
	
	//printf("(%d)The accept list is: \n",getpid());
	debug_print(*acceptList);
	//printf("(%d)The reject list is: \n",getpid());
	debug_print(*rejectList);
}

/* Printer part */
void printer_userSchedule(int schedulerID, char *userName, char *fileName, Job *acceptList) {
    FILE *f;
    int slotUsed = 0, eventCount = 0, isExist = 0;
    char *schedulerName;
    Job *temp;

    // TODO: check isFile duplicate (true -> need to change title)
    f = fopen(fileName, "w");
    if (f == NULL) {
        printf("Error opening file\n"); return;
    }
    schedulerName = schdName(schedulerID);
    temp = acceptList;
    while(temp != NULL) {
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

    fprintf(f, "Personal Organizer\n***Appointment Schedule***\n\
        %s, you have \033[4m%d\033[0m appointments\n\
        Algorithm used: %s\n\
        \033[4m%d\033[0m timeslots occupied.\n\n\
        Date\tStart\tEnd\tType\tRemarks\n\
        =========================================================\n\
    ",userName, eventCount, schedulerName, slotUsed);

    temp = acceptList;
    while(temp != NULL) {
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
    fprintf(f, "\n\
        =========================================================\n\
        \n\t- End -\n");
    fclose(f);
}

void printer_report(int schedulerID, char *fileName, int toParent, Job *acceptList, Job *rejectList, int num) {
    FILE *f;
    int acceptCount = 0, rejectCount = 0;
    int slotUsed = 0, maxSlot = num * 14 * 10;
    char *schedulerName;
    Job *temp;
    // TODO: check isFile duplicate (true -> need to change title)
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

    fprintf(f, "Personal Organizer\n***Schedule Report***\n\
        Algorithm used: %s\n\
        ***Accept List***\n\
        There are \033[4m%d\033[0m requests accepted.\n\
        Date\tStart\tEnd\tType\tRemarks\n\
        =========================================================\n\
    ", schedulerName, acceptCount);
    temp = acceptList;
    while(temp != NULL) {
        Extra *tex = temp->remark;
        int thour = temp->endTime - temp->startTime;
        slotUsed += thour; // owner time
        while(tex != NULL)
            slotUsed += thour; // participant time
        printer_outputJob(temp, f, NULL);
        temp = temp->next;
    }

    fprintf(f, "\n\
        =========================================================\n\n\
        ***Reject List***\n\
        There are \033[4m%d\033[0m requests rejected.\n\
        Date\tStart\tEnd\tType\tRemarks\n\
        =========================================================\n\
    ", rejectCount);
    temp = rejectList;
    while(temp != NULL) {
        printer_outputJob(temp, f, NULL);
        temp = temp->next;
    }

    fprintf(f, "\n\
        =========================================================\n\
        Total number of request:    %d\n\
        Timeslot in use:            %d\n\
        Timeslot not in use:        %d\n\
        Utilization:                %d\n\
        \n\t- End -\n\
    ", acceptCount + rejectCount,slotUsed, maxSlot-slotUsed, (slotUsed*100)/maxSlot);
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
    if(curr->endTime < 10) sprintf(start, "0%d:00", curr->endTime);
    else sprintf(end, "%d:00", curr->endTime);
    switch(curr->ssType) {
        case CLASSES:   strcpy(type, "Class");      break;
        case MEETING:   strcpy(type, "Meeting");    break;
        case GATHERING: strcpy(type, "Gathering");  break;
    }
    if(userName == NULL) { // report
        sprintf(remark, "\033[4m%s\033[0m", curr->owner);
        while(tex != NULL) {
            sprintf(remark, "%s %s", remark, tex->name);
            tex = tex->next;
        }
    } else if(strcmp(userName, curr->owner) == 0) { // owner
        if(tex == NULL) strcpy(remark, "-");
        else {
            sprintf(remark, "%s", tex->name);
            tex = tex->next;
            while(tex != NULL) {
                sprintf(remark, "%s %s", remark, tex->name);
                tex = tex->next;
            }  
        }
    } else { // participant
        sprintf(remark, "%s", curr->owner);
        while(tex != NULL) {
            if(strcmp(userName, tex->name) != 0)
                sprintf(remark, "%s %s", remark, tex->name);
            tex = tex->next;
        }  
    }
    fprintf(f, "%s-%s-%s   %s   %s   %s      %s\n", YY, MM, DD, start, end, type, remark);
}

/*-------------Grandchild process-------------*/
rbtnode* leftRotate(rbtnode *root, rbtnode *x)
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
	return root;
}

rbtnode* rightRotate(rbtnode *root, rbtnode *y){
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

rbtnode* insert(rbtnode *root, char *begin, char *end)
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
    root=insert_fixup(root,node);
	return root;
}

rbtnode* insert_fixup(rbtnode *root, rbtnode *node)
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

void freetree(rbtnode *node)
{
    if(node==NULL) return;
    freetree(node->left);
    freetree(node->right);
    free(node);
}

void preorder(rbtnode *cur)
{
	if(cur==NULL) return;
	preorder(cur->left);
	printf("%s %s\n",cur->begin,cur->end);
	preorder(cur->right);
}

void grandchildProcess(int a, int cToG[2], int gToC[2])
{
    rbtnode *root=NULL;
    int n;
    close(cToG[1]);
    close(gToC[0]);
    while(1){
        char se[100];
        memset(se,0,sizeof(se));
        n = read(cToG[0], se, 100);
        se[n] = 0;
        if(se[0] == 'c'){
            // close the process
            freetree(root);
            close(cToG[0]);
            close(gToC[0]);
            exit(0);
        }
        char **wList = malloc(sizeof(char*) * 1);
        strtok(se," ");
        splitString(wList,se);
        char begin[BUF_SZ], end[BUF_SZ];
		memset(begin,0,sizeof(begin));
		memset(end,0,sizeof(end));
        strcpy(begin, wList[0]);
        strcpy(end, wList[1]);
        if(addable(root,begin,end)){
            write(gToC[1],"Y",3); //I have spare time to join
        }
        else{
            char period[1000];
            memset(period,0,sizeof(period));
            conflict(root, begin, end, period);
            char con[1000];
			memset(con, 0, sizeof(con));
            sprintf(con,"N %s",period);
			con[strlen(con)-1]=0;
            write(gToC[1],con,strlen(con)); //I do not have time
        }
        char buf[BUF_SZ];
		memset(buf, 0, sizeof(0));
        read(cToG[0],buf,BUF_SZ);
        if(buf[0]=='Y'){
            //add it to my time schedule
            root=insert(root,begin,end);
        }
    }
}



