/* Keypad Layout
*  Numbers 0-9 enter numbers for setting time and solving math problems
*  '*' is backspace
*  '#* is enter
*  'A' enters time programming mode
*  'B' is left arrow
*  'C' is right arrow
*  'D' inverts colors
*/
#ifndef KEYPAD_H
#define KEYPAD_H
#include <stdint.h>

void Keypad_Init(void);

uint16_t MatrixKeypad_Bitmap(void);
char MatricKeypad_Scan(int32_t *keysDownPt);

int MatrixKeypad_Get(char *datapt);
char MatrixKeypad_Peek();
int MatrixKeypad_AdvanceFIFO();
void MatrixKeypad_Flush(void);
void MatrixKeypad_Task(void); // called from ISR
char MatrixKeypad_KeyAt(int32_t row, int32_t col);


extern volatile uint16_t Debug_IsrBitmap;
extern volatile uint32_t Debug_IsrCount;
extern volatile char     Debug_IsrKey;
extern volatile int32_t  Debug_IsrKeysDown;
#endif