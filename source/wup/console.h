#ifndef _CONSOLE_H_
#define _CONSOLE_H_

int Console_Open(int w, int h);
int Console_OpenDefault();
void Console_Close();

int Console_IsActive();

void Console_Clear();
void Console_Print(char* str, int len);

void Console_Update();

#endif // _CONSOLE_H_
