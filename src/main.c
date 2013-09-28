/*This code is part of the Epoch Init System.
* The Epoch Init System is maintained by Subsentient.
* This software is public domain.
* Please read the file LICENSE.TXT for more information.*/

/**CLI parsing, etc. main() is here.**/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/reboot.h>
#include "epoch.h"

#define ArgIs(z) !strcmp(CArg, z)
#define CmdIs(z) __CmdIs(argv[0], z)

/*Forward declarations for static functions.*/
static rStatus ProcessGenericHalt(int argc, char **argv);
static Bool __CmdIs(const char *CArg, const char *InCmd);
static void PrintEpochHelp(const char *RootCommand, const char *InCmd);
static rStatus HandleEpochCommand(int argc, char **argv);
static void SigHandler(int Signal);

/*
 * Actual functions.
 */
 
static Bool __CmdIs(const char *CArg, const char *InCmd)
{ /*Check if we are or end in the command name specified.*/
	const char *TWorker = CArg;
	
	if ((TWorker = strstr(CArg, InCmd)))
	{
		while (*TWorker != '\0') ++TWorker;
		TWorker -= strlen(InCmd);
		
		if (!strcmp(TWorker, InCmd))
		{
			return true;
		}
	}
			
	return false;
}

static void SigHandler(int Signal)
{
	const char *ErrorM = NULL;
	void *BTList[25];
	char **BTStrings;
	size_t BTSize;
	char OutMsg[MAX_LINE_SIZE * 2] = { '\0' }, *TWorker = OutMsg;
	
	switch (Signal)
	{
		case SIGINT:
		{
			static unsigned long LastKillAttempt = 0;
			
			if (getpid() == 1)
			{
				if ((LastKillAttempt == 0 || time(NULL) > (LastKillAttempt + 5)) && CurrentTaskPID != 0)
				{
					char MsgBuf[MAX_LINE_SIZE];
					
					snprintf(MsgBuf, sizeof MsgBuf, "\n%sKilling current task. Press CTRL-ALT-DEL again within 5 seconds to reboot.%s",
							CONSOLE_COLOR_YELLOW, CONSOLE_ENDCOLOR);
					puts(MsgBuf);
					fflush(NULL);
					
					WriteLogLine(MsgBuf, true);
					
					if (kill(CurrentTaskPID, SIGKILL) != 0)
					{
						snprintf(MsgBuf, sizeof MsgBuf, "%sUnable to kill task.%s", CONSOLE_COLOR_RED, CONSOLE_ENDCOLOR);
					}
					else
					{
						snprintf(MsgBuf, sizeof MsgBuf, "%sTask successfully killed.%s", CONSOLE_COLOR_GREEN, CONSOLE_ENDCOLOR);
					}
					puts(MsgBuf);
					fflush(stdout);
					
					WriteLogLine(MsgBuf, true);
					
					LastKillAttempt = time(NULL);
					return;
				}
				else
				{
					LaunchShutdown(OSCTL_LINUX_REBOOT);
				}
			}
			else
			{
				puts("SIGINT received. Exiting.");
				ShutdownMemBus(false);
				exit(0);
			}
		}
		case SIGSEGV:
		{
			ErrorM = "A segmentation fault has occurred in Epoch!";
			break;
		}
		case SIGILL:
		{
			ErrorM = "Epoch has encountered an illegal instruction!";
			break;
		}
		case SIGFPE:
		{
			ErrorM = "Epoch has encountered an arithmetic error!";
			break;
		}
		case SIGABRT:
		{
			ErrorM = "Epoch has received an abort signal!";
			break;
		}
		
	}
	
	BTSize = backtrace(BTList, 25);
	BTStrings = backtrace_symbols(BTList, BTSize);

	snprintf(OutMsg, sizeof OutMsg, "%s\n\nBacktrace:\n", ErrorM);
	TWorker += strlen(TWorker);
	
	for (; BTSize > 0 && *BTStrings != NULL; --BTSize, ++BTStrings, TWorker += strlen(TWorker))
	{
		snprintf(TWorker, sizeof OutMsg - strlen(OutMsg) - 1, "\n%s", *BTStrings);
	}
	
	if (getpid() == 1)
	{
		EmulWall(OutMsg, false);
		EmergencyShell();
	}
	else
	{
		SpitError(OutMsg);
		exit(1);
	}
}
	
