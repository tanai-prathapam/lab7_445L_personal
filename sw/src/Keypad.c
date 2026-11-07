#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "Keypad.h"
#include "../inc/Clock.h"

#define MAX_QUEUE_SIZE 8
#define SETTLE_DELAY 200 // init of 100 (2 glitches) -> 150 (0 glitches) -> 250 (0 glitches)

#define NUM_ROWS 4
#define NUM_COLS 4

#define PB1_MASK 1u<<1
#define PA28_MASK 1u<<28
#define PA31_MASK 1u<<31
#define PB20_MASK 1u<<20

#define PB18_MASK 1u<<18
#define PA18_MASK 1u<<18
#define PA24_MASK 1u<<24
#define PA17_MASK 1u<<17

// uint16_t prevState; //bit vector
static char inputQueue[MAX_QUEUE_SIZE]; // input buffer

static volatile uint32_t PutI;
static volatile uint32_t GetI;

static int Fifo_Put(char data) {
    if((PutI + 1) % MAX_QUEUE_SIZE  == GetI) {
        return 0; // full, so drop the key
    }
    inputQueue[PutI] = data;
    PutI = (PutI + 1) % MAX_QUEUE_SIZE; // write data before advancing PutI
    return 1;
}

/**
 * Pops the oldest keypress from the FIFO into the location specified by \a datapt.
 * @param datapt Pointer to the location to store the character
 * @return Success/failure code (0: empty FIFO; 1: value retrieved)
 * @brief FIFO pop
 */
int MatrixKeypad_Get(char *datapt) {
    if(PutI == GetI) {
        return 0; // empty
    }
    *datapt = inputQueue[GetI];
    GetI = (GetI + 1) % MAX_QUEUE_SIZE; // read data before advancing GetI
    return 1;
}
/**
 * Stores the oldest keypress from the FIFO into the location specified by \a datapt without popping.
 * @param datapt Pointer to the location to store the character
 * @return Success/failure code (0: empty FIFO; 1: value retrieved)
 * @brief FIFO peek
 */
char MatrixKeypad_Peek(){
    if(PutI == GetI) {
        return 0; // empty
    }
    return inputQueue[GetI];
}
/**
 * Advances the FIFO, equivalent to consuming a FIFO item without reading it. 
 * @param none
 * @return Success/failure code (0: empty FIFO; 1: value retrieved)
 * @brief FIFO advance
 */
int MatrixKeypad_AdvanceFIFO(){
    if(PutI == GetI) {
        return 0; // empty
    }
    GetI = (GetI + 1) % MAX_QUEUE_SIZE;
    return 1;
}

// Needed to discard anything queued in case of a mode change
void MatrixKeypad_Flush(void) {
    GetI = PutI;
}

void Keypad_Init(void){
     // ****ECE445L write this ****
    // outputs, HiZ mode
    IOMUX->SECCFG.PINCM[PB1INDEX]  = (uint32_t) 0x02000081;
    IOMUX->SECCFG.PINCM[PA28INDEX]  = (uint32_t) 0x02000081;
    IOMUX->SECCFG.PINCM[PA31INDEX]  = (uint32_t) 0x02000081;
    IOMUX->SECCFG.PINCM[PB20INDEX]  = (uint32_t) 0x02000081;
    // set outputs
    GPIOB->DOE31_0 |= 1<<1 | 1<<20;
    GPIOA->DOE31_0 |= 1<<28 | 1<<31;
    GPIOB->DOUTCLR31_0 = 1<<1 | 1<<20;
    GPIOA->DOUTCLR31_0 = 1<<28 | 1<<31;
    // inputs, pull-up resistors
    IOMUX->SECCFG.PINCM[PB18INDEX]  = (uint32_t) 0x00060081;
    IOMUX->SECCFG.PINCM[PA18INDEX]  = (uint32_t) 0x00060081;
    IOMUX->SECCFG.PINCM[PA24INDEX]  = (uint32_t) 0x00060081;
    IOMUX->SECCFG.PINCM[PA17INDEX]  = (uint32_t) 0x00060081;
}

