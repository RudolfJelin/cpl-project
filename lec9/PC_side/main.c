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

#define BUFFER_SIZE 255

// array of strings for command menu
char * menu[] = 
	{
	"Item 'o': LED ON",
	"Item 'f': LED OFF",
	"Item 'b': BUTT:STATE",
	"Item 'm': Enter morse code",
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

pthread_mutex_t mtx;
pthread_cond_t condvar;

bool quit = false;
char * fileName = NULL;
char chBuffOut[BUFFER_SIZE];
char chBuffIn[BUFFER_SIZE];

// prints string as hexa
// unused function
void printBuffer(char * str, int len)
{
	for(int i = 0; i < len; i++)
	{
		printf(" %02X", str[i]);
	}
	printf("\n");
}

void printSelection(char * str)
{
	char strLine[] = "\rInfo:                          | Enter option: ";
	
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

// prints menu and scans selection to where the pointer is pointing
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

// serices comunication with Nucleo according to specifications
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
					//printBuffer(pSerialData->chCmdBuff, pSerialData->iCmdBuffLen);
					
					if((pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-2] == '\r') &&
					   (pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-1] == '\n'))
					{
						// command finished
						pSerialData->chCmdBuff[pSerialData->iCmdBuffLen-2] = 0;
						
						if(strstr(pSerialData->chCmdBuff, "Welcome to Nucleo") != NULL)
						{
							printSelection("Nucleo was reset\n");
						}
						else if(!strcmp(pSerialData->chCmdBuff, "BUTTON:PRESSED"))
						{
							printSelection("Button is down!\n");
						}
						else if(!strcmp(pSerialData->chCmdBuff, "BUTTON:RELEASED"))
						{
							printSelection("Button is up!\n");
						}
						else if (!strcmp(pSerialData->chCmdBuff, "Wrong command"))
						{
							printSelection("Unknown command.\n");
						}
						
						fflush(stdout);
						pSerialData->iCmdBuffLen = 0;
					}
				}
			}
		}
		
		q = quit;
	}
	//printf("\n");
	return 0;
}

// services Morse signals
// requests file, scans and evaluates. Errors are checked on the run
// if the command is activated while another instance is evaluating, 
// it will not interrupt the old one or create a duplicate
void* morse(void *v) 
{
	bool q = false;
	
	tSerialData * pSerialData;
	pSerialData = (tSerialData *)v;
	
	while(!q)
	{	
		pthread_mutex_lock(&mtx);
		// waits here
		pthread_cond_wait(&condvar, &mtx);
		if(fileName == NULL) // eg. thread to be terminated before filename obtained
		{
			return 0;
		}
		
		char chBuff[BUFFER_SIZE];
		FILE *fp;
		if ((fp = fopen(fileName, "r")) == NULL)
		{
			fprintf(stderr, "Error opening file!\n");
			return 0;
		}
		
		int i = 0;
		
		float onDelay = 0;
		float offDelay = 0;
		
		while(true)
		{
			char* result = fgets(chBuff, BUFFER_SIZE, fp);
			if (result == NULL){
				fprintf(stderr, "No morse code signals found!\n");
				return 0;
			}
		
			if(chBuff[0] == '#' || chBuff[0] == '\n')
			{
				continue;
			}
			if(strstr(chBuff, "sig_on=") != NULL)
			{
				sscanf(chBuff, "sig_on=%f", &onDelay);
			}
			else if(strstr(chBuff, "sigoff=") != NULL)
			{
				sscanf(chBuff, "sigoff=%f", &offDelay);
			}
			else
			{
				break;
			}
		}
		
		if(onDelay <= 0 || offDelay <= 0)
		{
			fprintf(stderr, "Ivalid morse signal times!\n");
			return 0;
		}
		
		while (!q) { // one char
			
			if(chBuff[i] == '\n' || chBuff[i] == -1)
			{
				break;
			}
			else if(chBuff[i] == '1' || chBuff[i] == '0')
			{
				// identical to normal led on command
				int iBuffOutSize = 0;
				sprintf(chBuffOut, "%s\r\n", LED_ON);
				iBuffOutSize = strlen(chBuffOut);
				serial_write(pSerialData->hSerial, chBuffOut, iBuffOutSize);
				
				if(chBuff[i] == '1')
				{
					usleep((int)(3*1000*1000*onDelay));
				}
				else
				{
					usleep((int)(1000*1000*onDelay));
				}
				
				iBuffOutSize = 0;
				sprintf(chBuffOut, "%s\r\n", LED_OFF);
				iBuffOutSize = strlen(chBuffOut);
				serial_write(pSerialData->hSerial, chBuffOut, iBuffOutSize);
				
				usleep((int)(1000*1000*offDelay));
			}
			else if(chBuff[i] == ' ')
			{
				usleep((int)(2*1000*1000*offDelay)); // one sleep already done, 2 left
			}
			else
			{
				fprintf(stderr, "Wrong morse file input!\n");
				return 0;
			}
			
			i++;
			q = quit;
		} // end morse char sending
		pthread_mutex_unlock(&mtx);
		fclose(fp);
		usleep(10);
	} // end main cycle
	return 0; // ends
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

int main(int argc, char *argv[]) {	
	// if no name given
	if(argc == 1){
		fprintf(stderr, "No commandline arguments given. Path to the serial interface unknown.\n");
		return -1;
	}
	printf("Path to the serial interface is \"%s\"\n", argv[1]);
	
	int hSerial = serial_init(argv[1]);
	printf("1\n");
	if (hSerial <= 0){ // could not establish connection
		return -1;
	}
	
	tSerialData oSerialData;
	oSerialData.hSerial = hSerial;
	pthread_create(&oSerialData.oCom, NULL, comm, (void *)&oSerialData);
	
	tSerialData mSerialData;
	mSerialData.hSerial = hSerial;
	pthread_create(&mSerialData.oCom, NULL, morse, (void *)&mSerialData);
	
	call_termios(0);
	
	char* selection = malloc(sizeof(char)); // allocates space for input char
	
	printMenu(selection);
	
	while(selection[0] != 'e') // e exits program by exiting loop. 
	{
		//printMenu(selection);
		
		*selection = getchar();
		
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
					
					/*memset(chBuffIn, '\0', BUFFER_SIZE);
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
					}*/
					break;
				}

			case 'm': // morse section
				{	
					char chFileName[BUFFER_SIZE];
					printf("\nEnter filename: ");
					scanf("%s", chFileName);
					fileName = &chFileName[0];
					printf("\n");
					
					pthread_cond_signal(&condvar);
					
					break;
				}

			case 'c': // sends a string from stdin followed by "\r\n".
				{	  // the nucleo will evaluate this as an unknown command
					  // if the string matches one of the previous commands, 
					  // it will be evaluated instead
					printf("Enter command: ");
					char command[BUFFER_SIZE];
					scanf("%s", command); // waits for string
					
					int iBuffOutSize = 0;
					sprintf(chBuffOut, "%s\r\n", command); //passes the custom string
					iBuffOutSize = strlen(chBuffOut);
					int n_written = serial_write(hSerial, chBuffOut, iBuffOutSize);
					
					usleep(1000*1000);
					
					/*memset(chBuffIn, '\0', BUFFER_SIZE);
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
					}*/
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
	} // END while()
	
	free(selection);
	pthread_join(oSerialData.oCom, NULL);
	pthread_cond_signal(&condvar);
	pthread_join(mSerialData.oCom, NULL);
	serial_close(hSerial);
	call_termios(1);
	return 1;
}	







