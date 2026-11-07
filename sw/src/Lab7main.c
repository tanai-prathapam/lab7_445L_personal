/* Lab7main.c
 * This is a C language project that implements ECE445L Lab5.
 * Jonathan Valvano
 * June 29, 2026
 */
// Possible hardware interface
// 2-bit input, positive logic switches, positive logic software
//   bit1 PA28 Voice switch 
//   bit0 PA27 Play switch 


#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "../inc/Clock.h"
#include "../inc/ST7735_SDC.h"
#include "Lab7.h"
#include "Switch.h"
#include "Mister.h"
#include "../inc/UART.h"
#include "Keypad.h"
#include "BarcodeReader.h"
#include "../inc/Timer.h"
#include "Dump.h"

volatile uint32_t JitterReady = 0;

void (*PeriodicTask)(void);   // user function to be called periodically

void SysTick_InitArm(uint32_t period, uint32_t priority){
  SysTick->CTRL = 0x00;      // disable SysTick during setup
  SysTick->LOAD = period-1;  // reload value
  SCB->SHP[1] = (SCB->SHP[1]&(~0xC0000000))|(priority<<30); // priority 2
  SysTick->VAL = 0;          // any write to VAL clears COUNT and sets VAL equal to LOAD
  SysTick->CTRL = 0x07;      // enable SysTick with 80 MHz bus clock and interrupts
}

#if DEBUG_MODE == 2
// MISTER TESTER
int main(void){
  __disable_irq(); 
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator
  Mister_Init();
  __enable_irq();

  while(1){
    Mister_On();
    Mister_Off();
  }
}
#elif DEBUG_MODE == 3
// MOTOR TEST
int main(void){
  uint32_t n=0;
  __disable_irq(); 
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator

  ST7735_InitR(INITR_REDTAB); //INITR_REDTAB for AdaFruit, INITR_BLACKTAB for SPI HiLetgo ST7735R
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_SetCursor(0, 0);  
  ST7735_OutString("ECE445L main0\n");
  ST7735_OutString("Ramp output to DAC\n");
  TIMG0->COUNTERREGS.CTRCTL &= ~0x01; // disarm if not using SDC
  TLV5616_Init(0);
  while(1){
    TLV5616_OutNonBlocking(n);
    n = (n+1)&0x0FFF; // 0 to 4095
    Clock_Delay(43);
  }
}
#elif DEBUG_MODE == 4
// Barcode/RFID Test
int main(void){
  char barcode[BARCODE_MAX_LENGTH+1];
  __disable_irq(); 
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator
  UART_Init();
  if(!BarcodeReader_Init(40000000U,115200)){
        __enable_irq();
        UART_OutString("Barcode UART configuration invalid.\r\n");
        while(1){__WFI();}
    }
  __enable_irq();
  UART_OutString("Barcode test: waiting for 16-character long UART scans.\r\n");

  while(1){
    BarcodeResult result=BarcodeReader_Read(barcode,sizeof(barcode));
    if(result==BARCODE_READY){
        UART_OutString("Scanned: ");
        UART_OutString(barcode);
        UART_OutString("\r\n");
    }else if(result!=BARCODE_NONE){
        UART_OutString("Rejected scan; check settings, length, or UART overrun.\r\n");
    }

    uint32_t saved=__get_PRIMASK();
    __disable_irq();
    if(!BarcodeReader_HasPendingInput()){
        __DSB();
        __WFI();
    }
    __set_PRIMASK(saved);
  }
}
#elif DEBUG_MODE == 5

void TIMG0_IRQHandler(void){// runs every 1ms
  if((TIMG0->CPU_INT.IIDX) == 1){ // this will acknowledge
    static uint16_t counter = 0;
    counter = (counter + 1) % 1000;
    if(counter % 100 == 0) {
        MatrixKeypad_Task();
      }
  }
}

