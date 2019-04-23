//Sean Malloy
//Kiran Bhadury

#include <stdio.h>
#include <assert.h>
#include <string.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "hw_apps_rcm.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "gpio.h"
#include "utils.h"
#include "timer.h"
#include "Adafruit_GFX.h"
#include "spi.h"
#include "uart.h"

// Common interface includes
#include "uart_if.h"
#include "timer_if.h"
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"

#include "pin_mux_config.h"

//SPI data
#define APPLICATION_VERSION     "1.1.1"
#define SPI_IF_BIT_RATE  100000
#define TR_BUFF_SIZE     100

//Screen data
#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 128
#define CHAR_HEIGHT 8
#define CHAR_WIDTH 6
#define CHARS_PER_LINE 21
#define RECEIVER_Y_OFFSET 70

//Times are in ms
#define SIGNAL_LENGTH_MS 26
#define USER_DELAY_TIMEOUT_MS 1000

//Time is in ticks
#define DOUBLE_SEND_THRESHHOLD_TK 64000000
#define USER_DELAY_TIMEOUT_TK 80000000

//Key defines
#define MUTE 127
#define LAST 128
#define UNKNOWN 63 //maps to ?
#define NO_KEY 46 //maps to .

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);

static volatile unsigned long edge_count;
static volatile unsigned long signal_base;
static volatile unsigned long wait_base;
static volatile unsigned int timer_recordings[128];
static volatile unsigned char must_be_new_letter;
static volatile unsigned char previous_key;
static volatile unsigned char num_repeats;
static volatile int char_position;
static volatile int drawNow;
static char sender_message[200];
static char receiver_message[200];
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//*****************************************************************************
//                 PIN SETUP -- Start
//*****************************************************************************
typedef struct PinSetting {
    unsigned long port;
    unsigned int pin;
} PinSetting;

static PinSetting receiver_rising = { .port = GPIOA0_BASE, .pin = 0x1};
static PinSetting receiver_falling = { .port = GPIOA3_BASE, .pin = 0x40};
//*****************************************************************************
//                 PIN SETUP -- End
//*****************************************************************************

//*****************************************************************************
//                 LOCAL FUNCTIONS -- Start
//*****************************************************************************
static void BoardInit(void);
static void SPIInit(void);
static void OLEDInit(void);
static void GPIOInit(void);
static void TimersInit(void);
static void UARTInit(void);
static void DrawCharAtSender(int pos, unsigned char c);
static void DrawCharAtReceiver(int pos, unsigned char c);
static void ClearCharAtSender(int pos);
static void ClearSender(void);
static void ClearReceiver(void);
static void DrawReceivedMessage(void);
static void RisingEdgeHandler(void);
static void FallingEdgeHandler(void);
static void StartTimerHandler(void);
static void OnTimerEndHandler(void);
static void OnWaitEndHandler(void);
static void UARTReceiveHandler(void);
static unsigned char DetermineNumber(int input);
static void SendOverUART(void);
//*****************************************************************************
//                 LOCAL FUNCTIONS -- End
//*****************************************************************************

//*****************************************************************************
//                 INITIALIZERS -- Start
//*****************************************************************************
//Initializes the Board
static void BoardInit(void) {
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);

    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}
//Initilializes SPI
static void SPIInit(void)
{
    // Enable the SPI module clock
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    // Reset the peripheral
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
}

static void OLEDInit(void)
{
    //Initializes the OLED screen to black with a horizontal line midway through
    Adafruit_Init();
    fillScreen(0);
    drawLine(0, 64, 127, 64, 0x07FF);
}

static void GPIOInit(void)
{
    // Register the interrupt handlers
    MAP_GPIOIntRegister(receiver_rising.port, RisingEdgeHandler);
    MAP_GPIOIntRegister(receiver_falling.port, StartTimerHandler); //Initially set to start timer on trigger

    // Configure edge interrupts
    MAP_GPIOIntTypeSet(receiver_rising.port, receiver_rising.pin, GPIO_RISING_EDGE);
    MAP_GPIOIntTypeSet(receiver_falling.port, receiver_falling.pin, GPIO_FALLING_EDGE);

    //Clear interrupts
    MAP_GPIOIntClear(receiver_rising.port, receiver_rising.pin);
    MAP_GPIOIntClear(receiver_falling.port, receiver_falling.pin);

    //Enable falling edge interrupt only
    MAP_GPIOIntEnable(receiver_falling.port, receiver_falling.pin);
}