static void PrintEpochHelp(const char *RootCommand, const char *InCmd)
{ /*Used for help for the epoch command.*/
	const char *HelpMsgs[] =
	{ 
		("[poweroff/halt/reboot]:\n\n"
		
		 "Enter poweroff, halt, or reboot to do the obvious."
		),
		
		( "[disable/enable] objectid:\n\n"
		  "Enter disable or enable followed by an object ID to disable or enable\nthat object."
		),
		
		( "[start/stop] objectid:\n\n"
		  "Enter start or stop followed by an object ID to start or stop that object."
		),
		
		( "objrl objectid [del/add/check] runlevel:\n\n"
		
		  "runlevel del and add do pretty much what it sounds like,\n"
		  "and check will tell you if that object is enabled for that runlevel."
		),
		  
		( "status objectid:\n\n"
		
		  "Enter status followed by an object ID to see if that object\nis currently started."
		),
		
		( "setcad [on/off]:\n\n"
		
		  "Sets Ctrl-Alt-Del instant reboot modes. If set to on, striking Ctrl-Alt-Del\n"
		  "at a console will instantly reboot the system without intervention by Epoch.\n"
		  "Otherwise, if set to off, Epoch will perform a normal reboot when Ctrl-Alt-Del\n"
		  "is pressed."
		),
			
		
		( "configreload:\n\n"
		
		  "Enter configreload to reload the configuration file epoch.conf.\nThis is useful for "
		  "when you change epoch.conf\n"
		  "to add or remove services, change runlevels, and more."
		)
	};
	
	enum { HCMD, ENDIS, STAP, OBJRL, STATUS, SETCAD, CONFRL };
	
	
	printf("%s\n\n", VERSIONSTRING);
	
	if (InCmd == NULL)
	{
		short Inc = 0;
		
		puts(CONSOLE_COLOR_RED "Printing all help.\n" CONSOLE_ENDCOLOR "----\n");
		
		for (; Inc <= CONFRL; ++Inc)
		{
			printf("%s %s\n%s----%s\n", RootCommand, HelpMsgs[Inc], CONSOLE_COLOR_RED, CONSOLE_ENDCOLOR);
		}
	}
	else if (!strcmp(InCmd, "poweroff") || !strcmp(InCmd, "halt") || !strcmp(InCmd, "reboot"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[HCMD]);
		return;
	}
	else if (!strcmp(InCmd, "disable") || !strcmp(InCmd, "enable"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[ENDIS]);
		return;
	}
	else if (!strcmp(InCmd, "start") || !strcmp(InCmd, "stop"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[STAP]);
		return;
	}
	else if (!strcmp(InCmd, "objrl"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[OBJRL]);
		return;
	}
	else if (!strcmp(InCmd, "status"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[STATUS]);
		return;
	}
	else if (!strcmp(InCmd, "setcad"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[SETCAD]);
		return;
	}
	else if (!strcmp(InCmd, "configreload"))
	{
		printf("%s %s\n\n", RootCommand, HelpMsgs[CONFRL]);
		return;
	}
	else
	{
		fprintf(stderr, "Unknown command name, \"%s\".\n", InCmd);
		return;
	}
}

