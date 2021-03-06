#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <stdbool.h>
#include <time.h>

#include "commands.h"
#include "serial.h"
#include "list.h"

// array of strings for command menu
char * menu[] = 
	{
	"Item 'o': LED ON",
	"Item 'f': LED OFF",
	"Item 'b': BUTTON STATE",
	"Item 'j': Last Joystick action",
	"Item 'l': Send File",
	"Item 'c': Enter a custom command",
	"Item 'e': Exit"
	};

typedef struct tSerialData
{
	char chBuffIn[BUFFER_SIZE];
	int iBuffLen;
	
	char chCmdBuff[BUFFER_SIZE];
	int iCmdBuffLen;
	
	pthread_t oCom;
	
} tSerialData;

bool quit = false;
bool sendingFile = false;
char fileName[BUFFER_SIZE];
char chBuffOut[BUFFER_SIZE];
char chBuffIn[BUFFER_SIZE];
int hSerial;

int joy_state = 0;
char joystick[6][10] = {"JOY_NONE", "JOY_UP", "JOY_DOWN", "JOY_LEFT", "JOY_RIGHT", "JOY_SEL"};

pthread_mutex_t mtx;
pthread_cond_t condvar;

void printSelection(char * str)
{
	char strLine[] = "Enter option: ";
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
	printf("\r%s", strLine);
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

void send_string(char * strOut)
{
	sprintf(chBuffOut, "%s\r\n", strOut);
	char * hex = chBuffOut;
	while(*hex){
		serial_write(hSerial, hex, 1);
		usleep(1000*50);
		//For some reason, the serial connection will otherwise skip bytes
		//It seems that the evaluation on nucleo side is taking more than in the other projects
		hex++;
	}
}

listItem * skip_labels(listItem * pTmp){
		//skip all labels
		while(strstr(pTmp->pLine, "#label:") == pTmp->pLine)
		{
			pTmp = pTmp->pNext;
		}
		return pTmp;
}

// loads file line by line, then sends it. Is only accessed by a threaad
void send_file()
{
	//printf("Sending file '%s'\n", fileName);

	listItem * firstItem;
	firstItem = LI_load(fileName);
	if(firstItem == NULL){
		return;
	}
	
	int filesFound = LI_processIncludes(firstItem);
	//printf("Found files to include: %d\n", filesFound);
	
	label_t labelList = LI_listLabels(firstItem);
	
	//printf("found %d labels\n", labelList.count);
	//LI_print(firstItem);
	
	listItem * pTmp = firstItem;
	while(pTmp != NULL || !quit) // also can quit
	{
		
		pTmp = skip_labels(pTmp);
		//if is #exit
		if(strstr(pTmp->pLine, "#exit:") == pTmp->pLine)
		{
			//printf("Breaking LI eval\n");
			break; // jumps out of loop
		}
		//if is #goto or :goto - evaluate and change pTmp
		else if(strstr(pTmp->pLine, "#goto:") == pTmp->pLine)
		{
			pTmp = goto_eval(&pTmp->pLine[1], labelList)->pNext; // pTmp jumps to label, then skips it
		}
		// joystic wait
		//if wait for joystick - evaluate and move on
		else if(strstr(pTmp->pLine, "#wait_for_joystick:") == pTmp->pLine)
		{
			joy_state = JOY_NONE;
			double wait_ms = (double)wait_eval(&pTmp->pLine[19]);
			
			time_t start = time(NULL);
			double elapsed;
			int quit_timer = 0;
			while((!quit_timer) && (!joy_state)) // while time running or 
			{
			
				elapsed = difftime(time(NULL), start);
				//printf("elapsed: %lf/%lf\n", elapsed*1000, wait_ms);
				if(wait_ms != (double)-1 && elapsed * 1000 >= wait_ms){ // elapsed_ms > wait_ms or wait_ms = infinite -1
					quit_timer = 1;	
				}
				else{
					usleep(100 * 1000); // 0.1s accuracy is OK enough
				}
			}
			pTmp = pTmp->pNext;
		}
		//#if/else syntax block: 
		while(strstr(pTmp->pLine, "#if:") == pTmp->pLine){
			//printf("evaluating #if:\n");
			//string ex: #if:JOY_UP:goto:Label1:
			int iFirstSemicolon = charIndex(pTmp->pLine, ':');
			if(iFirstSemicolon > 0)
			{
				int iSecondSemicolon = charIndex(&pTmp->pLine[iFirstSemicolon+1], ':');
				if(iSecondSemicolon > 0)
				{
					char joy_name[BUFF_SIZE];
					memset(joy_name, 0, BUFF_SIZE);
					strncpy(joy_name, &pTmp->pLine[iFirstSemicolon+1], iSecondSemicolon);
					//ex: joy_name = "JOY_UP"
					int if_joy_state = 0;
					for(int i = 0; i < 6; i++)
					{
						if(strstr(joy_name,joystick[i]) == joy_name)
						{
							if_joy_state = i;
							break;
						}
					}
					
					if(if_joy_state == joy_state) // if condition of #if is met
					{
						pTmp = goto_eval(&pTmp->pLine[iFirstSemicolon+1+iSecondSemicolon+1], labelList)->pNext;
						break;
					}
					else
					{
						pTmp = pTmp->pNext;
					}
				}
			}
		}
		// #else: syntax block
		if(strstr(pTmp->pLine, "#else:") == pTmp->pLine)
		{ // ex: "#else:goto:Label1:"
			//printf("evaluating #else:\n");
			listItem * pTest = goto_eval(&pTmp->pLine[6], labelList);
			//printf("%p\n", pTest);
			pTmp = pTest->pNext;
			continue;
		}
		
		send_string(pTmp->pLine);
		pTmp = pTmp->pNext;
	}
	printf("End of processing file\n");	 
	LI_remove(firstItem);
	fflush(stdout);
}

void load_file(){
	printf("\nEnter filename: ");
	scanf("%s", fileName);
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
		
		iRecv = serial_read(hSerial,pSerialData->chBuffIn, BUFFER_SIZE);
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
						//printf("NUCLEO SAYS: \"%s\"\n", pSerialData->chCmdBuff);
						
						// checks for all known nucleo outputs
						if(strstr(pSerialData->chCmdBuff, "ERROR") == pSerialData->chCmdBuff)
						{
							printf("Wrong command syntax!\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "UNKNOWN") == pSerialData->chCmdBuff)
						{
							printf("Unknown command\n");
						}
						else if(strstr(pSerialData->chCmdBuff, "NUCLEO") == pSerialData->chCmdBuff)
						{
							printf("Identification: '%s'\n", pSerialData->chCmdBuff);
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_DOWN") == pSerialData->chCmdBuff)
						{
							joy_state = JOY_DOWN;
							if(sendingFile == false){ // if file is being sent dont interfere
								sendingFile = true;
								printf("Nucleo requested LCD clear\n");
								send_string("DRAW:CLEAR 9");
								usleep(1000 * 20);
								sendingFile = false;
							}
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_SEL") == pSerialData->chCmdBuff)
						{
							joy_state = JOY_SEL;
							if(sendingFile == false ){ // if file is being sent dont interfere
								sendingFile = true;
								printf("Nucleo requested resend\n");
								pthread_cond_signal(&condvar);
								usleep(1000 * 20);
								sendingFile = false;
							}
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_UP") == pSerialData->chCmdBuff)
						{
							joy_state = JOY_UP;
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_LEFT") == pSerialData->chCmdBuff)
						{
							joy_state = JOY_LEFT;
						}
						else if(strstr(pSerialData->chCmdBuff, "EVENT:JOY_RIGHT") == pSerialData->chCmdBuff)
						{
							joy_state = JOY_RIGHT;
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

// a separate thread for sending files
void* send(void *v) 
{
	bool q = false;
	int iRecv;
	
	tSerialData * pSerialData;
	pSerialData = (tSerialData *)v;
	
	memset(pSerialData->chBuffIn, '\0', BUFFER_SIZE);
	pSerialData->iBuffLen = 0;
	memset(pSerialData->chCmdBuff, '\0', BUFFER_SIZE);
	pSerialData->iCmdBuffLen = 0;
	
	while (!q) 
	{
		pthread_mutex_lock(&mtx);

		pthread_cond_wait(&condvar, &mtx);
		
		if(fileName[0] == 0 && !quit){ //dont print error if program is set to quit
			fprintf(stderr,"Error: No filename specified!\n");
			return 0;
		}
		else if(!quit){ // otherwise thread end runs this once more	
			while(sendingFile == true) // e.g. something else is sending files
			{
				usleep(1000 * 100); // long interval decreases number of checks. 0.1s doesn't really affect anything.
			}
			
			sendingFile = true;
			send_file();
			sendingFile = false;
		}
		
		q = quit;
		pthread_mutex_unlock(&mtx);
		usleep(10);
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
		return -1;
	}
	call_termios(0);
	//init nucleo's communication service thread
	tSerialData oSerialData;
	pthread_create(&oSerialData.oCom, NULL, comm, (void *)&oSerialData);
	
	tSerialData mSerialData;
	pthread_create(&mSerialData.oCom, NULL, send, (void *)&mSerialData);
	// end of init block
	
	
	// start of main loop section
	char* selection = malloc(sizeof(char)); // allocates space for input char
	*selection = '\n'; // initializes default value
	printMenu(selection);
	
	while(selection[0] != 'e') // e exits program by exiting loop. 
	{
		if(selection[0] != '\n'){
			printMenu(selection);
		}
		
		*selection = getchar();
		printf("\n\n");
		
		switch(*selection)
		{
			case 'o': // sends "LED ON\r\n"
				{
					send_string("LED ON");
					usleep(1000*100);
					break;
				}

			case 'f': // sends "LED OFF\r\n". 
				{	
					send_string("LED OFF");
					usleep(1000*100);
					break;
				}

			case 'b': // sends "BUTTON?". 
				{	
					send_string("BUTTON?");
					usleep(1000*100);
					break;
				}

			case 'l': // load file prototype, evaluate and send
				{	
					load_file();
					pthread_cond_signal(&condvar); // signal to start processing file
					break;
				}

			case 'j': // output last joystick state
				{	
					printf("Last joystick state: %s\n", joystick[joy_state]);
					break;
				}

			case 'c': // sends a string from stdin, without thread
				{	
					if(sendingFile == false)
					{
						sendingFile = true;
						printf("Enter command: ");
						char command[BUFFER_SIZE];
						scanf("%s", command); // waits for string
						send_string(command);
						usleep(1000*100);
						sendingFile = false;
					}
					else
					{
						fprintf(stderr, "A file is being sent. Please wait.");
					}
					break;
				}

			case 'e':
				{	// waits for while condition to end loop
					quit = true;
					usleep(1000*1000);//wait for thread to loop over and quit
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
	
	quit = true;
	pthread_join(oSerialData.oCom, NULL);
	pthread_cond_signal(&condvar);
	pthread_join(mSerialData.oCom, NULL);
	
	serial_close(hSerial);
	call_termios(1);
	return 1;
}	




