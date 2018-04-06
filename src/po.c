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

typedef struct occupy{
	char start[20], end[20];
} occupy;

/* Prototype */
void readInput          (char(*));
int splitString         (char**, char (*));
int checkType           (char*);
void debug_print        (Job*);
void freeParticipantList(Extra*);
int findUser            (char*, int, char*[]);
char* dateToString      (int, int);
void addToList          (Job**, Job*);
void copyJob            (Job*, Job*);
int countTotalTime      (Job*);
void addParticipant     (Extra**, char*);

void parent_checkUserNum    (int, char*[]);
void parent_write           (char*, int[][2]);
void parent_handler         (char(*), char*[], int*, int[][2], int[][2], int);
void parent_request         (char*, char*[], int[][2], int);
int parent_validate         (char**, char*[], int, int);
int parent_checkUserExist   (char**, char*[], int, int);
int parent_verifyUser       (char*, char*[]);
int parent_verifyParticipant(char**, char*[], int);
void parent_addBatch        (char*, char*[], int[][2], int);
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

rbtnode* leftRotate         (rbtnode *root, rbtnode *x);
rbtnode* rightRotate        (rbtnode *root, rbtnode *y);
int addable                 (rbtnode *root, char *begin, char *end);
char* minstr                (char *a, char *b);
char* maxstr                (char *a, char *b);
void conflict               (rbtnode *node, char *begin, char *end, char *period);
rbtnode* insert             (rbtnode *root, char *begin, char *end);
rbtnode* insert_fixup       (rbtnode *root, rbtnode *node);
void freetree               (rbtnode *node);
void grandchildProcess      (int a, int cToG[2], int gToC[2]);

// Reschedule
void reschd(Job **rejectList, Job **acceptList, int cToG[][2], int gToC[][2],int users, char *userList[]);

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
            parent_handler(cmd, argv, &loop, toChild, toParent, argc);
        }
        for(j = 0; j < N_CHILD; j++) { 
			close(toChild[j][1]);
			close(toParent[j][0]);
		}
    }
    /* wait for all child */
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

