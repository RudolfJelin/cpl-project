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
char * fileName;
char chBuffOut[BUFFER_SIZE];
char chBuffIn[BUFFER_SIZE];


// loads file line by line, then sends it. Can be reworked into a thread.
void send_file(int hSerial)
{
	listItem * firstItem;
	firstItem = LI_load(fileName);
	listItem * pTmp = firstItem;
	
	while(pTmp != NULL)
	{
		sprintf(chBuffOut, "%s\r\n", pTmp->pLine); //TODO /r/n removed
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
		
		//serial_write(hSerial, chBuffOut, 1);
		//serial_write(hSerial, "*IDN?", 4);
		pTmp = pTmp->pNext;
	}
		
	LI_remove(firstItem);	
	fflush(stdout);//
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
							int LCD_clear_size = 15;
							char LCD_clear[] = "DRAW:CLEAR 9\r\n";
							serial_write(pSerialData->hSerial, LCD_clear, LCD_clear_size);
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
	
	int hSerial = serial_init(argv[1]);
	if (hSerial <= 0){ // could not establish connection
		//TODO return -1;
	}
	
	call_termios(0);
	
	//init nucleo's communication service thread
	tSerialData oSerialData;
	oSerialData.hSerial = hSerial;
	pthread_create(&oSerialData.oCom, NULL, comm, (void *)&oSerialData);
			
	// loads entire file and sends it; TODO
	char chFileName[BUFFER_SIZE];
	printf("\nEnter filename: ");
	scanf("%s", chFileName);
	fileName = &chFileName[0];
	send_file(hSerial);
	
	while(1); // the thread comm is supposed to run indefinitely as a new request can be sent anytime
	
	pthread_join(oSerialData.oCom, NULL);
	serial_close(hSerial);
	call_termios(1);
	return 1;
}	







