#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define BUFF_SIZE 255

typedef struct listItem
{
	struct listItem * pPrev;
	struct listItem * pNext;
	char * pLine;
}listItem;

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
	
	int len = strlen(strLine)-1; // -1 to remove trailing newline. Will be replaced with /r/n
	p->pLine = (char *) malloc(len+1); // to include \0
	if(p->pLine == NULL){
		free(p); // can be replaced with LI_free
		return NULL;
	}
	(void)memset( (void *)p->pLine, 0, len+1);
	strncpy(p->pLine, strLine, len);
	p->pLine[len] = 0;// redund., will leave here (TODO)
	
	return p;
}

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
		printf("LI: %s", pTmp->pLine);
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
		LI_free(pTmp);
		pTmp = pNext;
	}

}

listItem * LI_load(char * strFile)
{	
	listItem * firstItem;
	listItem * pTmp;
	listItem * pNew;

	FILE * pFile = fopen(strFile, "r");
	if(pFile == NULL){
		exit(EXIT_FAILURE);
	}
	
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	
	int i = 0;
	while( (read = getline(&line, &len, pFile)) != -1)
	{
		if(len < 2){
			continue;
		}
		line[len-2] = 0; // \r\n
		
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
		char * pIncl = strstr(pTmp->pLine, "#include:");
		if(pIncl)
		{
			int iFirstSemicolon = charIndex(pTmp->pLine, ':');
			if(iFirstSemicolon > 0)
			{
				int iSecondSemicolon = charIndex(&pTmp->pLine[iFirstSemicolon+1], ':');
				if(iFirstSemicolon > 0)
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
					}
					
				}
			}
			filesFound++;
		}
		
		pTmp = pTmp->pNext;
	}
	return filesFound;
}

// TODO: this funct doesnt work. rest should be ok;
listItem * LI_findLabel(listItem * pTmp, char * label)
{
	int filesFound = 0;

	if(pTmp == NULL){
		return 0;
	}
	while(pTmp != NULL)
	{
		char * pIncl = strstr(pTmp->pLine, "#include:");
		if(pIncl)
		{
			int iFirstSemicolon = charIndex(pTmp->pLine, ':');
			if(iFirstSemicolon > 0)
			{
				int iSecondSemicolon = charIndex(&pTmp->pLine[iFirstSemicolon+1], ':');
				if(iFirstSemicolon > 0)
				{
					char labelName[BUFF_SIZE];
					memset(labelName, 0, BUFF_SIZE);
					strncpy(labelName, &pTmp->pLine[iFirstSemicolon+1], iSecondSemicolon);
					printf("Label found: %s\n", labelName);
					
					if(strcmp(labelName, label) == 0)
					{
						return pTmp;
					}
				}
			}
			filesFound++;
		}
		
		pTmp = pTmp->pNext;
	}
	return NULL;
}
