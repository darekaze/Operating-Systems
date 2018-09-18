#ifndef JOB_H_
#define JOB_H_

typedef struct Extra {
    char name[20];
    struct Extra *next;
} Extra;

typedef struct Job {
    int ssType;
    int date; 		// YYYYMMDD
    int startTime; 	// hr only
    int endTime; 	// hr only
    char owner[20];
    Extra *remark; 	// NULL by default
    struct Job *next;
} Job;

#endif // !JOB_H_
