#ifndef UTIL_H_
#define UTIL_H_

#include "job.h"

void debug_print        (Job*);
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

#endif // !UTIL_H_