static rStatus ProcessGenericHalt(int argc, char **argv)
{
	const char *CArg = argv[1];
	signed long OSCode = -1;
	
	/*Figure out what we are.*/
	if (CmdIs("poweroff") || CmdIs("halt") || CmdIs("reboot"))
	{
		char *GCode = NULL, *SuccessMsg = NULL, *FailMsg[2] = { NULL, NULL };
		
		if (CmdIs("poweroff"))
		{
			GCode = MEMBUS_CODE_POWEROFF;
			OSCode = OSCTL_LINUX_POWEROFF;
			SuccessMsg = "Power off in progress.";
			FailMsg[0] = "Failed to request immediate poweroff.";
			FailMsg[1] = "Failed to request poweroff.";
		}
		else if (CmdIs("reboot"))
		{
			GCode = MEMBUS_CODE_REBOOT;
			OSCode = OSCTL_LINUX_REBOOT;
			SuccessMsg = "Reboot in progress.";
			FailMsg[0] = "Failed to request immediate reboot.";
			FailMsg[1] = "Failed to request reboot.";
		}
		else if (CmdIs("halt"))
		{
			GCode = MEMBUS_CODE_HALT;
			OSCode = OSCTL_LINUX_HALT;
			SuccessMsg = "System halt in progress.";
			FailMsg[0] = "Failed to request immediate halt.";
			FailMsg[1] = "Failed to request halt.";
		}
		else
		{ /*Why are we called for a different task?*/
			SpitError("ProcessGenericHalt(): We are being called for a task"
					"other than shutdown procedures.\nThis is probably a bug. Please report.");
			return FAILURE;
		}
		
		
		if ((CArg = argv[1]))
		{
			if (argc == 2 && ArgIs("-f"))
			{
				sync();
				reboot(OSCode);
			}
			else
			{
				SpitError("Bad argument(s).");
				return FAILURE;
			}
		}
		else
		{
			if (!SendPowerControl(GCode))
			{
				SpitError(FailMsg[1]);
				return FAILURE;
			}
			else
			{
				printf("\n%s\n", SuccessMsg);
				fflush(NULL);
			}
			
		}
	}
	return SUCCESS;
}

