#ifndef __LIST_H__
#define __LIST_H__

typedef struct listItem
{
	struct listItem * pPrev;
	struct listItem * pNext;
	char * pLine;
}listItem;

typedef struct label
{
	char name[255];// TODO size
	listItem * location;
}label;

listItem * LI_create(char * strLine);
void LI_free(listItem * item);
int LI_length(listItem * pTmp);
listItem * LI_lastItem(listItem * pTmp);
void LI_print(listItem * pTmp);
void LI_printBackward(listItem * pTmp);
void LI_remove(listItem * pTmp);
listItem * LI_load(char * strFile);
int charIndex(char * str, char ch);
int LI_processIncludes(listItem * pTmp);
//listItem * LI_findLabel(listItem * pTmp, char * label);
label * LI_listLabels(listItem * pTmp);
//int main(int argc, char * argv[]);

#endif
