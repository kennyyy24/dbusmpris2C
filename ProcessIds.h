//#############################################################################
//
//                       MPRIS2 Controller (through DBus)
//        Vincent Crocher - 2015 - 2016
//
//#############################################################################
#ifndef PROCESSIDS_H_INCLUDED
#define PROCESSIDS_H_INCLUDED

//! Get the process ID based on process name and instance number (0 for first, 1 for second...)
//! /return Process id if exists
//! /return -1 otherwise
int GetProcessPID(const char* p_name, int nb)
{
    //Check paramater acceptable values (no more than 10 allowed, should be enough...)
    if(nb<0 || nb>10)
        return -1;

    //Ask for PIDs of processes
    int pids[10];
    char cmd[1024];
    sprintf(cmd, "pgrep %s", p_name);
	FILE * get_pid = popen(cmd, "r");
	int i=-1;
	while(feof(get_pid)==0 && i<10)
	{
		i++;
		fscanf(get_pid, "%d", &pids[i]);
	}
	pclose(get_pid);

	//If requested process instance does not exist
	if(i<nb+1)
        return -1;
    else
        return pids[nb];
}

#endif // PROCESSIDS_H_INCLUDED