static rStatus HandleEpochCommand(int argc, char **argv)
{
	const char *CArg = argv[1];
	
	/*Help parser and shutdown commands (for possible -f).*/
	if (argc >= 2)
	{
		if (ArgIs("help"))
		{
			if ((CArg = argv[2]))
			{
				PrintEpochHelp(argv[0], CArg);
			}
			else
			{
				PrintEpochHelp(argv[0], NULL);
			}
		
			return SUCCESS;
		}
		else if (ArgIs("poweroff") || ArgIs("reboot") || ArgIs("halt"))
		{
			Bool RVal;
			
			if (!InitMemBus(false))
			{
				SpitError("main(): Failed to connect to membus.");
				return FAILURE;
			}
			
			RVal = !ProcessGenericHalt(argc - 1, argv + 1);
			
			ShutdownMemBus(false);
			return (int)RVal;
		}
	}
	
	
	if (argc == 1)
	{
		PrintEpochHelp(argv[0], NULL);
		return SUCCESS;
	}
	/*One argument?*/
	else if (argc == 2)
	{
		CArg = argv[1];
		
		if (ArgIs("configreload"))
		{
			char TRecv[MEMBUS_SIZE/2 - 1];
			char TBuf[3][MAX_LINE_SIZE];

			if (!InitMemBus(false))
			{
				SpitError("main(): Failed to connect to membus.");
				return FAILURE;
			}

			if (!MemBus_Write(MEMBUS_CODE_RESET, false))
			{
				SpitError("Failed to write to membus.");
				ShutdownMemBus(false);
				return FAILURE;
			}
			
			while (!MemBus_Read(TRecv, false)) usleep(1000);
			
			snprintf(TBuf[0], sizeof TBuf[0], "%s %s", MEMBUS_CODE_ACKNOWLEDGED, MEMBUS_CODE_RESET);
			snprintf(TBuf[1], sizeof TBuf[1], "%s %s", MEMBUS_CODE_FAILURE, MEMBUS_CODE_RESET);
			snprintf(TBuf[2], sizeof TBuf[2], "%s %s", MEMBUS_CODE_BADPARAM, MEMBUS_CODE_RESET);
			
			if (!strcmp(TBuf[0], TRecv))
			{
				puts("Reload successful.");
				ShutdownMemBus(false);
				
				return SUCCESS;
			}
			else if (!strcmp(TBuf[1], TRecv))
			{
				puts("Reload failed!");
				ShutdownMemBus(false);
				
				return FAILURE;
			}
			else if (!strcmp(TBuf[2], TRecv))
			{
				SpitError("We are being told that MEMBUS_CODE_RESET is not a valid signal! Please report to Epoch.");
				ShutdownMemBus(false);
				
				return FAILURE;
			}
			else
			{
				SpitError("Unknown response received! Can't handle this! Report to Epoch please!");
				ShutdownMemBus(false);
				
				return FAILURE;
			}
		}
		
		else
		{
			fprintf(stderr, "Bad command %s.\n", CArg);
			PrintEpochHelp(argv[0], NULL);
			return FAILURE;
		}
	}
	else if (argc == 3)
	{
		CArg = argv[1];
		
		if (ArgIs("setcad"))
		{
			const char *MCode = NULL, *ReportLump = NULL;
			rStatus RetVal = SUCCESS;
			
			if (!InitMemBus(false))
			{
				SpitError("HandleEpochCommand(): Failed to connect to membus.");
				return FAILURE;
			}
			
			CArg = argv[2];
			
			if (ArgIs("on"))
			{
				MCode = MEMBUS_CODE_CADON;
				ReportLump = "enable";
			}
			else if (ArgIs("off"))
			{
				MCode = MEMBUS_CODE_CADOFF;
				ReportLump = "disable";
			}
			else
			{
				fprintf(stderr, "%s\n", "Bad parameter. Valid values are on and off.");
				return FAILURE;
			}
			
			if (SendPowerControl(MCode))
			{
				printf("Ctrl-Alt-Del instant reboot has been %s%c.\n", ReportLump, 'd');
				RetVal = SUCCESS;
			}
			else
			{
				fprintf(stderr, CONSOLE_COLOR_RED "Failed to %s Ctrl-Alt-Del instant reboot!\n" CONSOLE_ENDCOLOR, ReportLump);
				RetVal = FAILURE;
			}
			
			ShutdownMemBus(false);
			return RetVal;
		}
		if (ArgIs("enable") || ArgIs("disable"))
		{
			rStatus RV = SUCCESS;
			Bool Enabling = ArgIs("enable");
			char TOut[MAX_LINE_SIZE];
			
			if (!InitMemBus(false))
			{
				SpitError("main(): Failed to connect to membus.");
				return FAILURE;
			}
			
			CArg = argv[2];
			snprintf(TOut, sizeof TOut, (Enabling ? "Enabling %s" : "Disabling %s"), CArg);
			printf("%s", TOut);
			fflush(NULL);
			
			RV = ObjControl(CArg, (Enabling ? MEMBUS_CODE_OBJENABLE : MEMBUS_CODE_OBJDISABLE));
			PerformStatusReport(TOut, RV, false);
			
			ShutdownMemBus(false);
			return !RV;
		}
		else if (ArgIs("start") || ArgIs("stop"))
		{
			rStatus RV = SUCCESS;
			Bool Starting = ArgIs("start");
			char TOut[MAX_LINE_SIZE];
			
			if (!InitMemBus(false))
			{
				SpitError("main(): Failed to connect to membus.");
				return FAILURE;
			}
			
			CArg = argv[2];
			snprintf(TOut, sizeof TOut, (Starting ? "Starting %s" : "Stopping %s"), CArg);
			printf("%s", TOut);
			fflush(NULL);
			
			RV = ObjControl(CArg, (Starting ? MEMBUS_CODE_OBJSTART : MEMBUS_CODE_OBJSTOP));
			PerformStatusReport(TOut, RV, false);
			
			ShutdownMemBus(false);
			return !RV;
		}
		else if (ArgIs("status"))
		{
			Bool Started, Running, Enabled;
			Trinity InVal;
			
			if (!InitMemBus(false))
			{
				SpitError("main(): Failed to connect to membus.");
				return FAILURE;
			}
			
			CArg = argv[2];
			
			InVal = AskObjectStatus(CArg);
			
			if (!InVal.Flag)
			{
				printf("Unable to retrieve status of object %s. Does it exist?\n", CArg);
				ShutdownMemBus(false);
				return SUCCESS;
			}
			else if (InVal.Flag == -1)
			{
				SpitError("HandleEpochCommand(): Internal error retrieving status via membus.");
				ShutdownMemBus(false);
				return FAILURE;
			}
			else
			{
				Started = InVal.Val1;
				Running = InVal.Val2;
				Enabled = InVal.Val3;
			}
			
			printf("Status for object %s:\n---\nEnabled on boot: %s\nStarted: %s\nRunning: %s\n", CArg, /*This bit is kinda weird I think, but the output is pretty.*/
					(Enabled ? CONSOLE_COLOR_GREEN "Yes" CONSOLE_ENDCOLOR : CONSOLE_COLOR_RED "No" CONSOLE_ENDCOLOR),
					(Started ? CONSOLE_COLOR_GREEN "Yes" CONSOLE_ENDCOLOR : CONSOLE_COLOR_RED "No" CONSOLE_ENDCOLOR),
					(Running ? CONSOLE_COLOR_GREEN "Yes" CONSOLE_ENDCOLOR  : CONSOLE_COLOR_RED "No" CONSOLE_ENDCOLOR));

			ShutdownMemBus(false);
			return SUCCESS;
		}
		else
		{
			fprintf(stderr, "Bad command %s.\n", argv[1]);
			PrintEpochHelp(argv[0], NULL);
			return FAILURE;
		}
	}
	else if (argc == 5)
	{
		if (ArgIs("objrl"))
		{
			const char *ObjectID = argv[2], *RL = argv[4];
			char OBuf[MEMBUS_SIZE/2 - 1];
			char IBuf[MEMBUS_SIZE/2 - 1];
			rStatus ExitStatus = SUCCESS;
			
			if (!InitMemBus(false))
			{
				SpitError("main(): Failed to connect to membus.");
				return FAILURE;
			}
			
			CArg = argv[3];
			
			if (ArgIs("add"))
			{
				snprintf(OBuf, sizeof OBuf, "%s %s %s", MEMBUS_CODE_OBJRLS_ADD, ObjectID, RL);
			}
			else if (ArgIs("del"))
			{
				snprintf(OBuf, sizeof OBuf, "%s %s %s", MEMBUS_CODE_OBJRLS_DEL, ObjectID, RL);
			}
			else if (ArgIs("check"))
			{
				snprintf(OBuf, sizeof OBuf, "%s %s %s", MEMBUS_CODE_OBJRLS_CHECK, ObjectID, RL);
			}
			else
			{
				fprintf(stderr, "Invalid runlevel option %s.\n", CArg);
				ShutdownMemBus(false);
				return FAILURE;
			}
			
			if (!MemBus_Write(OBuf, false))
			{
				SpitError("Failed to write to membus.");
				ShutdownMemBus(false);
				return FAILURE;
			}
			
			while (!MemBus_Read(IBuf, false)) usleep(1000);
			
			if (ArgIs("add") || ArgIs("del"))
			{	
				char PossibleResponses[3][MEMBUS_SIZE/2 - 1];
				
				snprintf(PossibleResponses[0], sizeof PossibleResponses[0], "%s %s %s %s", MEMBUS_CODE_ACKNOWLEDGED,
						(ArgIs("add") ? MEMBUS_CODE_OBJRLS_ADD : MEMBUS_CODE_OBJRLS_DEL), ObjectID, RL);
				
				snprintf(PossibleResponses[1], sizeof PossibleResponses[1], "%s %s %s %s", MEMBUS_CODE_FAILURE,
						(ArgIs("add") ? MEMBUS_CODE_OBJRLS_ADD : MEMBUS_CODE_OBJRLS_DEL), ObjectID, RL);
						
				snprintf(PossibleResponses[2], sizeof PossibleResponses[2], "%s %s", MEMBUS_CODE_BADPARAM, OBuf);
				
				if (!strcmp(PossibleResponses[0], IBuf))
				{
					char *PSFormat[2] = { "Object %s added to runlevel %s\n", "Object %s deleted from runlevel %s\n" };
					printf(PSFormat[(ArgIs("add") ? 0 : 1)], ObjectID, RL);
				}
				else if (!strcmp(PossibleResponses[1], IBuf))
				{
					char *PSFormat[2] = { "Unable to add %s to runlevel %s!\n", "Unable to remove %s from runlevel %s!\n" };
					
					fprintf(stderr, PSFormat[(ArgIs("add") ? 0 : 1)], ObjectID, RL);
					ExitStatus = FAILURE;
				}
				else if (!strcmp(PossibleResponses[2], IBuf))
				{
					SpitError("Internal membus error, received BADPARAM upon your request. Please report to Epoch.");
					ExitStatus = FAILURE;
				}
				else
				{
					SpitError("Received unrecognized or corrupted response via membus! Please report to Epoch.");
					ExitStatus = FAILURE;
				}
				
				ShutdownMemBus(false);
				return ExitStatus;
			}
			else if (ArgIs("check"))
			{
				char PossibleResponses[3][MEMBUS_SIZE/2 - 1];
		
				snprintf(PossibleResponses[0], sizeof PossibleResponses[0], "%s %s %s ", MEMBUS_CODE_OBJRLS_CHECK, ObjectID, RL);
				snprintf(PossibleResponses[1], sizeof PossibleResponses[1], "%s %s", MEMBUS_CODE_FAILURE, OBuf);
				snprintf(PossibleResponses[2], sizeof PossibleResponses[2], "%s %s", MEMBUS_CODE_BADPARAM, OBuf);
			
				if (!strncmp(PossibleResponses[0], IBuf, strlen(PossibleResponses[0])))
				{
					char CNumber[2] = { '\0', '\0' };
					const char *CNS = IBuf + strlen(PossibleResponses[0]);
					
					CNumber[0] = *CNS;
					
					if (*CNumber == '0')
					{
						printf(CONSOLE_COLOR_RED "Object %s is NOT enabled for runlevel %s.\n" CONSOLE_ENDCOLOR,
								ObjectID, RL);
					}
					else if (*CNumber == '1')
					{
						printf(CONSOLE_COLOR_GREEN "Object %s is enabled for runlevel %s.\n" CONSOLE_ENDCOLOR,
								ObjectID, RL);
					}
					else
					{
						SpitError("Internal error, bad status number received from membus. Please report to Epoch.");
						ExitStatus = FAILURE;
					}
					ShutdownMemBus(false);
					
					return ExitStatus;
				}
				else if (!strcmp(PossibleResponses[1], IBuf))
				{
					printf("Unable to determine if object %s belongs to runlevel %s. Does it exist?\n", ObjectID, RL);
					ShutdownMemBus(false);
					return FAILURE;
				}
				else if (!strcmp(PossibleResponses[2], IBuf))
				{
					SpitError("We are being told that we sent a bad signal over the membus. "
								"This is a bug, please report to epoch.");
					ShutdownMemBus(false);
					return FAILURE;
				}
				else
				{
					SpitError("Received unrecognized or corrupted response via membus! Please report to Epoch.");
					ShutdownMemBus(false);
					return FAILURE;
				}
			}
		}
		else
		{
			fprintf(stderr, "Bad command %s.\n", CArg);
			PrintEpochHelp(argv[0], NULL);
			ShutdownMemBus(false);
			return FAILURE;
		}
	}
	else
	{
		fprintf(stderr, "%s\n", "Invalid usage.");
		PrintEpochHelp(argv[0], NULL);
		return FAILURE;
	}
	
	return SUCCESS;
}