//Function to initialize the Timers
static void TimersInit(void)
{
    signal_base = TIMERA0_BASE;
    wait_base = TIMERA1_BASE;
    Timer_IF_Init(PRCM_TIMERA0, signal_base, TIMER_CFG_ONE_SHOT, TIMER_A, 0);
    Timer_IF_Init(PRCM_TIMERA1, wait_base, TIMER_CFG_ONE_SHOT, TIMER_A, 0);
    Timer_IF_IntSetup(signal_base, TIMER_A, OnTimerEndHandler);
    Timer_IF_IntSetup(wait_base, TIMER_A, OnWaitEndHandler);
    Timer_IF_InterruptClear(signal_base);
    Timer_IF_InterruptClear(wait_base);
}

//Function to initialize UART
static void UARTInit(void)
{
    MAP_UARTConfigSetExpClk(UARTA1_BASE,
                            MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                            UART_BAUD_RATE,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE)
                            );

    //Set up interrupts
    UARTFIFOLevelSet(UARTA1_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);
    UARTIntRegister(UARTA1_BASE, UARTReceiveHandler);
    UARTIntClear(UARTA1_BASE, UART_INT_RX|UART_INT_RT);
    UARTIntEnable(UARTA1_BASE, UART_INT_RX|UART_INT_RT);

    //Enable communication
    UARTEnable(UARTA1_BASE);
}
//*****************************************************************************
//                 INITIALIZERS -- End
//*****************************************************************************

//*****************************************************************************
//                 HANDLERS -- Start
//*****************************************************************************
//This handler gets triggered when we have a rising edge, and records the current timer for the
//the message we received from the IR remote, then increases the edge count we got
static void RisingEdgeHandler(void)
{
    //Clear interrupts
    MAP_GPIOIntClear(receiver_rising.port, receiver_rising.pin);

    //Record timer value
    assert(edge_count < 128);
    timer_recordings[edge_count] = Timer_IF_GetCount(signal_base, TIMER_A);
    edge_count++;
}

//This handler gets triggered when we have a rising edge, and records the current timer for the
//the message we received from the IR remote, then increases the edge count we got
static void FallingEdgeHandler(void)
{
    //Clear interrupts
    MAP_GPIOIntClear(receiver_falling.port, receiver_falling.pin);

    //Record timer value
    assert(edge_count < 128);
    timer_recordings[edge_count] = Timer_IF_GetCount(signal_base, TIMER_A);
    edge_count++;
}

//Called once the first falling edge is triggered, indicating a signal start
static void StartTimerHandler(void)
{
    //Clear signal timer interrupt
    Timer_IF_InterruptClear(signal_base);

    //Clear pin interrupt
    MAP_GPIOIntClear(receiver_falling.port, receiver_falling.pin);

    //Reset edge count
    edge_count = 0;

    //Check wait timer
    Timer_IF_Stop(wait_base, TIMER_A);
    unsigned int waitTime = MAP_TimerValueGet(wait_base, TIMER_A);

    //Check if not a double send, and get ready to parse valid signal
    if(waitTime < DOUBLE_SEND_THRESHHOLD_TK || waitTime == USER_DELAY_TIMEOUT_TK)
    {
        //Enable rising edge interrupt handler, re-map falling edge handler, and start timer
        MAP_GPIOIntEnable(receiver_rising.port, receiver_rising.pin);
        MAP_GPIOIntRegister(receiver_falling.port, FallingEdgeHandler);
        Timer_IF_Start(signal_base, TIMER_A, SIGNAL_LENGTH_MS);
    }

    //Restart wait timer
    Timer_IF_InterruptClear(wait_base);
    Timer_IF_Start(wait_base, TIMER_A, USER_DELAY_TIMEOUT_MS);
}

