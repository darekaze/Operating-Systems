#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SZ 128
#define CLASSES 1
#define MEETING 2
#define GATHERING 3

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
	return -1; // if the input type cannot be recognized
}

void addSession(int argc, char **argv, Job **head_ref, int t) {
    Job *temp, *newJob = (Job*)malloc(sizeof(Job));

    // TODO: add check users and other format
    if (newJob == NULL) {
        printf("Error: Out of memory..");
        exit(1);
    }
    if(argc < 5) {
        printf("Error: Not enough argument...\n");
        return;
    }
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

void addBatchFile(char *fname, Job **jobList) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    fp = fopen(fname,"r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
        
    while (getline(&line, &len, fp) != -1) {
        char **chunks = malloc(sizeof(char*) * 1);
        char *p;
        int l, t;
        
        if ((strlen(line) > 0) && (line[strlen(line) - 1] == '\n'))
            line[strlen(line) - 1] = '\0';
        p = strtok(line, " ");
        l = splitString(chunks, p);
        t = checkType(chunks[0]);
        printf("%s\n", chunks[0]);
        addSession(l, chunks, jobList, t);
        free(chunks);
    }
    fclose(fp);
    if (line) free(line);
}

/*Note: handleCmd can be deleted*/
void handleCmd(char (*cmd), Job **jobList, int *loop) {
    char **wList = malloc(sizeof(char*) * 1);
    char *p = strtok(cmd, " ");
    int t, l;

    l = splitString(wList, p);
    t = checkType(wList[0]);
    switch(t) { // TODO: implement functions
        case 1: case 2: case 3:
            addSession(l, wList, jobList, t);
            break;
        case 4:
            addBatchFile(wList[1], jobList);
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

/*new function begins*/
/*can be continued.... such as 1. the owner name is not in the list 2. The char is required for the digit bit */
int checkValid(char (*cmd)) {
	char **wList;
	*wList = (char *)malloc(sizeof(char*) * 1);
	char *p = strtok(cmd, " ");
	int t, l;
	
	l = splitString(wList, p);
	t = checkType(wList[0]);
	
	switch(t) {
		case 0: 
			return -1;
		case 1:
			if(l!=5 || strlen(wList[2])!=8 || strlen(wList[3])!=4) {
				printf("Error: Unrecognised Format \n");
				printf("Usage: \n    addClass xxx YYYYMMDD hhmm n\n");
				return 0;
			}
			return 1;
		case 2:
			if(l<5 || strlen(wList[2])!=8 || strlen(wList[3])!=4) {
				printf("Error: Unrecognised Format \n");
				printf("Usage: \n    addMeeting xxx YYYYMMDD hhmm n xxx xxx ...\n");
				return 0;
			}
			return 1;
		case 3:
			if(l<5 || strlen(wList[2])!=8 || strlen(wList[3])!=4) {
				printf("Error: Unrecognised Format \n");
				printf("Usage: \n    addGathering xxx YYYYMMDD hhmm n xxx xxx ...\n");
				return 0;
			}
			return 1;
		case 4: 
			if(l!=2) {
				printf("Error: Unrecognised Format \n");
				printf("Usage: \n    addBatch [filename]\n");
				return 0;
			}
			return 1;
		case 5:
			if(l!=4 || (strcmp(wList[2],"fcfs")!=0 && strcmp(wList[2],"pr")!=0 && strcmp(wList[2],"sjf")!=0)) {
				printf("Error: Unrecognised Format \n");
				printf("Usage: \n    printSchd xxx [fcfs/pr/sjf] [filename]\n");
				return 0;
			}
			return 1;
		case 6: 
			if(l!=2) {
				printf("Error: Unrecognised Format \n");
				printf("Usage: \n    printSchd [filename]\n");
				return 0;
			}
			return 1;
		default: 
			printf("Error: Unrecognised Command \n");
			return 0;
	}
}
/*new function ends*/

void debug_print(Job *head) {
    printf("Debug-log\n");
    while(head) {
        printf("%d %s %d %d %d\n", 
            head->ssType, head->owner, head->date, head->startTime, head->endTime);
        head = head->next;
    }
    printf("\n");
}


/*-------Main-------*/
int main(int argc, char *argv[]) {
	/*Reference: child 0 means FCFS, child 1 means priority, child 2 means SJF*/
    int loop = 1;
    char cmd[MAX_INPUT_SZ];
    Job *jobList = NULL;
    readUsers(--argc, ++argv); // Initialize
	
     /*new part begin*/
     /*-----build scheduler (Children Part)-----*/
    int cToP[5][2], pToC[5][2];
    int i;
    for(i = 0; i < 3; i++) {
		int pid;
		if (pipe(cToP[i])<0 || pipe(pToC[i])<0) {
			printf("Pipe creation error. \n");
			exit(1);
		}
		pid = fork();
		if (pid == 0) {
			close(pToC[i][1]);
			close(cToP[i][0]);
			//To Do: The function of each schdule
			exit(0);
		}
	}
	/*new part ends*/
	
	/*-----Parent Part-----*/
	for (i = 0; i < 3; i++) {
		close(pToC[i][0]);
		close(cToP[i][1]);
	}
    while(loop) {
        readInput(cmd);
        //handleCmd(cmd, &jobList, &loop);
    }
    printf("-> Bye!!!!!!\n");
    // Debug
    debug_print(jobList);
    free(jobList);
    return 0;
}