#ifndef NOMAINFUNC
int main(int argc, char **argv)
{ /*Lotsa sloppy CLI processing here.*/
	/*Figure out what we are.*/

	/*Set up signal handling.*/
	signal(SIGSEGV, SigHandler);
	signal(SIGILL, SigHandler);
	signal(SIGFPE, SigHandler);
	signal(SIGABRT, SigHandler);
	signal(SIGINT, SigHandler); /*For reboots and closing client membus correctly.*/
	
	if (argv[0] == NULL)
	{
		SpitError("main(): argv[0] is NULL. Why?");
		return 1;
	}
	
	if (CmdIs("poweroff") || CmdIs("reboot") || CmdIs("halt"))
	{
		Bool RVal;
		
		/*Start membus.*/
		if (argc == 1 && !InitMemBus(false))
		{ /*Don't initialize the membus if we could be doing "-f".*/
			SpitError("main(): Failed to connect to membus.");
			return 1;
		}
		
		RVal = !ProcessGenericHalt(argc, argv);
		
		ShutdownMemBus(false);
		
		return (int)RVal;
	}
	else if (CmdIs("epoch")) /*Our main management program.*/
	{	
		HandleEpochCommand(argc, argv);
	}
	else if (CmdIs("init"))
	{ /*This is a bit long winded here, however, it's better than devoting a function for it.*/
		if (getpid() == 1)
		{ /*Just us, as init. That means, begin bootup.*/

				LaunchBootup();
		}
		else if (argc == 2)
		{
			char TmpBuf[MEMBUS_SIZE/2 - 1];
			char MembusResponse[MEMBUS_SIZE/2 - 1];
			char PossibleResponses[3][MEMBUS_SIZE/2 - 1];
			
			if (strlen(argv[1]) >= (MEMBUS_SIZE/2 - 1))
			{
				SpitError("Runlevel name too long. Please specify a runlevel with a sane name.");
				return 1;
			}
			
			snprintf(TmpBuf, sizeof TmpBuf, "%s %s", MEMBUS_CODE_RUNLEVEL, argv[1]);
			snprintf(PossibleResponses[0], sizeof PossibleResponses[0], "%s %s", MEMBUS_CODE_ACKNOWLEDGED, TmpBuf);
			snprintf(PossibleResponses[1], sizeof PossibleResponses[1], "%s %s", MEMBUS_CODE_FAILURE, TmpBuf);
			snprintf(PossibleResponses[2], sizeof PossibleResponses[2], "%s %s", MEMBUS_CODE_BADPARAM, TmpBuf);
			
			if (!InitMemBus(false))
			{
				SpitError("Failed to communicate with Epoch init, membus is down.");
				return 1;
			}
			
			if (!MemBus_Write(TmpBuf, false))
			{				
				SpitError("Failed to change runlevels, failed to write to membus after establishing connection.\n"
							"Is Epoch the running boot system?");		
				ShutdownMemBus(false);
				
				return 1;
			}
			
			while (!MemBus_Read(MembusResponse, false)) usleep(1000);
			
			if (!strcmp(MembusResponse, PossibleResponses[0]))
			{
				ShutdownMemBus(false);
				
				return 0;
			}
			else if (!strcmp(MembusResponse, PossibleResponses[1]))
			{
				printf("Failed to change runlevel to \"%s\".\n", argv[1]);
				ShutdownMemBus(false);
				
				return 1;
			}
			else if (!strcmp(MembusResponse, PossibleResponses[2]))
			{
				ShutdownMemBus(false);
				SpitError("We are being told that MEMBUS_CODE_RUNLEVEL is not understood.\n"
						"This is bad. Please report to Epoch.");
				
				return 1;
			}
			else
			{
				SpitError("Invalid response provided over membus.");
				ShutdownMemBus(false);
				
				return 1;
			}
		}
		else
		{
			printf("%s", "Too many arguments. Specify one argument to set the runlevel.\n");
			return 1;
		}
	}
	else if (CmdIs("killall5"))
	{
		const char *CArg = argv[1];
		
		if (argc == 2)
		{
			if (*CArg == '-')
			{
				++CArg;
			}
			
			if (AllNumeric(CArg))
			{
				return !EmulKillall5(atoi(CArg));
			}
			else
			{
				SpitError("Bad signal number. Please specify an integer signal number.\nPass no arguments to assume signal 15.");
				
				return 1;
			}
		}
		else if (argc == 1)
		{
			return !EmulKillall5(SIGTERM);
		}
		else
		{
			SpitError("Too many arguments. Syntax is killall5 -signum where signum is the integer signal number to send.");
			return 1;
		}
		
	}
	else if (CmdIs("wall"))
	{
		if (argc == 2)
		{
			EmulWall(argv[1], true);
			return 0;
		}
		else if (argc == 3 && !strcmp(argv[1], "-n"))
		{
			EmulWall(argv[2], false);
			return 0;
		}
		else
		{
			puts("Usage: wall [-n] message");
			return 1;
		}
	}
	else if (CmdIs("shutdown"))
	{
		return !EmulShutdown(argc, (const char**)argv);

	}
	else
	{
		SpitError("Unrecognized applet name.");
		return 1;
	}
	
	return 0;
}
#endif