//Called after the signal timer has timed out, meaning we've (hopefully) read a valid signal
static void OnTimerEndHandler(void)
{
    // Clear the signal timer interrupt and stop it
    Timer_IF_InterruptClear(signal_base);
    Timer_IF_Stop(signal_base, TIMER_A);

    //Save must_be_new_letter in case wait timer later changes it
    unsigned char is_new_letter = must_be_new_letter;

    //Disable handlers while we parse input
    MAP_GPIOIntDisable(receiver_falling.port, receiver_falling.pin);
    MAP_GPIOIntDisable(receiver_rising.port, receiver_rising.pin);

    //Parse input
    if(edge_count > 20) //check if valid signal, if not ignore
    {
        //Set up variables
        int current = 1; //signal starts by sending a 1
        int previous;
        int result = 0; //MUST be set to 0 since we'll do bit operations on it

        //Extract 1s and 0s
        int i = edge_count - 8;
        previous = timer_recordings[i];
        for(; i < edge_count; ++i)
        {
            //gets the time for each bit we received from the IR remote
            //and stores each bit value we receive
            timer_recordings[i] -= previous;
            previous += timer_recordings[i];
            //if there was a large delay, change the current bit to the opposite bit
            if(timer_recordings[i] > 100000){
                if(current == 0)
                {
                    current = 1;
                }
                else
                {
                    current = 0;
                }
            }
            //gets the result and makes it into a bit key for each signal
            result = (result << 1) | current;
        }

        //Convert to key and perform appropriate action
        unsigned char now = DetermineNumber(result); //this function takes the result we got above
        if(now == 1) //this button does nothing
        {
            //do nothing
        }
        else if(now == MUTE)// if mute, delete a letter in our string
        {
            --char_position;
            ClearCharAtSender(char_position);
        }
        else if(now == LAST){
            //if last, then insert a NO_KEY into a string, and send the message
            //This should also clear the sender's message on the OLED
             sender_message[char_position] = NO_KEY;
             sender_message[char_position+1] = '\0';
             ClearSender();
             SendOverUART();
        }
        else //not 1, mute, or last
        {
            //Print character to screen
            if(now == ' ') //space
            {
                num_repeats = 0;
                DrawCharAtSender(char_position, now);
                ++char_position;
            }
            else if(is_new_letter || now != previous_key) //if new letter
            {
                num_repeats = 0;
                DrawCharAtSender(char_position, now);
                ++char_position;
            }
            else //if multi-press
            {
                ++num_repeats;
                //Figure out which key to use on multi-press
                if(now == 'p' || now == 'w')
                {
                    num_repeats %= 4; //resets it if 4
                }
                else
                {
                    num_repeats %= 3; //resets it if 3
                }
                --char_position;
                ClearCharAtSender(char_position);
                //what now+num_repeats does is takes the base character for a key press, and increments
                //it if the key has been pressed multiple times, to get the next character fpr the key
                DrawCharAtSender(char_position, now+num_repeats);
                ++char_position;
            }
        } //not mute or last

        //reset some varaibles
        previous_key = now;
        must_be_new_letter = 0;
    } //valid signal

    //Re-enable falling edge and re-map to trigger on next timer start
    MAP_GPIOIntRegister(receiver_falling.port, StartTimerHandler);
    MAP_GPIOIntEnable(receiver_falling.port, receiver_falling.pin);
}

//Called after the user hasn't pressed any keys after USER_DELAY time
static void OnWaitEndHandler(void)
{
    //Clear wait timer interrupt
    Timer_IF_InterruptClear(wait_base);

    //Update key status
    must_be_new_letter = 1;
}

//called when receiving a message
static void UARTReceiveHandler(void)
{
    //Clear interrupt
    UARTIntClear(UARTA1_BASE, UART_INT_RX|UART_INT_RT);

    //Parse message
    int pos;
    long received;
    //while there is something in the SPI queue, get the next character until empty
    while(UARTCharsAvail(UARTA1_BASE)){
      pos = strlen(receiver_message);
      //function to get character from queue
      received = UARTCharGetNonBlocking(UARTA1_BASE);
      if(received == NO_KEY) //end of transmission
      {
          drawNow = 1; //set drawNow flag that will print the message out in Main
      }
      else //save transmission
      {
          //save message in receive character Array
          receiver_message[pos] = received;
          receiver_message[pos+1] = '\0';
      }
    }
}
//*****************************************************************************
//                 HANDLERS -- End
//*****************************************************************************

//*****************************************************************************
//                 UART -- Start
//*****************************************************************************
//called when pressing the LAST key
static void SendOverUART(void)
{
    int i = 0;
    int len = strlen(sender_message);
    //for the length of Sender_message, put each character in the array into the SPI Queue
    //for the receiver to get later
    while(i < len){
        UARTCharPut(UARTA1_BASE, sender_message[i]);
        i++;
    }

    //Reset sender
    sender_message[0] = '\0';
    char_position = 0;
}
//*****************************************************************************
//                 UART -- End
//*****************************************************************************

