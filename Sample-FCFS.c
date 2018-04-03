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
} Job;

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

/*------------Child_part------------*/
void scheduler_base(int schedulerID, int fromParent, int toParent, int users, char *userList[]) {
    int loop = 1;
	Job *jobList = NULL;

    int i;

	while (loop) {
		char cmdBuf[MAX_INPUT_SZ] = "";
		char **wList = malloc(sizeof(char *) * 1);
		int t, l;

		read(fromParent, cmdBuf, MAX_INPUT_SZ);
		strtok(cmdBuf, " ");
		l = splitString(wList, cmdBuf);
		t = checkType(wList[0]);

		switch (t) {
			case 1: case 2: case 3:
				scheduler_selector(i, jobList, wList, l, t);
				break;
			case 5:
				printSchd(i, &acceptList, wList, l, t);
				break;
			case 6:
				printRepo(i, &acceptList, &rejectList, wList, l, t);
				break;
			case 0:
				loop = 0;
				break;
			default:
				printf("Meh..\n");
				break;
		}
	}
    
    debug_print(jobList);
    free(jobList);
	exit(0);
}

void scheduler_selector(int schedulerID, Job *head_ref, char **wList, int length, int t) {
    switch(schedulerID) {
        case 0: scheduler_sample(head_ref, achead_ref, rehaead_ref, wList, length, t); break;
		
        case 1: scheduler_fcfs(head_ref, wList, length, t); break; //use this as an example
		
        case 2: scheduler_priority(head_ref, achead_ref, rehaead_ref, wList, length, t); break;
        case 3: scheduler_special(head_ref, achead_ref, rehaead_ref, wList, length, t); break;
        default: break;
    }
}

void scheduler_initJobNode(Job *newJob, char **wList, int t) {
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
}

void scheduler_fcfs(Job *head, char **wList, int length, int t) {
	//add to the linked list based on the time order
    Job *newJob = (Job *)malloc(sizeof(Job));
    scheduler_initJob(newJob, wList, t);
	if(head == NULL) {
		head=newJob;
	}
	else{
		Job *last=NULL;
		Job *cur=head;
		while(cur != NULL && (newJob->date > cur->date) || (newJob->date == cur->date && newJob->startTime >= cur->startTime)) {
			last=cur;
			cur=cur->next;
		}
		if(last!=NULL){
			last->next=newJob;
		}
		newJob->cur=cur;
	}
}


















void scheduler_sample(Job **head_ref, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t) {
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

void scheduler_priority(Job **head_ref, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t) {

}

void scheduler_special(Job **head_ref, Job **achead_ref, Job **rehaead_ref, char **wList, int length, int t) {

}

//print the schedule
void printSchd(int schedulerID, Job **head_ref, char **wList, int length, int t){
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
    if(schedulerID==0){ //sample
        fprintf(file, "Algorithm used: Sample\n");
    }
    else if(schedulerID==1){
        fprintf(file, "Algorithm used: First come first served\n");
    }
    else if(schedulerID==2){
        fprintf(file, "Algorithm used: Priority\n");
    }
    else if(schedulerID==3){
        fprintf(file, "Algorithm used: Special\n");
    }
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

//print the report
void printRepo(int schedulerID, Job **acceptList, Job **rejectList, char **wList, int length, int t){
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
    if(schedulerID==0){ //sample
        fprintf(file, "Algorithm used: Sample\n");
    }
    else if(schedulerID==1){
        fprintf(file, "Algorithm used: First come first served\n");
    }
    else if(schedulerID==2){
        fprintf(file, "Algorithm used: Priority\n");
    }
    else if(schedulerID==3){
        fprintf(file, "Algorithm used: Special\n");
    }
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
