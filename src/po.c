#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define NOR_INPUT_SZ 120
#define NORMAL_LENGTH 5

#define CLASSES 1
#define MEETING 2
#define GATHERING 3
#define N_CHILD 4 // How many scheduler

typedef struct Extra {
    char name[20];
	char note[100];
    struct Extra *next;
} Extra;

typedef struct Job {
    int ssType;
    char owner[20];
    int date; // YYYYMMDD
    int startTime; // hr only
    int endTime; // hr only
    Extra *remark;
    struct Job *next;
    struct Job *acnext;
	struct Job *renext;
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
void addToAccept(Job **head, Job *node);
void addToReject(Job **head, Job *node);

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

void scheduler_base(int, int, int, int, char *[]);
void scheduler_initJob(Job *, char **, int);
void scheduler_selector(int, Job **, char **, int);
void scheduler_sample(Job **, char **, int);
void scheduler_fcfs(Job **, char **, int);
void scheduler_priority(Job **, char **, int);
void scheduler_special(Job **, char **, int);
void scheduler_print(Job *, Job **, Job **, int, char *[]);

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

//rescheduler
void rescheduler(Job *jobList, Job *acceptList, Job *rejectList, int users, char *userList[]);



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
            for(j = 0; j < N_CHILD; j++){
                close(toChild[j][1]);
                close(toParent[j][0]);
                if(j != i) {
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
	memset(newUser->note, 0, sizeof(newUser->note));
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

char* dateToString(int date, int time)
{
    static char str[80];
    memset(str,0,sizeof(str));
    if(time<10) sprintf(str,"%d0%d00",date,time);
    else sprintf(str,"%d%d00",date,time);
    return str;
}

void addToAccept(Job **head, Job *node)
{
    node->acnext=NULL;
    if(*head == NULL) {
        *head=node;
    }
    else{
        Job *cur=*head;
        while(cur->acnext != NULL) {
            cur=cur->acnext;
        }
        cur->acnext=node;
    }
}

void addToReject(Job **head, Job *node)
{
    node->renext=NULL;
    if(*head == NULL) {
        *head=node;
    }
    else{
        Job *cur=*head;
        while(cur->renext != NULL) {
            cur=cur->renext;
        }
        cur->renext=node;
    }
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
        printf("P: send [%s] to child %d\n", str, i);
        write(toChild[i][1], str, MAX_INPUT_SZ);
    }
}

void parent_write_print(char *str, int toChild[][2], int toParent[][2]) {
	int i;
	char temp[80];
	for(i=0;i<N_CHILD;i++){
		printf("P: send [%s] to child %d\n", str, i);
        write(toChild[i][1], str, MAX_INPUT_SZ);
		read(toParent[i][0], temp, MAX_INPUT_SZ);
	}
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
        case 5: case 6: 
            parent_write_print(str, toChild, toParent); // TODO: avoid writing collision
            break;
        case 0:
            parent_write_print(str, toChild, toParent);
            *loop = 0;
            break;
        default:
            printf("Unrecognized command.\n");
            break;
    }
}

void parent_request(char *cmd, char *users[], int toChild[][2]) {
    char str[MAX_INPUT_SZ];
	memset(str,0,sizeof(str));
    char **wList = malloc(sizeof(char*) * 1);
    int t;

    strcpy(str, cmd);
    strtok(cmd, " ");
    int n = splitString(wList, cmd);
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
        char cmdBuf[MAX_INPUT_SZ] = "";
        char **wList = malloc(sizeof(char*) * 1);
        int t;

        read(fromParent, cmdBuf, MAX_INPUT_SZ);
		printf("S %d: from parent info [%s]\n",schedulerID, cmdBuf);
        strtok(cmdBuf, " ");
        splitString(wList, cmdBuf);
        t = checkType(wList[0]);
		
		
		switch(t) {
            case 1: case 2: case 3:
                scheduler_selector(schedulerID, &jobList, wList, t);
                break;
            case 5://Todo: judge whether the command belongs to itself
				/*
				*if belong -> print -> printschedule
				*/
                printSchd(schedulerID, &jobList, wList, userList, users);
				write(toParent, "Finished", 80);
                break;
            case 6:
				//print -> printreport
				printf("S %d: Begin print. \n", schedulerID);
                printRepo(schedulerID, &jobList, wList, userList, users);
				printf("S %d: Finished. \n", schedulerID);
				write(toParent, "Finished", 80);
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

void scheduler_selector(int schedulerID, Job **head_ref, char **wList, int t) {
    switch(schedulerID) {
        case 0: scheduler_sample(head_ref, wList, t); break;
        case 1: scheduler_fcfs(head_ref, wList, t); break;
        case 2: scheduler_priority(head_ref, wList, t); break;
        case 3: scheduler_special(head_ref, wList, t); break;
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

    newJob->remark = (Extra*)malloc(sizeof(Extra));
    while(wList[++l] != NULL)
        addParticipant(&(newJob->remark), wList[l]);
    newJob->next = NULL;
}

void scheduler_sample(Job **head_ref, char **wList, int t) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));
    
    scheduler_initJob(newJob, wList, t); // input data into list

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

/* TODO: add each type of scheduler (use scheduler_fcfs_sample() as your template) */

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

}

void scheduler_print(Job *jobList, Job **acceptList, Job **rejectList, int users, char *userList[])
{
	int i, j;
	int cToG[users][2],gToC[users][2];
	//grandChild process
	for(i=0;i<users;i++){
		if(pipe(cToG[i])<0 || pipe(gToC[i])<0) {
			printf("Create pipe error...\n");
			exit(1);
		}
		int pid=fork();
		if(pid==0){
			grandchildProcess(cToG[i], gToC[i]);
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
		char se[80], buf[80];
		sprintf(se,"%s %s",dateToString(cur->date, cur->startTime),dateToString(cur->date, cur->endTime));
		
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		int n=1;
		Extra *now=cur->remark;
		while(now!=NULL){
			userId[n++]=findUser(now->name, users, userList);
			now=now->next;
		}
		for(i=0;i<n;i++){
			write(cToG[userId[i]][1], se, 80);
			read(gToC[userId[i]][0], buf, 80);
			if(buf[0]=='Y'){
				if(i==n-1){
					//The job can be accepted
					for(j=0;j<n;j++){
						write(cToG[userId[j]][1], "Y", 80);
					}
					addToAccept(acceptList, cur);
				}
			}
			else{
				//The job cannot be accepted
				for(j=0;j<=i;j++){
					write(cToG[userId[j]][1], "N", 80);
				}
				addToReject(rejectList, cur);
			}
		}
	}
	
	//close grandchild process
	for(i=0;i<users;i++){
		write(cToG[i][0], "c", 80);
	}
	
	for(i=0; i<users; i++) wait(NULL);
	
	for(i=0;i<users;i++){
		close(cToG[i][1]);
		close(gToC[i][0]);
	}
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

void grandchildProcess(int cToG[2], int gToC[2])
{
    rbtnode *root=NULL;
    int n;
    close(cToG[1]);
    close(gToC[0]);
    while(1){
        char se[100];
        memset(se,0,sizeof(se));
        n = read(cToG[0], se, 100);
		printf("grandchild, pid %d: %s\n",getpid(), se);
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
        char begin[80], end[80];
		memset(begin,0,sizeof(begin));
		memset(end,0,sizeof(end));
        strcpy(begin, wList[0]);
        strcpy(end, wList[1]);
		printf("hhh\n",getpid());
        if(addable(root,begin,end)){
			printf("grandchild, pid %d Y\n",getpid());
            write(gToC[1],"Y",3); //I have spare time to join
        }
        else{
			printf("grandchild, pid %d N\n",getpid());
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

int findUser(char *name, int users, char *userList[])
{
    int i;
    for(i=0;i<users;i++){
        if(strcmp(userList[i],name)==0){
            return i;
        }
    }
    return -1;
}

void rescheduler(Job *jobList, Job *acceptList, Job *rejectList, int users, char *userList[])
{
    int i, cToG[users][2], gToC[users][2];
    for(i=0;i<users;i++){
        if(cToG[i]<0 || gToC[i]<0){
            printf("Oops... Pipe creation error. \n");
            exit(1);
        }
        int pid=fork();
        if(pid==0){
            grandchildProcess(cToG[i], gToC[i]);
        }
    }
    for(i=0;i<users;i++){
        close(cToG[i][0]);
        close(gToC[i][1]);
    }
    Job *cur=jobList;
    while(cur!=NULL){
        char se[80];
        char buf[80];
        memset(se, 0, sizeof(se));
        sprintf(se,"%s %s",dateToString(cur->date, cur->startTime),dateToString(cur->date, cur->endTime));
        int userId[20];
        userId[0]=findUser(cur->owner, users, userList);
        Extra *p = cur->remark;
        int j=1;
        while(p!=NULL){
            userId[j++]=findUser(p->name, users, userList);
            p=p->next;
        }
        write(cToG[userId[0]][1],se,80);
        read(gToC[userId[0]][0],buf,80);
        if(buf[0]=='N'){
            write(cToG[userId[0]][1],"N",3); //reject
            addToReject(rejectList, cur);
        }
        else{
            int agree=0;
            int leaveTime[50];
            char info[50][80];
            char str[80]; // TODO:
            

            memset(info,0,sizeof(info));
            memset(leaveTime,0,sizeof(leaveTime));
            for(i=1;i<j;i++){
                write(cToG[userId[i]][1],se,80);
                read(gToC[userId[i]][0],info[i],80);
                if(info[i][0]=='Y') agree++;
                else {
                    char **wList = malloc(sizeof(char*) * 1);
                    strcpy(str, info[i]);
                    strtok(info[i], " ");
                    int n = splitString(wList, info[i]);
                    int k;
                    for(k=1;k<n;k=k+2){
                        leaveTime[i]=leaveTime[i]+(wList[k+1][8]-wList[k][8])*10+(wList[k+1][9]-wList[k][9]);
                    }
                    if(leaveTime[i] <= (cur->endTime - cur->startTime)/2) agree++;
                }
            }
            if(agree>(j+1)/2){
                write(cToG[userId[0]][1],"Y",80);
                Extra *last=NULL;
                Extra *now=cur->remark;
                i=0;
                while(now != NULL){
                    i++;
                    if(leaveTime[i]==0){
                        write(cToG[userId[i]][1],"Y",80);
                        now=now->next;
                    }
                    else if(leaveTime[i] > (cur->endTime - cur->startTime)/2){
                        write(cToG[userId[i]][1],"N",80);
                        if(last!=NULL){
                            last->next=now->next;
                            free(now);
                            now=last->next;
                        }
                        else{
                            Extra *temp=now->next;
                            free(now);
                            now=temp;
                        }
                    }
                    else{
                        write(cToG[userId[i]][1],"N",80);
                        char **wList = malloc(sizeof(char*) * 1);
                        strcpy(str, info[i]);
                        strtok(info[i], " ");
                        int n = splitString(wList, info[i]);

                        char begin[80], end[80];
                        char temp[80];
                        strcpy(begin,dateToString(cur->date, cur->startTime));
                        strcpy(end, dateToString(cur->date, cur->endTime));
                        int flag=0;
                        memset(now->note, 0, sizeof(now->note));
                        if(strcmp(begin, wList[1])!=0){
                            memset(temp,0,sizeof(temp));
                            sprintf(temp, "leave at %c%c00",wList[1][8],wList[1][9]);
                            strcat(now->note, temp);
                            memset(se,0,sizeof(se));
                            sprintf(se,"%s %s",begin,wList[1]);
                            write(cToG[userId[i]][1],se,80);
                            read(gToC[userId[i]][0],info[i],80);
                            write(cToG[userId[i]][1],"Y",80);
                            flag=1;
                        }
                        int k;
                        for(k=2;k<n-1;k=k+2){
                            memset(temp,0,sizeof(temp));
                            if(flag) strcat(now->note, " -> ");
                            sprintf(temp, "come back at %c%c00 -> leave at %c%c00", wList[k][8], wList[k][9], wList[k+1][8], wList[k+1][9]);
                            strcat(now->note, temp);
                            memset(se,0,sizeof(se));
                            sprintf(se,"%s %s",wList[k],wList[k+1]);
                            write(cToG[userId[i]][1],se,80);
                            read(gToC[userId[i]][0],info[i],80);
                            write(cToG[userId[i]][1],"Y",80);
                            flag=1;
                        }
                        if(strcmp(end, wList[n-1])!=0){
                            memset(temp, 0, sizeof(temp));
                            if(flag) strcat(now->note, " -> ");
                            sprintf(temp, "come back at %c%c00",wList[n-1][8],wList[n-1][9]);
                            strcat(now->note, temp);
                            memset(se,0,sizeof(se));
                            sprintf(se,"%s %s",wList[n-1],end);
                            write(cToG[userId[i]][1],se,80);
                            read(gToC[userId[i]][0],info[i],80);
                            write(cToG[userId[i]][1],"Y",80);
                            flag=1;
                        }
                    }
                }
            }
            else{
                for(i=0;i<j;i++) write(cToG[userId[i]][1],"N",80);
            }
        }
        cur=cur->next;
    }
}