typedef const struct OutputAlias{
  volatile uint32_t* registerSetLocation;
  volatile uint32_t* registerClearLocation;
  uint32_t bitMask;
} OutputAlias;

static const OutputAlias GPIOOutputs[NUM_COLS] = { // Column indices
    {&(GPIOB->DOUTSET31_0), &(GPIOB->DOUTCLR31_0), PB1_MASK}, // PB1, col 0
    {&(GPIOA->DOUTSET31_0), &(GPIOA->DOUTCLR31_0), PA28_MASK}, // PA28, col 1
    {&(GPIOA->DOUTSET31_0), &(GPIOA->DOUTCLR31_0), PA31_MASK}, // PA31, col 2
    {&(GPIOB->DOUTSET31_0), &(GPIOB->DOUTCLR31_0), PB20_MASK} // PB20, col 3
};

typedef const struct InputAlias{
    const volatile uint32_t* registerReadLocation;
    uint32_t bitMask;
} InputAlias;

static const InputAlias GPIOInputs[NUM_ROWS] = { // Row indices
    {&(GPIOB->DIN31_0), PB18_MASK}, // PB18, row 0
    {&(GPIOA->DIN31_0), PA18_MASK}, // PA18, row 1
    {&(GPIOA->DIN31_0), PA24_MASK}, // PA24, row 2
    {&(GPIOA->DIN31_0), PA17_MASK}, // PA17, row 3
};

