#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <stdbool.h>

#include "commands.h"
#include "serial.h"
#include "list.h"

#define BUFFER_SIZE 255

// array of strings for command menu
char * menu[] = 
	{
	"Item 'o': LED ON",
	"Item 'f': LED OFF",
	"Item 'b': BUTT:STATE",
	"Item 'm': Send File",
	"Item c: Enter a custom command",
	"Item e: Exit"
	};

typedef struct tSerialData
{
	char chBuffIn[BUFFER_SIZE];
	int iBuffLen;
	
	char chCmdBuff[BUFFER_SIZE];
	int iCmdBuffLen;
	
	pthread_t oCom;
	int hSerial;
	
} tSerialData;

bool quit = false;
char fileName[BUFFER_SIZE];
char chBuffOut[BUFFER_SIZE];
char chBuffIn[BUFFER_SIZE];
int hSerial;

void printSelection(char * str)
{
	char strLine[] = "Enter option: ";//"\rInfo:                          | Enter option: ";
	
	if(str != NULL)
	{
		for(int i = 0; i < 23; i++)
		{
			if(isprint(str[i]))
			{
				strLine[i+7] = str[i];
			}
			else
			{
				break;
			}
		}
	}
	printf("\r%s\n", strLine); // unfortunately printing to stderr will not be nicely formatted
}

void printMenu(char* selection)
{
	printf("== program menu ==\n");
	
	int i;
	for(i = 0; i < sizeof(menu)/sizeof(char*); i++) // determines loop length with sizeof()
	{
		printf("%s\n", menu[i]);
	}
//	printf("Selection: "); //no newline on purpose
//	scanf("%s[0]", selection); // scanning using '%c' is not possible

	printSelection(NULL);
}

void send_string(int hSerial, char * strOut)
{
	printf("Preparing to send '%s'\n", strOut);

	sprintf(chBuffOut, "%s\r\n", strOut); //TODO /r/n removed
	//sprintf(chBuffOut, "*IDN?\r\n");
	
	printf("Sending: \"");
	char * hex = chBuffOut;
	//hex[strlen(hex)-1] = 0;
	while(*hex){
		printf("%02x ", *hex);
		serial_write(hSerial, hex, 1);
		usleep(1000*100); // this delay is crucial. For some reason, the serial connection will otherwise skip bytes
		hex++;
	}
	printf("\"\n");
}

// loads file line by line, then sends it. Can be reworked into a thread.
void send_file(int hSerial)
{
	printf("Sending file '%s'\n", fileName);

	listItem * firstItem;
	firstItem = LI_load(fileName);
	listItem * pTmp = firstItem;
	
	while(pTmp != NULL)
	{
		send_string(hSerial, pTmp->pLine);
		pTmp = pTmp->pNext;
	}
		
	LI_remove(firstItem);	
	fflush(stdout);//
}

void load_file(int hSerial){
	// loads entire file and sends it; TODO
	printf("\nEnter filename: ");
	scanf("%s", fileName);
	send_file(hSerial);
}

