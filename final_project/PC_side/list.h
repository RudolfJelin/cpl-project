#define BUFF_SIZE 255

#ifndef __LIST_H__
#define __LIST_H__

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
label_t LI_listLabels(listItem * pTmp);
listItem * goto_eval(char * gotoLine, label_t labels);
int wait_eval(char * line);
//int main(int argc, char * argv[]);

#endif
