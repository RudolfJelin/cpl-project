#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define BUFF_SIZE 255

typedef struct listItem
{
	struct listItem * pPrev;
	struct listItem * pNext;
	char * pLine;
}listItem;

typedef struct label_t
{
	int count;
	char name[10][BUFF_SIZE];
	listItem * location[10];
}label_t;

void LI_free(listItem * item)
{
	if(item != NULL){
		if(item->pLine != NULL){
			free(item->pLine);
			item->pLine = NULL;
		}
		free(item);
	}
	
}

listItem * LI_create(char * strLine)
{
	listItem * p = (listItem *)malloc(sizeof(listItem));
	if(p == NULL){
		return NULL;
	}
	//(void)memset( (void*)p, 0, sizeof(listItem));
	p->pPrev = NULL;
	p->pNext = NULL;
	p->pLine = NULL;
	
	int len = strlen(strLine); // TODO ?? -1 to remove trailing newline. Will be replaced with /r/n
	p->pLine = (char *) malloc(len+1); // to include \0
	if(p->pLine == NULL){
		LI_free(p); // can be replaced with LI_free
		return NULL;
	}
	(void)memset( (void *)p->pLine, 0, len+1);
	strncpy(p->pLine, strLine, len);
	p->pLine[len] = 0;// redund., will leave here (TODO)
	
	return p;
}

int LI_length(listItem * pTmp)
{
	int i = 0;
	while(pTmp != NULL)
	{
		pTmp = pTmp->pNext;
		i++;
	}
	return i;
}

listItem * LI_lastItem(listItem * pTmp)
{
	int len = LI_length(pTmp);
	for(int i = 0; i < len-1; i++)
	{
		pTmp = pTmp->pNext;
	}
	return pTmp;
}

void LI_print(listItem * pTmp)
{
	while(pTmp != NULL)
	{
		printf("LI: %s\n", pTmp->pLine);
		pTmp = pTmp->pNext;
	}
}

void LI_printBackward(listItem * pTmp)
{
	pTmp = LI_lastItem(pTmp);
	while(pTmp != NULL)
	{
		printf("LI: %s", pTmp->pLine);
		pTmp = pTmp->pPrev;
	}
}

void LI_remove(listItem * pTmp)
{
	while(pTmp != NULL){
		listItem * pNext = pTmp->pNext;
		printf("Freeing line: '%s'\n", pTmp->pLine);
		LI_free(pTmp); 
		pTmp = pNext;
	}

}

int uncomment(char * line)
{
	int isComment = 0;
	int len = strlen(line);
	for(int i = 1; i < len; i++){ // strip end of line of garbage
		if (line[i-1] == '/' && line[i] == '/'){
			isComment = 1; // remove rest of line
			line[i-1] = 0;
		}
		if(line[i] == 0x0a || line[i] == 0x0d || line[i] == 0 || isComment){
			line[i] = 0;
		}	
	}//TODO strip trailing whitelspace - AND TEST IT
	for(int i = strlen(line)-1; i >= 0; i--)
	{
		if(line[i] == ' ' || line[i] == '\t'){
			line[i] = 0;
		}
		else{
			break;
		}
	}
	
	return strlen(line);
}

listItem * LI_load(char * strFile)
{	
	printf("LI_load: %s\n", strFile);

	listItem * firstItem;
	listItem * pTmp;
	listItem * pNew;

	if(access(strFile, F_OK)){
		fprintf(stderr, "Error: invalid file!\n");
		//exit(EXIT_FAILURE);
		return NULL;
	}

	FILE * pFile = fopen(strFile, "r");
	if(pFile == NULL){
		fprintf(stderr, "Error: invalid file!\n");
		//exit(EXIT_FAILURE);
		return NULL;
	}
	
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	
	int i = 0;
	while( (read = getline(&line, &len, pFile)) != -1)
	{
		len = uncomment(line);
		line = realloc(line, len+1); // +1 for \0
		
		if(len < 2){
			free(line);
			line = NULL;
			continue; // line was either empty or a comment
		}
		
		//line[len-1] = 0; // \r\n
		printf(">> Loaded file line: '%s'\n", line);
			//printf("   Line: \"");
			char * hex = line;
			while(*hex){
				//printf("%02x ", *hex);
				hex++;
			}
			//printf("\"\n");
		
		pNew = LI_create(line);
		if(i == 0){
			firstItem = pNew;
		}
		else{
			pTmp->pNext = pNew;
			pNew->pPrev = pTmp;
		}
		pTmp = pNew;
		i++;
	}
	free(line);
	fclose(pFile);
	
	return firstItem;
}