//*****************************************************************************
//                 DRAWING -- Start
//*****************************************************************************
//This function takes a character we got from the IR remote, along with what position it is in the string
//and prints it out ont he Top of the OLED
static void DrawCharAtSender(int pos, unsigned char c)
{
    sender_message[pos] = c;
    sender_message[pos+1] = '\0';
    if(c == ' ')
        return;
    int x = (pos % CHARS_PER_LINE) * CHAR_WIDTH;
    int y = (pos / CHARS_PER_LINE) * CHAR_HEIGHT; //integer division
    drawChar(x, y, c, 0x07FF, 0, 1);
}

//This function takes a charater and a position it has in the string
//and prints it out on the bottom portion of the OLED display
static void DrawCharAtReceiver(int pos, unsigned char c)
{
    if(c == ' ')
        return;
    int x = (pos % CHARS_PER_LINE) * CHAR_WIDTH;
    int y = (pos / CHARS_PER_LINE) * CHAR_HEIGHT; //integer division
    y += RECEIVER_Y_OFFSET;
    drawChar(x, y, c, 0x07FF, 0, 1);
}

//this clears out a character on the top portion of the OLED display
//essentially deleting a character from a message.
static void ClearCharAtSender(int pos)
{
    sender_message[pos] = '\0';
    int x = (pos % CHARS_PER_LINE) * CHAR_WIDTH;
    int y = (pos / CHARS_PER_LINE) * CHAR_HEIGHT; //integer division
    fillRect(x, y, CHAR_WIDTH, CHAR_HEIGHT, 0);
}

//this clears out the top of the OLED display for new messages to be made
static void ClearSender(void)
{
    fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT/2, 0);
}

//this clears out the top of the OLED display for new messages to be received
static void ClearReceiver(void)
{
    fillRect(0, SCREEN_HEIGHT/2 + 2, SCREEN_WIDTH, SCREEN_HEIGHT/2 - 2, 0);
}


//This function takes the received string we got from the sender, and prints out the message
//on the bottom half of the OLED display, it calls DrawCharAtReceiver to print it a character at a time
static void DrawReceivedMessage(void)
{
    ClearReceiver();
    int pos = 0;
    int len = strlen(receiver_message);
    while(pos < len)
    {
        DrawCharAtReceiver(pos, receiver_message[pos]);
        ++pos;
    }
}
//*****************************************************************************
//                 DRAWING -- End
//*****************************************************************************

//*****************************************************************************
//                 OTHER METHODS -- Start
//*****************************************************************************
//Convert signal to key number, by determing what the result variable corresponds to
static unsigned char DetermineNumber(int input)
{
    switch(input)
    {
    case 255: //0
        return ' ';
    case 252: //1
        return UNKNOWN;
    case 253: //2
        return 'a';
    case 240: //3
        return 'd';
    case 247: //4
        return 'g';
    case 244: //5
        return 'j';
    case 241: //6
        return 'm';
    case 192: //7
        return 'p';
    case 223: //8
        return 't';
    case 220: //9
        return 'w';
    case 196:
        return MUTE;
    case 130:
        return LAST;
    }

    return UNKNOWN;
}
//*****************************************************************************
//                 OTHER METHODS -- End
//*****************************************************************************

int main() {

    //Initialize peripherals
    BoardInit();
    PinMuxConfig();
    SPIInit();
    OLEDInit();
    GPIOInit();
    TimersInit();
//    UARTInit();

    //Set up global variables
    edge_count = 0;
    must_be_new_letter = 1;
    previous_key = NO_KEY;
    num_repeats = 0;
    char_position = 0;
    sender_message[0] = '\0';
    receiver_message[0] = '\0';
    drawNow = 0;

    //Start wait timer
    Timer_IF_Start(wait_base, TIMER_A, USER_DELAY_TIMEOUT_MS);

    while (1)
    {
        //if the global variable drawNow becomes 1 from the receiver handler, then it should print
        //the received message. We store this function here since its faster to serve it in main
        //and makes the handler more atomic
        if(drawNow == 1){
            DrawReceivedMessage();
            receiver_message[0] = '\0';
            drawNow = 0; //reset drawNow to be used again later.
        }
    }
}
