#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>


#include "commands.h"
#include "serial.h"

#define BUFFER_SIZE 255

// array of strings for command menu
//set size 32 can be changed without affecting loop length calculations
char * menu[] = 
	{
	"Item 'o': LED ON",
	"Item 'f': LED OFF",
	"Item 'b': BUTT:STATE",
	"Item 'w': read buffer",
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
char chBuffOut[BUFFER_SIZE];
char chBuffIn[BUFFER_SIZE];

// prints menu and scans selection to where the pointer is pointing
void printMenu(char* selection)
{
	printf("== program menu ==\n");
	
	int i;
	for(i = 0; i < sizeof(menu)/sizeof(char*); i++) // determines loop length with sizeof()
	{
		printf("%s\n", menu[i]);
	}
	printf("Selection: "); //no newline on purpose
	scanf("%s[0]", selection); // scanning using '%c' is not possible
}

void printBuffer(char * str)
{
	while(str != 0)
	{
		printf(" %02X", *str);
		str++;
	}
	printf("\n");
}

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
					printBuffer(pSerialData->chCmdBuff);
					
					if((pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-2] == '\r') &&
					   (pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-1] == '\n'))
					{
						// command finished
						pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-2] = 0;
						
						if(strstr(pSerialData->chCmdBuff, "Welcome to Nucleo") != NULL)
						{
							printf("Nucleo was reset\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "BUTTON:PRESSED") != NULL)
						{
							printf("Nucleo claims the button is down!\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "BUTTON:RELEASED") != NULL)
						{
							printf("Nucleo claims the button is up!\n");
						}
						else if (strstr(pSerialData->chCmdBuff, "Wrong command") != NULL)
						{
							printf("Nucleo claims it does not know the command.\n");
						}
						
						pSerialData->iCmdBuffLen = 0;
					}
				}
			}
		}
		
		q = quit;
	
		/*pthread_mutex_lock(&mtx);
		pthread_cond_wait(&condvar, &mtx);
		printf("\rCounter %10i", counter);
		fflush(stdout);
		q = quit;
		pthread_mutex_unlock(&mtx);*/
	}
	//printf("\n");
	return 0;
}

int main(int argc, char *argv[]) {	
	// if no name given
	if(argc == 1){
		fprintf(stderr, "No commandline arguments given. Path to the serial interface unknown.\n");
		return -1;
	}
	printf("Path to the serial interface is \"%s\"\n", argv[1]);
	
	int hSerial = serial_init(argv[1]);
	printf("1\n");
	if (hSerial <= 0){
		printf(":(\n");
		//return -1; //TODO
	}
	
	tSerialData oSerialData;
	oSerialData.hSerial = hSerial;
	pthread_create(&oSerialData.oCom, NULL, comm, (void *)&oSerialData);
	
	char* selection = malloc(sizeof(char)); // allocates space for input char
	
	while(selection[0] != 'e') // e exits program by exiting loop. 
	{
		printMenu(selection); //prints menu and scans input into the memory location specified by the pointer
		
		switch(*selection)
		{
			case 'o': // sends "LED ON\r\n". This will be evaulated as a command to switch LD2 on.
				{
					int iBuffOutSize = 0;
					sprintf(chBuffOut, "%s\r\n", LED_ON);
					iBuffOutSize = strlen(chBuffOut);
					int n_written = serial_write(hSerial, chBuffOut, iBuffOutSize);
					break;
				}

			case 'f': // sends "LED OFF\r\n". 
				{	 // This will be evaulated as a command to switch LD2 off.
					int iBuffOutSize = 0;
					sprintf(chBuffOut, "%s\r\n", LED_OFF);
					iBuffOutSize = strlen(chBuffOut);
					int n_written = serial_write(hSerial, chBuffOut, iBuffOutSize);
					break;
				}

			case 'b': // sends "BUTT:STATUS?". 
				{	 // This will be evaulated as a command to send back the state of the button.
					int iBuffOutSize = 0;
					sprintf(chBuffOut, "%s\r\n", BUT_STAT);
					iBuffOutSize = strlen(chBuffOut);
					int n_written = serial_write(hSerial, chBuffOut, iBuffOutSize);
					
					usleep(1000*1000);
					
					memset(chBuffIn, '\0', BUFFER_SIZE);
					int n_read = serial_read(hSerial, chBuffIn, BUFFER_SIZE);
					//print("R: %s\n", chBuffIn);
					
					if(chBuffIn[n_read-2] == '\r' &&
					   chBuffIn[n_read-1] == '\n')
					{
						chBuffIn[n_read-2] = 0;
						if(strcmp("BUTTON:PRESSED", chBuffIn) == 0){
							printf("Nucleo claims the button is down!\n");
						}
						else if(strcmp("BUTTON:RELEASED", chBuffIn) == 0){
							printf("Nucleo claims the button is up!\n");
						}
						// TODO: 
					}
					break;
				}

			case 'w': // w 
				{	 // just read
					memset(chBuffIn, '\0', BUFFER_SIZE);
					int n_read = serial_read(hSerial, chBuffIn, BUFFER_SIZE);
					//print("R: %s\n", chBuffIn);
					
					break;
				}

			case 'c': // sends a string from stdin followed by "\r\n".
				{	  // the nucleo will evaluate this as an unknown command
					  // if the string matches one of the previous 3 commands, 
					  // it will be evaluated instead
					printf("Enter command: ");
					char command[255];
					scanf("%s", command); // waits for string
					
					int iBuffOutSize = 0;
					sprintf(chBuffOut, "%s\r\n", command); //passes the custom string
					iBuffOutSize = strlen(chBuffOut);
					int n_written = serial_write(hSerial, chBuffOut, iBuffOutSize);
					
					usleep(1000*1000);
					
					memset(chBuffIn, '\0', BUFFER_SIZE);
					int n_read = serial_read(hSerial, chBuffIn, BUFFER_SIZE);
					
					if(chBuffIn[n_read-2] == '\r' &&
					   chBuffIn[n_read-1] == '\n')
					{
						chBuffIn[n_read-2] = 0;
						if(strcmp("BUTTON:PRESSED", chBuffIn) == 0){ 
							// this can be the case if the custom command sent is "BUTT:STATUS?"
							printf("Nucleo claims the button is down!\n");
						}
						else if(strcmp("BUTTON:RELEASED", chBuffIn) == 0){
							printf("Nucleo claims the button is up!\n");
						}
						else if(strcmp("Wrong command", chBuffIn) == 0){
							printf("Nucleo claims it does not know the command.\n");
						} // if none of these occured, the sent command was 
						  // probably a LED operation which returns nothing
					}
					break;
				}

			case 'e':
				{	// waits for while condition to end loop
					// can't free memory until after end of loop
					quit = true;
					break;
				}
			default:
				fprintf(stderr, "Wrong option\n"); // if wrong selection
				
		} // END switch
	} // END while()
	
	free(selection); // frees all mallocated memory
	pthread_join(oSerialData.oCom, NULL);
	serial_close(hSerial);
	return 1;
}	