int charIndex(char * str, char ch)
{
	int i = 0;
	while(str[i] != 0)
	{
		if(str[i] == ch){
			return i;
		}
		i++;
	}
	return -1;
}

int LI_processIncludes(listItem * pTmp)
{
	int filesFound = 0;

	if(pTmp == NULL){
		return 0;
	}
	while(pTmp != NULL)
	{
		listItem * pToBeNext = pTmp->pNext;
	
		char * pIncl = strstr(pTmp->pLine, "#include:");
		if(pIncl)
		{
			int iFirstSemicolon = charIndex(pTmp->pLine, ':');
			if(iFirstSemicolon > 0)
			{
				int iSecondSemicolon = charIndex(&pTmp->pLine[iFirstSemicolon+1], ':');
				if(iSecondSemicolon > 0)
				{
					char fileName[BUFF_SIZE];
					memset(fileName, 0, BUFF_SIZE);
					strncpy(fileName, &pTmp->pLine[iFirstSemicolon+1], iSecondSemicolon);
					printf("File found: %s\n", fileName);
					
					listItem * firstItem;
					listItem * lastItem;
					
					firstItem = LI_load(fileName);
					if(firstItem)
					{
						lastItem = LI_lastItem(firstItem);
						pTmp->pPrev->pNext = firstItem;
						firstItem->pPrev = pTmp->pPrev;
						
						lastItem->pNext = pTmp->pNext;
						pTmp->pNext->pPrev = lastItem;
						
						LI_free(pTmp);
					}
					
				}
			}
			filesFound++;
		}
		
		pTmp = pToBeNext;//pTmp->pNext;
	}
	return filesFound;
}

listItem * goto_eval(char * gotoLine, label_t labels) 
				// input: line from beginning of goto statement and list of labels to compare
{				// output: pointer to where goto is pointing
	//gotoLine ex: "goto:Label3:"
	// to be obtained using pointer to mid-string
	
	printf("gotoLine to be found: '%s'\n", gotoLine);
	
	int labelCount = labels.count;
	char labelName[BUFF_SIZE];
	memset(labelName, 0, BUFF_SIZE);
	
	if(sscanf(gotoLine, "goto:%[^:]s:", labelName) == 0){ // TODO compiler issue ps also fix comment spaces
		fprintf(stderr, "Error: Couln't load goto label name\n");
	}
	
	printf("Loaded goto label: '%s'\n", labelName); // ex: labelName = "Label3"
	
	for(int i = 0; i < labelCount; i++)
	{
		printf("testing: label no. %d/%d: '%s' vs. '%s'\n", i,labelCount,labels.name[i],labelName);
		if(strcmp(labelName,labels.name[i]) == 0)
		{
			return labels.location[i];
		}
	}
	return NULL; // will end program
}

int wait_eval(char * line)
{
	printf("wait_eval called on: '%s'\n", line);
	int msWait = -1;
	
	int iSemicolon = charIndex(line, ':'); //doesnt care about what is after :
	if(iSemicolon > 0){
		//line[iSemicolon] = 0; //also edits original string
		msWait = atoi(line);
	}
	//else  semicolon not found = no time specified
	
	printf("Found wait time: '%d'\n", msWait);
	return msWait;
}

label_t LI_listLabels(listItem * pTmp) // inputs empty label list pointer and firstItem, 
{								     //fills label list, outputs number of labels
	label_t labels;
	int labelsFound = 0;
	int labelCapacity = 10;

	if(pTmp == NULL){
		exit(-1);
	}
	
	while(pTmp != NULL)
	{
		printf("Checking for label: %s\n", pTmp->pLine);
		
		char * pLabel = strstr(pTmp->pLine, "#label:");
		if(pLabel)
		{
			int iFirstSemicolon = charIndex(pTmp->pLine, ':');
			if(iFirstSemicolon > 0)
			{
				int iSecondSemicolon = charIndex(&pTmp->pLine[iFirstSemicolon+1], ':');
				if(iSecondSemicolon > 0)
				{
					if(labelsFound >= labelCapacity){
						fprintf(stderr, "Label overflow error\n");
						exit(-1);
					}
					
					memset(labels.name[labelsFound], 0, BUFF_SIZE);
					strncpy(labels.name[labelsFound], &pTmp->pLine[iFirstSemicolon+1], iSecondSemicolon);
					
					labels.location[labelsFound] = pTmp;
					
					labelsFound++;
				}
			}
		}
		pTmp = pTmp->pNext;
	}
	labels.count = labelsFound;
	
	return labels;
}