static const char KeyMap[NUM_ROWS][NUM_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

uint16_t MatrixKeypad_Bitmap(void){
    uint16_t bitmap = 0;
    int32_t col, row, deselectCol;

    for(col = 0; col < NUM_COLS; col++){
        for(deselectCol = 0; deselectCol < NUM_COLS; deselectCol++) {
            *(GPIOOutputs[deselectCol].registerSetLocation) = GPIOOutputs[deselectCol].bitMask;
        }

        *(GPIOOutputs[col].registerClearLocation) = GPIOOutputs[col].bitMask;

        Clock_Delay(SETTLE_DELAY);

        uint32_t portA = GPIOA->DIN31_0;
        uint32_t portB = GPIOB->DIN31_0;

        for(row = 0; row < NUM_ROWS; row++) {
            uint32_t level;
            if(GPIOInputs[row].registerReadLocation == &GPIOA->DIN31_0) {
                level = portA;
            } else{
                level = portB;
            }

             // negative logic. Low means pressed
            if((level & GPIOInputs[row].bitMask) == 0) {
                bitmap |= (uint16_t)(1u << ((4*row) + col));
            }
        }
    }

    for(col = 0; col < NUM_COLS; col++) {
        *(GPIOOutputs[col].registerClearLocation) = GPIOOutputs[col].bitMask;
    }
    return bitmap;
}

// --------------- REMOVE AFTER DEBUG --------------------
volatile uint16_t Debug_IsrBitmap;
volatile uint32_t Debug_IsrCount;
volatile char     Debug_IsrKey;
volatile int32_t  Debug_IsrKeysDown;
// -------------------------------------------------------


char MatrixKeypad_Scan(int32_t *keysDownPt) {
    uint16_t bitmap = MatrixKeypad_Bitmap();
    // --------------- REMOVE AFTER DEBUG --------------------
    Debug_IsrBitmap = bitmap;
    // -------------------------------------------------------
    int32_t keysDown = 0;
    char key = 0;
    int32_t keyIndex;

    for(keyIndex = 0; keyIndex < 16; keyIndex++) {
        if(bitmap & (1u << keyIndex)) {
            keysDown++;
            key = KeyMap[keyIndex >> 2][keyIndex & 3]; // keyIndex/4 = rorw ; keyIndex%4 = col
        }
    }

    *keysDownPt = keysDown;
    // --------------- REMOVE AFTER DEBUG --------------------
    Debug_IsrKey = (keysDown == 1) ? key : 0;
    Debug_IsrKeysDown = keysDown;
    // -------------------------------------------------------
    return (keysDown == 1) ? key : 0; // if more than one key is pressed, ignore, else return key pressed
}

void MatrixKeypad_Task(void) {
    static char lastSample = 0;
    static char stableKey = 0;
    int32_t keysDown;

    // --------------- REMOVE AFTER DEBUG --------------------
    Debug_IsrCount++;
    Debug_IsrBitmap = MatrixKeypad_Bitmap();
    // -------------------------------------------------------

    char key = MatrixKeypad_Scan(&keysDown); // returns 0 unless exactly one key

    // debounce check
    if(key == lastSample){
        if(key != stableKey) {
            stableKey = key;
            if(key != 0) {
                Fifo_Put(key); // enqueue on the 0 -> key transition only
            }
        }
    }
    lastSample = key;
}

char MatrixKeypad_KeyAt(int32_t row, int32_t col) {
    if((row < 0) || (row >= NUM_ROWS) || (col < 0) || (col >= NUM_COLS)) {
        return ' ';
    }
    return KeyMap[row][col];
}


// struct Row{
//   uint32_t APinsToTurnOn; // output to select row
//   uint32_t APinsToTurnOff; // output to deselect row
//   uint32_t BPinsToTurnOn; // output to select row
//   uint32_t BPinsToTurnOff; // output to deselect row
//   char keycode[4];
// };
// typedef const struct OutputAlias{
//   uint8_t index;
//   uint32_t bitMask;
//   uint32_t* registerSetLocation;
//   uint32_t* registerClearLocation;
// } OutputAlias;
// const OutputAlias GPIOOutputs[4] = { // Column indices
//     {1, 1<<1,  &(GPIOB->DOUTSET31_0), &(GPIOB->DOUTCLR31_0)}, // PB1
//     {2, 1<<28, &(GPIOA->DOUTSET31_0), &(GPIOA->DOUTCLR31_0)}, // PA28
//     {4, 1<<31, &(GPIOA->DOUTSET31_0), &(GPIOA->DOUTCLR31_0)}, // PA31
//     {8, 1<<20, &(GPIOB->DOUTSET31_0), &(GPIOB->DOUTCLR31_0)}, // PB20
// };
// typedef const struct InputAlias{
//     uint8_t index;
//     uint32_t bitMask;
//     uint32_t* registerReadLocation;
// } InputAlias;
// const InputAlias GPIOInputs[4] = { // Row indices
//     {1, 1<<18, &(GPIOB->DIN31_0)}, // PB18
//     {1, 1<<18, &(GPIOA->DIN31_0)}, // PA18
//     {1, 1<<24, &(GPIOA->DIN31_0)}, // PA24
//     {1, 1<<17, &(GPIOA->DIN31_0)}, // PA17
// };
// typedef const struct Row Row_t;
// Row_t ScanTab[5]={
// {  0x01, 0x0E, "123A" }, // row 0
// {  0x02, 0x0D, "456B" }, // row 1
// {  0x04, 0x0B, "789C" }, // row 2
// {  0x08, 0x07, "*0#D" }, // row 3
// {  0x00, 0x00, " " }};
// void MatrixKeypad_Scan(){
//   Row_t *pt;
//   char column, key;
//   int32_t j;
//   key = 0; // default values
//   pt = &ScanTab[0];
//   while(pt->PinsToTurnOn){
//     GPIOB->DOUTSET31_0 = pt->PinsToTurnOff; // 3 pins are off output
//     GPIOB->DOUTCLR31_0 = pt->PinsToTurnOn; // one pin is on
//     Clock_Delay(24); // adjust this depending on capacitive load
//     column = ((GPIOA->DIN31_0&0x0F000000)>>24);// read columns
//     for(j=0; j<=3; j++){
//       if((column&0x01)==0){
//         // key = pt->keycode[j];
//        }
//     column >>= 1; // shift into position
//     }
//     pt++;
//   }
// }
// // Waits for a key to be pressed, then released
// //  returns ASCII code for key pressed,
// //  n is the number of keys pressed
// char MatrixKeypad_In(void){ int32_t n;
// char letter;
//   do{
//     letter = MatrixKeypad_Scan(&n);
//   } while (n != 1); // repeat until exactly one
//   do{
//     letter = MatrixKeypad_Scan(&n);
//   } while (n != 0); // repeat until release
//   return letter;
// }