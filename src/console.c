/*This code is part of the Epoch Init System.
* The Epoch Init System is maintained by Subsentient.
* This software is public domain.
* Please read the file UNLICENSE.TXT for more information.*/

/**This file is for console related stuff, like status reports and whatnot.
 * I can't see this file getting too big.**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "epoch.h"

/*The banner we show upon startup.*/
struct _BootBanner BootBanner = { false, { '\0' }, { '\0' } };
/*Should we Disable CTRL-ALT-DEL instant reboots?*/
Bool DisableCAD = true;
Bool AlignStatusReports = true;

void PrintBootBanner(void)
{ /*Real simple stuff.*/
	if (!BootBanner.ShowBanner)
	{
		return;
	}
	
	if (!strncmp(BootBanner.BannerText, "FILE", strlen("FILE")))
	{ /*Now we read the file and copy it into the new array.*/
		char *Worker, *TW;
		FILE *TempDescriptor;
		short TChar;
		unsigned long Inc = 0;
		
		BootBanner.BannerText[Inc] = '\0';	
		
		Worker = BootBanner.BannerText + strlen("FILE");
		
		for (; *Worker == ' ' || *Worker == '\t'; ++Worker);

		if ((TW = strstr(Worker, "\n")))
		{
			*TW = '\0';
		}
		
		if (!(TempDescriptor = fopen(Worker, "r")))
		{
			char TmpBuf[1024];
			
			snprintf(TmpBuf, 1024, "Failed to display boot banner, can't open file \"%s\".", Worker);
			SpitWarning(TmpBuf);
			return;
		}
		
		for (; (TChar = getc(TempDescriptor)) != EOF && Inc < MAX_LINE_SIZE - 1; ++Inc)
		{ /*It's a loop copy. Get over it.*/
			BootBanner.BannerText[Inc] = (char)TChar;
		}
		BootBanner.BannerText[Inc] = '\0';
		
		fclose(TempDescriptor);
	}
	
	if (*BootBanner.BannerColor)
	{
		printf("%s%s%s\n\n", BootBanner.BannerColor, BootBanner.BannerText, CONSOLE_ENDCOLOR);
	}
	else
	{
		printf("%s\n\n", BootBanner.BannerText);
	}
}

void SetBannerColor(const char *InChoice)
{
	if (!strcmp(InChoice, "BLACK"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_BLACK);
	}
	else if (!strcmp(InChoice, "BLUE"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_BLUE);
	}
	else if (!strcmp(InChoice, "RED"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_RED);
	}
	else if (!strcmp(InChoice, "GREEN"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_GREEN);
	}
	else if (!strcmp(InChoice, "YELLOW"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_YELLOW);
	}
	else if (!strcmp(InChoice, "MAGENTA"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_MAGENTA);
	}
	else if (!strcmp(InChoice, "CYAN"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_CYAN);
	}
	else if (!strcmp(InChoice, "WHITE"))
	{
		snprintf(BootBanner.BannerColor, 64, "%s", CONSOLE_COLOR_WHITE);
	}
	else
	{ /*Bad value? Warn and then set no color.*/
		char TmpBuf[1024];
		
		BootBanner.BannerColor[0] = '\0';
		snprintf(TmpBuf, 1024, "Bad color value \"%s\" specified for boot banner. Setting no color.", InChoice);
		SpitWarning(TmpBuf);
	}
}