int main(void) {
  char c;
  uint16_t map, lastMap = 0xFFFF;
  uint32_t count = 0;
  uint32_t line = 4;
  int32_t row, col;

  __disable_irq();
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator
  ST7735_InitR(INITR_GREENTAB);
  Keypad_Init();

  TimerG0_IntArm(1000, 79, 2);
  __enable_irq();

  ST7735_FillScreen(ST7735_BLACK);
  ST7735_SetCursor(0,0); ST7735_OutString("Mode 5: ISR + FIFO");
  ST7735_SetCursor(0,10); ST7735_OutString("Count:");
  ST7735_SetCursor(0,11); ST7735_OutString("Last:");

  while(1) {

    map = Debug_IsrBitmap;

    if(map != lastMap) {
      lastMap = map;
      ST7735_SetCursor(0,2);
      ST7735_OutUHex2((map >> 8) & 0xFF, ST7735_YELLOW);
      ST7735_OutUHex2(map & 0xFF, ST7735_YELLOW);
    }

    for(row = 0; row < 4; row++) {
      ST7735_SetCursor(2,4+row);
      for(col = 0; col < 4; col++) {
        if(map & (1u << ((4*row) + col))) {
          ST7735_OutChar(MatrixKeypad_KeyAt(row, col));
        } else {
          ST7735_OutChar('.');
        }
        ST7735_OutChar(' ');
      }
    }

    // ISR's view
    ST7735_SetCursor(0,12); ST7735_OutString("ISR map:");
    ST7735_SetCursor(9,12); ST7735_OutUHex2(Debug_IsrBitmap, ST7735_YELLOW);

    ST7735_SetCursor(0,13); ST7735_OutString("ISR key:");
    ST7735_SetCursor(9,13); ST7735_OutChar(Debug_IsrKey ? Debug_IsrKey : '-');
    ST7735_OutChar(' ');
    ST7735_OutUDec(Debug_IsrKeysDown);

    ST7735_SetCursor(0,8); ST7735_OutString("Ticks:");
    ST7735_SetCursor(9,8); ST7735_OutUDec(Debug_IsrCount);

    if(MatrixKeypad_Get(&c)) {
      count++;
      ST7735_SetCursor(9,10); ST7735_OutUDec(count);
      ST7735_SetCursor(9,11); ST7735_OutChar(c);
    }
  }

}
#elif DEBUG_MODE == 0
// Master MSPM0
int main(void){
  uint32_t n=0;
  __disable_irq(); 
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator

  ST7735_InitR(INITR_REDTAB); //INITR_REDTAB for AdaFruit, INITR_BLACKTAB for SPI HiLetgo ST7735R
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_SetCursor(0, 0);  
  ST7735_OutString("ECE445L main0\n");
  ST7735_OutString("Ramp output to DAC\n");
  TIMG0->COUNTERREGS.CTRCTL &= ~0x01; // disarm if not using SDC
  TLV5616_Init(0);
  while(1){
    TLV5616_OutNonBlocking(n);
    n = (n+1)&0x0FFF; // 0 to 4095
    Clock_Delay(43);
  }
}
#else
int main(void){ 
  // Slave MSPM0
  __disable_irq(); 
  // TIMG0 647 load value with prescale of 27
  // store arrays of uint16 create two macros one get 10 left bits and 6 for the right
  // struct with arrays 16 bit for the note and 16 bit for the delay
  // struct time ; tone

  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator
  // TIMG0->COUNTERREGS.CTRCTL &= ~0x01; // disarm G0 if not using SDC
  ST7735_InitR(INITR_REDTAB); //INITR_REDTAB for AdaFruit, INITR_BLACKTAB for SPI HiLetgo ST7735R
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_SetRotation(2);
  ST7735_DrawBitmap(0, 128, jacketart, 128, 128);
  ST7735_SetCursor(0, 12);
  // Switch_Init();
  // Debug_Init();
  // JitterReady = 0;
  
  ST7735_OutString("ECE445L Lab 7 main\n");
  
}
#endif

// -------------------      SysTick_Handler    -------------------------------
// Interrupt service routine
// Executed every 12.5ns*(period)
#if DEBUG_MODE == 2
void SysTick_Handler(void){
}
#endif
