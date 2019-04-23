// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"
#include "Adafruit_GFX.h"

// Common interface includes
#include "uart_if.h"
#include "pinmux.h"
#include "pinmux2.h"
#include "pin_mux_config.h"
#include "i2c_if.h"

#include "Adafruit_SSD1351.h"
#include "glcdfont.h"

#define APPLICATION_VERSION     "1.1.1"

#define SPI_IF_BIT_RATE  100000
#define TR_BUFF_SIZE     100

#define MASTER_MSG       "This is CC3200 SPI Master Application\n\r"
#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 128
#define CHAR_HEIGHT 7
#define CHAR_WIDTH 6

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************

int x;
int y;
int x2;
int y2;
int r = 4;

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//Adapted from GFX drawChar routine
void drawCharFromBinary(int x, int y, int arrayRow, unsigned int color, unsigned int bg)
{
    unsigned char line;
    char i;
    char j;

    //For each column in char...
    for (i=0; i<6; i++ ) {
      if (i == 5)
        line = 0x0; //leave space at end
      else
        line = font[(arrayRow*5)+i];
      for (j = 0; j<8; j++) { //for each bit in column
        if (line & 0x1) {
            drawPixel(x+i, y+j, color);
        } else if (bg != color) {
            drawPixel(x+i, y+j, bg);
        }
        line >>= 1; //examine next bit
      }
    }
}
//This Program is an extension of the ball program we needed to implement for our project.
//This extends the ball project by implementing a simple version of the game snake, in which a small
//Snake will collect a small randomly placed ball in the screen, and grows bigger each time
void Snake(){
    fillScreen(0);

    //Set up variables

    //devaddr and regoffset are unsigned chars
    // aucrddatabuf is unsigned char [256]
    unsigned long da = 0x18;
    unsigned long ro = 0x3;
    unsigned long rl = 3;
    unsigned char ucDevAddr = (unsigned char)da;
    unsigned char ucRegOffset = (unsigned char)ro;
    unsigned char ucRdLen = (unsigned char)rl;
    unsigned char aucRdDataBuf[256];
    int arrayx[500];
    int arrayy[500];
    int max = 5;
    int i = 0;
    int size = 0;
    int z = 0;
    int xpill;
    int ypill;
    xpill = (rand() % 110) + 10;
    ypill = (rand() % 110) + 10;
    fillCircle(ypill,xpill,r,0xF800);

    //Initial position
    x = 64;
    x2 = 64;
    y = 64;
    y2 = 64;

    while(1)
    {
        //reads data from the Accelerometer
        I2C_IF_Write(ucDevAddr,&ucRegOffset,1,0);
        I2C_IF_Read(ucDevAddr, &aucRdDataBuf[0], ucRdLen);
        //Converts the 2's complement HEX to the correct INT numbers
        int xa = ((int)aucRdDataBuf[0] + 128) % 256 - 128;
        int ya = ((int)aucRdDataBuf[2] + 128) % 256 - 128;
        if(xa < -7){
            xa = -7;
        }
        if(xa > 7){
            xa = 7;
        }
        if(ya < -7){
            ya = -7;
        }
        if(ya > 7){
            ya = 7;
        }
        arrayx[i] = x;
        arrayy[i] = y;
        if(size == max){
            x2 = arrayx[z];
            y2 = arrayy[z];
            z++;
            if(z > max)
                z = 0;
        }
        Ball(xa, ya);
        delay(10);
        i ++;
        printf("%d, %d\n", xpill, x);
        if((abs(xpill - x) < 5)){
            if((abs(ypill - y) < 5)){
                max+=2;
                fillCircle(ypill,xpill,r,0);
                xpill = (rand() % 110) + 10;
                ypill = (rand() % 110) + 10;
                fillCircle(ypill,xpill,r,0xF800);
            }
        }
        if(i > max){
            i = 0;
        }
        if(size < max){
          size ++;
        }

    }
}

