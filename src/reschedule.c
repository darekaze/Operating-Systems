/*rescheduler: 
对于meeting和gathering，允许参与者中途离开或中途进来，但需符合如下规则：
（注：活动持有者必须从头到尾参与该项活动中）
1. 若要中途离开，离开时间不应超过总时长的一半
2. 在任何时间内参与总人数都需超过一半*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define NORMAL_LENGTH 5

#define CLASSES 1
#define MEETING 2
#define GATHERING 3
#define N_CHILD 4 // How many scheduler

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
	struct Job *acnext;
	struct Job *renext;
} Job;

char* dateToString(int date, int time)
{
	static char str[80];
	memset(str,0,sizeof(str));
	if(time<10) sprintf(str,"%d0%d00",date,time);
	else sprintf(str,"%d%d00",date,time);
	return str;
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

//add to reject linked list
void addToReject(Job *head, Job *node)
{
	node->renext=NULL;
	if(head == NULL) {
		head=node;
	}
	else{
		Job *cur=head;
		while(cur->renext != NULL) {
			cur=cur->renext;
		}
		cur->renext=node;
	}
}

//add to accept linked list
void addToAccept(Job *head, Job *node)
{
	node->acnext=NULL;
	if(head == NULL) {
		head=node;
	}
	else{
		Job *cur=head;
		while(cur->acnext != NULL) {
			cur=cur->acnext;
		}
		cur->acnext=node;
	}
}

void rescheduler(char *jobList, char *acceptList, char *rejectList, int users, char *userList[])
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
		close(cToG[0]);
		close(gToC[1]);
	}
	Job *cur=jobList;
	while(cur!=NULL){
		char se[80];
		memset(se, 0, sizeof(se));
		sprintf(se,"%s %s",dateToString(cur->date, cur->startTime),dateToString(cur->date, cur->endTime));
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		extra *p= cur->remark;
		int j=1;
		while(p!=NULL){
			userId[j++]=findUser(p->name, users, userList);
			p=p->next;
		}
		write(cToG[userId[0]][1],se,80);
		char buf[80];
		read(gToC[userId[0]][1],buf,80);
		if(buf[0]=='N'){
			write(cToG[userId[0]][1],"N",3); //reject
			addToReject(reject, cur);
		} 
		else{
			
		}
		cur=cur->next;
	}
}