// a separate thread for recieving serial line data
void* comm(void *v) 
{
	bool q = false;
	int iRecv;
	
	tSerialData * pSerialData;
	pSerialData = (tSerialData *)v;
	memset(pSerialData->chBuffIn, '\0', BUFFER_SIZE);
	pSerialData->iBuffLen = 0;
	
	memset(pSerialData->chCmdBuff, '\0', BUFFER_SIZE);
	pSerialData->iCmdBuffLen = 0;
	
	while (!q) {
		
		iRecv = serial_read(pSerialData->hSerial, pSerialData->chBuffIn, BUFFER_SIZE);
		if (iRecv > 0)
		{
			for(int i = 0; i < iRecv; i++)
			{
				pSerialData->chCmdBuff[pSerialData->iCmdBuffLen] = pSerialData->chBuffIn[i];
				pSerialData->iCmdBuffLen++;
				if (pSerialData->iCmdBuffLen >= BUFFER_SIZE)
				{ // too large data
					pSerialData->iCmdBuffLen = 0;
				}
				
				if(pSerialData->iCmdBuffLen > 1)
				{
					if((pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-2] == '\r') &&
					   (pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-1] == '\n'))
					{
						// command finished
						pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-2] = 0;
						
						// output command
						printf("NUCLEO SAYS: \"%s\"\n", pSerialData->chCmdBuff);
						
						// checks for all possible nucleo outputs
						if(strstr(pSerialData->chCmdBuff, "ERROR") == pSerialData->chCmdBuff)
						{
							printf("Wrong command syntax!\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "UNKNOWN") == pSerialData->chCmdBuff)
						{
							printf("Unknown command\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_DOWN") == pSerialData->chCmdBuff)
						{
							printf("Nucleo requested LCD clear\n");
							send_string(pSerialData->hSerial, "DRAW:CLEAR 9");
							
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_SEL") == pSerialData->chCmdBuff)
						{
							printf("Nucleo requested resend\n");
							send_file(pSerialData->hSerial);
						}
						else if(strstr(pSerialData->chCmdBuff, "BUTTON 0") == pSerialData->chCmdBuff)
						{
							printf("Nucleo claims button is released\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "BUTTON 1") == pSerialData->chCmdBuff)
						{
							printf("Nucleo claims button is pressed\n");
						}
			
						fflush(stdout);
						pSerialData->iCmdBuffLen = 0;
					}
				}
			}
		}
		
		q = quit;
	}
	return 0;
}

// sets terminal functions
void call_termios(int reset)
{
	static struct termios tio, tioOld;
	tcgetattr(STDIN_FILENO, &tio);
	if (reset) 
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &tioOld);
	} 
	else 
	{
		tioOld = tio; //backupcfmakeraw(&tio);
		tio.c_lflag &= ~(ICANON); // I had to add this setting on my device, otherwise wouldnt evaluate after each char
		tio.c_oflag |= OPOST; // enable output postprocessing
		tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	}
}
// Initializes serial communication, loads filename & sends contents to Nucleo, sets up comm thread, then enters idle loop
int main(int argc, char *argv[]) {	
	// if no name given
	if(argc == 1){
		fprintf(stderr, "No commandline arguments given. Path to the serial interface unknown.\n");
		return -1;
	}
	printf("Path to the serial interface is \"%s\"\n", argv[1]);
	hSerial = serial_init(argv[1]);
	if (hSerial <= 0){ // could not establish connection
		//TODO return -1;
	}
	call_termios(0);
	//init nucleo's communication service thread
	tSerialData oSerialData;
	oSerialData.hSerial = hSerial;
	pthread_create(&oSerialData.oCom, NULL, comm, (void *)&oSerialData);
	// end of init block
	
	
	// start of main loop
	char* selection = malloc(sizeof(char)); // allocates space for input char
	//printMenu(selection);
	
	while(selection[0] != 'e') // e exits program by exiting loop. 
	{
		if(selection[0] != '\n'){
			printMenu(selection);
		}
		
		*selection = getchar();
		
		switch(*selection)
		{
			case 'o': // sends "LED ON\r\n"
				{
					send_string(hSerial, "LED ON");
					usleep(1000*1000);
					break;
				}

			case 'f': // sends "LED OFF\r\n". 
				{	
					send_string(hSerial, "LED OFF");
					usleep(1000*1000);
					break;
				}

			case 'b': // sends "BUTTON?". 
				{	
					send_string(hSerial, "BUTTON?");
					usleep(1000*1000);
					break;
				}

			case 'm': // load file prototype
				{	
					load_file(hSerial);
					break;
				}

			case 'c': // sends a string from stdin
				{	
					printf("Enter command: ");
					char command[BUFFER_SIZE];
					scanf("%s", command); // waits for string
					
					/*int iBuffOutSize = 0;
					sprintf(chBuffOut, "%s\r\n", command); //passes the custom string
					iBuffOutSize = strlen(chBuffOut);
					int n_written = serial_write(hSerial, chBuffOut, iBuffOutSize);
					*/
					send_string(hSerial, command);
					
					usleep(1000*1000);
					
					break;
				}

			case 'e':
				{	// waits for while condition to end loop
					// can't free memory until after end of loop
					quit = true;
					break;
				}
			case '\n': // does nothing
				{	
					break;
				}
			default:
				fprintf(stderr, "Wrong option\n"); // if wrong selection
				
		} // END switch
	} // END main loop
	free(selection);
	
	
	pthread_join(oSerialData.oCom, NULL);
	serial_close(hSerial);
	call_termios(1);
	return 1;
}	