/*Give this function the string you just printed, and it'll print a status report at the end of it, aligned to right.*/
void PerformStatusReport(const char *InStream, rStatus State, Bool WriteToLog)
{
	unsigned long StreamLength, Inc = 0;
	char OutMsg[8192] = { '\0' }, IP2[MAX_DESCRIPT_SIZE];
	char StatusFormat[64];
	struct winsize WSize;
	
	/*Get terminal width so we can adjust the status report.*/
    ioctl(0, TIOCGWINSZ, &WSize);
    
    if (AlignStatusReports)
    {
		StreamLength = WSize.ws_col;
    
		if (StreamLength >= sizeof OutMsg/2)
		{ /*Default to 80 if we get a very big number.*/
			StreamLength = 80;
		}
	}
	else
	{
		StreamLength = 1;
	}

	snprintf(IP2, sizeof IP2, "%s", InStream);
	
	switch (State)
	{
		case FAILURE:
		{
			snprintf(StatusFormat, sizeof StatusFormat, "[%s]\n", CONSOLE_COLOR_RED "FAILED" CONSOLE_ENDCOLOR);
			break;
		}
		case SUCCESS:
		{
			snprintf(StatusFormat, sizeof StatusFormat, "[%s]\n", CONSOLE_COLOR_GREEN "Done" CONSOLE_ENDCOLOR);
			break;
		}
		case WARNING:
		{
			snprintf(StatusFormat, sizeof StatusFormat, "[%s]\n", CONSOLE_COLOR_YELLOW "WARNING" CONSOLE_ENDCOLOR);
			break;
		}
		default:
		{
			SpitWarning("Bad parameter passed to PerformStatusReport() in console.c.");
			return;
		}
	}
	
	if (AlignStatusReports)
	{
		switch (State)
		{ /*Take our status reporting into account, but not with the color characters and newlines and stuff, 
			because that gives misleading results due to the extra characters that you can't see.*/
			case SUCCESS:
				StreamLength -= strlen("[Done]");
				break;
			case FAILURE:
				StreamLength -= strlen("[FAILED]");
				break;
			case WARNING:
				StreamLength -= strlen("[WARNING]");
				break;
			default:
				SpitWarning("Bad parameter passed to PerformStatusReport() in console.c");
				return;
		}

	
		if (strlen(IP2) >= StreamLength)
		{ /*Keep it aligned if we are printing a multi-line report.*/
			strncat(OutMsg, "\n", 2);
		}
		else
		{
			FILE *Descriptor = NULL;
			Bool Matches = true, JustABug = false;
			
			if (isatty(STDIN_FILENO))
			{
				char TTYName[MAX_LINE_SIZE];
				
				ttyname_r(STDIN_FILENO, TTYName, sizeof TTYName);
				
				if (strstr(TTYName, "/dev/pts") == NULL && (Descriptor = fopen("/dev/vcs", "r")) != NULL)
				{
					unsigned long Filesize = 0;
					char *FileBuf = NULL;
					char *Locator = NULL;
					
					/*Get the file size since nothing in those virtual filesystems
					 * has it in the filesystem.*/
					while (getc(Descriptor) != EOF) ++Filesize;
					rewind(Descriptor);
	
					FileBuf = malloc(Filesize + 1);
					
					fread(FileBuf, 1, Filesize, Descriptor);
					FileBuf[Filesize] = '\0';
					
					fclose(Descriptor);
					
					if (!(Locator = strstr(FileBuf, IP2)))
					{
						Matches = false;
						JustABug = true; /*This tells us "Don't add a space at the bottom,
										*We can't currently read the tty.*/
					}
					else
					{
						Locator += strlen(IP2);
						
						for (; *Locator != '\0'; ++Locator)
						{
							if (*Locator != ' ' && *Locator != '\t')
							{
								Matches = false;
								break;
							}
						}
					}
					free(FileBuf);
						
				}
			}
				
			if (Matches)
			{ /*Then we can go ahead and subtract the number of spaces.*/
				StreamLength -= strlen(IP2);
			}
			else
			{
				strncat(OutMsg, "\n", 1);
				if (!JustABug)
				{
					strncat(StatusFormat, "\n", 1);
				}
			}
		}
	}
	
	/*Appropriate spacing.*/
	for (; Inc < StreamLength; ++Inc)
	{
		strncat(OutMsg, " ", 2);
	}
	
	strncat(OutMsg, StatusFormat, strlen(StatusFormat));
	
	printf("%s", OutMsg);
	
	if (WriteToLog)
	{
		char TmpBuf[MAX_LINE_SIZE];
		char HMS[3][16], MDY[3][16];
		char TimeFormat[64];
		
		GetCurrentTime(HMS[0], HMS[1], HMS[2], MDY[0], MDY[1], MDY[2]);
		
		snprintf(TimeFormat, 64, "[%s:%s:%s | %s-%s-%s] ",
				HMS[0], HMS[1], HMS[2], MDY[0], MDY[1], MDY[2]);
		
		while (StatusFormat[strlen(StatusFormat) - 1] == '\n')
		{
			StatusFormat[strlen(StatusFormat) - 1] = '\0'; /*Get rid of the newline.*/
		}

		
		snprintf(TmpBuf, MAX_LINE_SIZE, "%s%s %s", TimeFormat, InStream, StatusFormat);
		WriteLogLine(TmpBuf, false);
	}
	
	return;
}

/*Three little error handling functions. Yay!*/
void SpitError(const char *INErr)
{
	char HMS[3][16], MDY[3][16];
	
	GetCurrentTime(HMS[0], HMS[1], HMS[2], MDY[0], MDY[1], MDY[2]);
	
	fprintf(stderr, "[%s:%s:%s | %s-%s-%s] " CONSOLE_COLOR_RED "Epoch: ERROR:\n" CONSOLE_ENDCOLOR "%s\n\n",
			HMS[0], HMS[1], HMS[2], MDY[0], MDY[1], MDY[2], INErr);
}

void SmallError(const char *INErr)
{
	fprintf(stderr, CONSOLE_COLOR_RED "* " CONSOLE_ENDCOLOR "%s\n", INErr);
}

void SpitWarning(const char *INWarning)
{
	char HMS[3][16], MDY[3][16];
	
	GetCurrentTime(HMS[0], HMS[1], HMS[2], MDY[0], MDY[1], MDY[2]);
	
	fprintf(stderr, "[%s:%s:%s | %s-%s-%s] " CONSOLE_COLOR_YELLOW "Epoch: WARNING:\n" CONSOLE_ENDCOLOR "%s\n\n",
			HMS[0], HMS[1], HMS[2], MDY[0], MDY[1], MDY[2], INWarning);
}