//Print all the characters in the glcdfont array
void printChars()
{
    fillScreen(0);
    int x, y;
    int i = 0;
    for(y = 0; y <= SCREEN_HEIGHT - CHAR_HEIGHT; y += CHAR_HEIGHT)
    {
        for(x = 0; x <= SCREEN_WIDTH - CHAR_WIDTH; x += CHAR_WIDTH)
        {
            drawCharFromBinary(x, y, i, 0x07E0, 0);
            ++i;
            if(i >= 255) return;
        }
    }
}

//The function updates the new ball location, and checks if its within bounds
//If the ball starts to fall out of bound, it will correct it
//This function also prints the new ball and erases the old ball.
void Ball(int xa, int ya)
{
    x += (xa/2);
    y += (ya/2);
    if((x - r) < 0){
        x = r;
    }
    if((x + r) > 128){
        x = 128 - r;
    }
    if((y - r) < 0){
        y = r;
    }
    if((y + r) > 128){
        y = 127 - r;
    }
    fillCircle(y2,x2,r,0x0000);
    fillCircle(y,x,r,0x07FF);
}

// This function configures SPI module as master and enables the channel for
// communication
void DrawTestPatterns()
{
        //This erases any garbage that might be on the screen
        fillScreen(0);
        //This prints out the txt file we were given earlier
        printChars();
        delay(300);
        fillScreen(0);
        //This prints Hello World
        drawChar(10,10, 'H', 0x07FF, 0x00, 1);
        drawChar(20,10, 'e', 0x07FF, 0x00, 1);
        drawChar(30,10, 'l', 0x07FF, 0x00, 1);
        drawChar(40,10, 'l', 0x07FF, 0x00, 1);
        drawChar(50,10, 'o', 0x07FF, 0x00, 1);
        drawChar(10,20, 'w', 0x07FF, 0x00, 1);
        drawChar(20,20, 'o', 0x07FF, 0x00, 1);
        drawChar(30,20, 'r', 0x07FF, 0x00, 1);
        drawChar(40,20, 'l', 0x07FF, 0x00, 1);
        drawChar(50,20, 'd', 0x07FF, 0x00, 1);
        drawChar(60,20, '!', 0x07FF, 0x00, 1);
        delay(300);
        lcdTestPattern2();
        delay(300);
        lcdTestPattern();
        delay(300);
        testdrawrects(0xFFE0);
        delay(300);
        testlines(0xF81F);
        delay(300);
        testfastlines(0x001F,0xF800);
        delay(300);
        testdrawrects(0x07E0);
        delay(300);
        testfillrects(0xF81F, 0x07FF);
        delay(300);
        testfillcircles(10,0x0000);
        delay(300);
        testroundrects();
        delay(300);
        testtriangles();
        delay(300);
}

// Board Initialization & Configuration
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

// Main function for spi demo application
void main()
{
    printf("In main!\n");
    //
    // Initialize Board configurations
    //
    BoardInit();

    //
    // Muxing UART and SPI lines.
    //
    PinMuxConfig0();
    PinMuxConfig1();
    PinMuxConfig2();

    I2C_IF_Open(I2C_MASTER_MODE_FST);

    //
    // Enable the SPI module clock
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    // Reset the peripheral
    //
    MAP_PRCMPeripheralReset(PRCM_GSPI);

    //Reset SPI
    MAP_SPIReset(GSPI_BASE);

    //Configure SPI
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                         SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                         (SPI_SW_CTRL_CS |
                         SPI_4PIN_MODE |
                         SPI_TURBO_OFF |
                         SPI_CS_ACTIVEHIGH |
                         SPI_WL_8));

    //Enable SPI for communication
    MAP_SPIEnable(GSPI_BASE);
    //initializes the OLED screen
    Adafruit_Init();


    //This function will print out a list of patterns
    DrawTestPatterns();
    //This function will run the Snake function we created earlier, which loops forever
    Snake();

}

