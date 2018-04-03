/*rescheduler: 
对于meeting和gathering，允许参与者中途离开或中途进来，但需符合如下规则：
（注：活动持有者必须从头到尾参与该项活动中）
1. 若要中途离开，离开时间不应超过总时长的一半，否则不参与该活动
2. callee须超过一半*/

typedef struct extra {
    char name[20];
	char note[1000];
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
		char buf[80];
		memset(se, 0, sizeof(se));
		sprintf(se,"%s %s",dateToString(cur->date, cur->startTime),dateToString(cur->date, cur->endTime));
		int userId[20];
		userId[0]=findUser(cur->owner, users, userList);
		extra p=cur->remark;
		int j=1;
		while(p!=NULL){
			userId[j++]=findUser(p->name, users, userList);
			p=p->next;
		}
		write(cToG[userId[0]][1],se,80);
		read(gToC[userId[0]][0],buf,80);
		if(buf[0]=='N'){
			write(cToG[userId[0]][1],"N",3); //reject
			addToReject(reject, cur);
		} 
		else{
			int agree=0;
			int leaveTime[50];
			char info[50][80];
			
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
				extra last=NULL;
				extra now=cur->remark;
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
							extra temp=now->next;
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