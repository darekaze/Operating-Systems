-- Personal Organizer (PO) --

To run the program, follow these commands: 
gcc PO.c -o PO
./PO amy tom simon yan kate

Function:
addClass [name] [YYYYMMDD] [HH00] [duration]
addMeeting [name] [YYYYMMDD] [HH00] [duration] (participants...)
addGathering [name] [YYYYMMDD] [HH00] [duration] (participants...)
addClass [file-name]
printSchd [name] [fcfs/pr/special] [file-name]
printReport [file-name]

If you want a time table with rescheduling funcion, here is the commands
printSchd [name] [fcfs/pr/special] [file-name] reschedule
printReport [file-name] reschedule