void freeJob(Job* node) {
	Job *temp;
	while(node != NULL){
		temp = node;
		node = node->next;
		freeParticipantList(temp->remark);
		free(temp);
	}
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

/*-------Parent_part-------*/
void parent_checkUserNum(int num, char *users[]) {
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

void parent_handler(char (*cmd), char *users[], int *loop, int toChild[][2], int toParent[][2], int user) {
    char str[MAX_INPUT_SZ];
	memset(str, 0, sizeof(str));
    int t;
	
    strcpy(str, cmd);
    strtok(cmd," ");
    t = checkType(cmd);
    switch(t) {
        case 1: case 2: case 3:
            parent_request(str, users, toChild, user);  break;
        case 4:
            parent_addBatch(str, users, toChild, user); break;
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

void parent_request(char *cmd, char *users[], int toChild[][2], int user) {
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
    else if(parent_validate(wList, users, t, user)) 
        parent_write(str,toChild);
    free(wList[l]);
}

int parent_validate(char **wList, char *users[], int t, int user) {
    int tStart = atoi(wList[3]) / 100;
    int cStart = atoi(wList[3]) % 100;
	
    if(atoi(wList[2]) < 20180401 || atoi(wList[2]) > 20180414)      printf("Error: Invalid Date\n");
	else if(cStart != 0 || (tStart < 8 || tStart > 17))             printf("Error: Invalid Starting Time\n");
    else if (atoi(wList[4]) < 1 || (atoi(wList[4]) + tStart) > 18)  printf("Error: Invalid Duration\n");
    else return parent_checkUserExist(wList, users, t, user);
    return 0;
}

int parent_checkUserExist(char **wList, char* users[], int t, int user) {
    int ownerValid, usersValid = 1;
    /* check owner's and participants' names and avoid duplicate */
    ownerValid = parent_verifyUser(wList[1], users);
    if(ownerValid == 1 && t != 1) 
        usersValid = parent_verifyParticipant(wList, users, user);
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

int parent_verifyParticipant(char **wList, char* users[], int user) {
    int l = NORMAL_LENGTH -1;
    int res = 1;

    while(wList[++l] != NULL) {
        if(findUser(wList[l], user, users) == -1) res = 0; 
    }
    if(l <= NORMAL_LENGTH) res = 0;
    return res;
}

void parent_addBatch(char *cmd, char *users[], int toChild[][2], int user) {
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
        parent_request(line, users, toChild, user);
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
    // debug_print(jobList);
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
			scheduler_print(jobList, &acceptList, &rejectList, users, userList, 1);
			flag=0;
			break;
		}
	}
    if(flag){
		scheduler_print(jobList, &acceptList, &rejectList, users, userList, 0);
	}

    // TODO: check isFile duplicate (true -> need to change title)

	if(t == 5)  printer_userSchedule(schedulerID, wList[1], wList[3], acceptList);
	else        printer_report(schedulerID, wList[1], toParent, acceptList, rejectList, users);
	freeJob(acceptList);
	freeJob(rejectList);
}

int getTimeslot(char *buf) {
	char **wList = malloc(sizeof(char*)*1);
    int l;

	strtok(buf, " ");
	l = splitString(wList, buf);
	int i = 0, ans = 0;
	while(wList[++i]!=NULL){
		ans = ans+(wList[i+1][8]-wList[i][8])*10+(wList[i+1][9]-wList[i][9]);
		i++;
	}
    free(wList[l]);
	return ans;
}

void scheduler_print(Job *jobList, Job **acceptList, Job **rejectList, int users, char *userList[], int rescheduler) {
	printf("This is rescheduler %d.\n", rescheduler);
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
		for(i=0;i<n;i++){
			write(cToG[userId[i]][1], se, BUF_SZ);
			memset(buf, 0, sizeof(buf));
			read(gToC[userId[i]][0], buf, 1000);
			printf("child: from user %d [%s]\n",userId[i], buf);
			if(buf[0]=='Y'){
				if(i==n-1){
					printf("This job will be accepted.\n");
					//The job can be accepted
					for(j=0;j<n;j++){
						write(cToG[userId[j]][1], "Y", BUF_SZ);
					}
					Job* node = (Job*)malloc(sizeof(Job)*1);
					copyJob(node, cur);
					addToList(acceptList, node);
					printf("Accepted! \n");
				}
			}
			else{
				printf("This job will be rejected.\n");
				//The job cannot be accepted
				for(j=0;j<=i;j++){
					write(cToG[userId[j]][1], "N", BUF_SZ);
				}
				printf("user rejected.\n");
				Job *node = (Job*)malloc(sizeof(Job)*1);
				copyJob(node, cur);
				addToList(rejectList, node);
				printf("Rejected! \n");
				break;
			}
		}
		cur=cur->next;
	}
	printf("child: finished getting \n");
	
	if(rescheduler){
		printf("Here will implemented rescheduler. \n");
		sleep(1);
		reschd(rejectList, acceptList, cToG, gToC, users, userList);
	}
	printf("child: finished rescheduling \n");
	
	int tot=0;
	//get total time slot
	for(i=0;i<users;i++){
		char buf[1000];
		write(cToG[i][1], "201804010700 201804141900", BUF_SZ);
		memset(buf, 0, sizeof(buf));
		read(gToC[i][0], buf, 1000);
		write(cToG[i][1], "N", BUF_SZ);
		tot+=getTimeslot(buf);
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
	
	printf("(%d)The accept list is: \n",getpid());
	debug_print(*acceptList);
	printf("(%d)The reject list is: \n",getpid());
	debug_print(*rejectList);
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
    if(curr->endTime < 10) sprintf(start, "0%d:00", curr->endTime);
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

void preorder(rbtnode *cur) {
	if(cur==NULL) return;
	preorder(cur->left);
	printf("%s %s\n",cur->begin,cur->end);
	preorder(cur->right);
}

void grandchildProcess(int a, int cToG[2], int gToC[2]) {
    rbtnode *root=NULL;
    int n;
    close(cToG[1]);
    close(gToC[0]);
    while(1){
        char se[100];
        int l;
        memset(se,0,sizeof(se));
        n = read(cToG[0], se, 100);
        se[n] = 0;
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
            write(gToC[1],"Y",3); // I have spare time to join
        }
        else{
            char period[1000];
            memset(period,0,sizeof(period));
            conflict(root, begin, end, period);
            char con[1000];
			memset(con, 0, sizeof(con));
            sprintf(con,"N %s",period);
			con[strlen(con)-1]=0;
            write(gToC[1],con,strlen(con)); // I do not have time
        }
        char buf[BUF_SZ];
		memset(buf, 0, sizeof(0));
        read(cToG[0],buf,BUF_SZ);
        if(buf[0]=='Y'){ // add it to my time schedule
            root=insert(root,begin,end);
        }
		//printf("%d My current tree is: \n",a);
		//preorder(root);
        free(wList[l]);
    }
}

/*---rescheduler---*/
int distance(char *start, char *end, int d) {
	printf("%s %s\n", start, end);
	if(strcmp(start, end)<=0) return 0;
	int day=1;
	int i;
	int sd=(start[6]-'0')*10+(start[7]-'0');
	int ed=(end[6]-'0')*10+(end[7]-'0');
	int sr=(start[8]-'0')*10+(start[9]-'0');
	int er=(end[8]-'0')*10+(end[9]-'0');
	for(i=sd;i<=ed;i++){
		int s,e;
		if(i==sd) s=sr;
		else s=8;
		if(i==ed) e=er;
		else e=18;
		if(e-s>=d){
			start[6]=i/10+'0';
			start[7]=i%10+'0';
			start[8]=s/10+'0';
			start[9]=s%10+'0';
			return 1;
		}
	}
	return 0;
}

void plus(char *start, int d, char *end)
{
	strcpy(end, start);
	if(end[9]+d>'0'+10){
		end[8]+=1;
		end[9]=end[9]+d-10;
	}
}

void inverse(occupy a[], int n, Job *node, int d)
{
	char start[20];
	int i;
	strcpy(start, "201804010800");
	for(i=0;i<n;i++){
		if(distance(start, a[i].start, d)){
			char end[20];
			node->date=(start[0]-'0')*10000000+(start[1]-'0')*1000000+(start[2]-'0')*100000+
						(start[3]-'0')*10000+(start[4]-'0')*1000+(start[5]-'0')*100+(start[6]-'0')*10+(start[7]-'0');
			node->startTime=(start[8]-'0')*10+start[9]-'0';
			plus(start, d, end);
			node->endTime=(end[8]-'0')*10+(end[9]-'0');
			return ;
		}
		else strcpy(start, a[i].end);
	}
}

void deleteNode(Job **head, Job *node)
{
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

void storeToOccupy(char *buf, occupy list[], int *tot)
{
	char **wList=malloc(sizeof(char*)*1);
    int l, i=0;
	strtok(buf, " ");
	l = splitString(wList, buf);
	while(wList[++i]!=NULL){
		strcpy(list[*tot].start, wList[i]);
		strcpy(list[*tot].start, wList[++i]);
		*tot=*tot+1;
	}
    free(wList[l]);
}

int cmp(const void *a, const void *b)
{
	struct occupy *c = (struct occupy *)a;
	struct occupy *d = (struct occupy *)b;
	if(strcmp(c->start, d->start) == 0) return strcmp(d->end, c->end);
	return strcmp(c->start, d->start);
}

void merge(occupy a[], occupy mer[], int n, int *tot)
{
	strcpy(mer[0].start, a[0].start);
	strcpy(mer[0].end, a[0].end);
	*tot=0;
	int i;
	for(i=1;i<n;i++){
		if(strcmp(mer[*tot].start, a[i].start)<=0 && strcmp(a[i].start, mer[*tot].start)<=0){
			if(strcmp(mer[0].end, a[i].end)<0) strcpy(mer[0].end, a[i].end);
		}
		else{
			*tot=*tot+1;
			strcpy(mer[*tot].start, a[i].start);
			strcpy(mer[*tot].end, a[i].end);
		}
	}
	*tot=*tot+1;
}

void reschd(Job **rejectList, Job **acceptList, int cToG[][2], int gToC[][2],int users, char *userList[])
{
	Job *cur=*rejectList;
	while(cur!=NULL){
		printf("reschedule child %d: current job is %s\n",getpid(),cur->owner);
		occupy a[2000];
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		int n=1;
		Extra *now=cur->remark;
		while(now!=NULL){
			userId[n++]=findUser(now->name, users, userList);
			now=now->next;
		}
		int d=cur->endTime-cur->startTime;
		int i;
		for(i=0;i<n;i++){
			printf("child: send [201804010700 201804141900] to user %d\n", userId[i]);
			write(cToG[userId[i]][1], "201804010700 201804141900", BUF_SZ);
		}
		
		int seg=0;
		for(i=0;i<n;i++){
			char buf[2000];
			memset(buf, 0, sizeof(buf));
			read(gToC[userId[i]][0], buf, 2000);
			printf("child: information from user %d [%s]\n", userId[i], buf);
			write(cToG[userId[i]][1], "N", 3);
			//storeToOccupy(buf, a, &seg);
		}
		cur=cur->next;
	}
}