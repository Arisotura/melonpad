#ifndef _LCD_H_
#define _LCD_H_

#define LCD_ID_JDI          0x08922201
#define LCD_ID_PANASONIC    0x00000002

void LCD_Init();
void LCD_DeInit();
void LCD_SetBrightness(int brightness);

#endif // LCD_H_
