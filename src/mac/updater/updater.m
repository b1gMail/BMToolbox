// $Id: updater.m,v 1.1 2013/01/07 22:07:55 patrick Exp $

#include <stdio.h>
#include <unistd.h>

#import <AppKit/AppKit.h>

int main (int argc, const char * argv[])
{
	// check args
	if(argc != 3)
	{
		printf("Usage: %s [app-path] [process-id]\n",
			argv[0]);
		return(1);
	}
		
	// get arguments
	const char *myPath = argv[0], *appPath = argv[1];
	pid_t processID = atoi(argv[2]);
	
	// wait for parent process to terminate
	if(getppid() != 1)
	{
		ProcessSerialNumber sn;
		while(GetProcessForPID(processID, &sn) != procNotFound)
			sleep(1);
	}
	
	// relaunch application
	[[NSWorkspace sharedWorkspace] openFile:[[NSFileManager defaultManager] stringWithFileSystemRepresentation:appPath length:strlen(appPath)]];
	
	// delete myself
	unlink(myPath);
	
	return(0);
}
