//*****************************************************************************
//
//! \addtogroup ssl
//! @{
//
//*****************************************************************************
#include <stdio.h>
#include <assert.h>
#include <string.h>


// Simplelink includes
#include "simplelink.h"

//Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_apps_rcm.h"
#include "gpio.h"
#include "timer.h"
#include "Adafruit_GFX.h"
#include "spi.h"

//Common interface includes
#include "pin_mux_config.h"
#include "Adafruit_SSD1351.h"
#include "timer_if.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"

#define MAX_URI_SIZE 128
#define URI_SIZE MAX_URI_SIZE + 1

//SPI data
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


#define APPLICATION_NAME        "SSL"
#define APPLICATION_VERSION     "1.1.1.EEC.Spring2018"
#define SERVER_NAME                "a24stzee4oei9t.iot.us-east-1.amazonaws.com"
#define GOOGLE_DST_PORT             8443

#define SL_SSL_CA_CERT "/cert/rootCA.der"
#define SL_SSL_PRIVATE "/cert/private.der"
#define SL_SSL_CLIENT  "/cert/client.der"

//NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE                31    /* Current Date */
#define MONTH               5     /* Month 1-12 */
#define YEAR                2018  /* Current year */
#define HOUR                5    /* Time - hours */
#define MINUTE              6    /* Time - minutes */
#define SECOND              0     /* Time - seconds */

#define POSTHEADER "POST /things/CC3200Thing/shadow HTTP/1.1\n\r"
#define GETHEADER "GET /things/CC3200Thing/shadow HTTP/1.1\n\r"
#define HOSTHEADER "Host: a24stzee4oei9t.iot.us-east-1.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

//#define DATA1 "{\"state\": {\r\n\"desired\" : {\r\n\"color\" : \"green\"\r\n}}}\r\n\r\n"
static char DATA1[300];
//DATA1 = "{\"state\": {\r\n\"desired\" : {\r\n\"default\" : \"Hi There!\"\r\n}}}\r\n\r\n"


// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

typedef struct
{
   /* time */
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   /* date */
   unsigned long tm_day;
   unsigned long tm_mon;
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
}SlDateTime;


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulPingPacketsRecv = 0; //Number of Ping Packets received
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
signed char    *g_Host = SERVER_NAME;
SlDateTime g_time;
#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

extern void (* const g_pfnVectors[])(void);

static long lRetVal = -1;
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
//                 GLOBAL VARIABLES -- End: df
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

//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
static long WlanConnect();
static int set_time();
static long InitializeAppVariables();
static int tls_connect();
static int connectToAccessPoint();
static int http_post(int);
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
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************
//*****************************************************************************
//                 INITIALIZERS -- Start
//*****************************************************************************
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
    printf("Hi there!");

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
    sender_message[len - 1];
    strcat(DATA1, "{\"state\": {\r\n\"desired\": {\r\n\"default\": \"");
    strcat(DATA1, sender_message);
    strcat(DATA1, "\"\r\n}}}\r\n\r\n");

    //for the length of Sender_message, put each character in the array into the SPI Queue
    //for the receiver to get later
    http_post(lRetVal);
    printf("Hi There!\n");
    http_get(lRetVal);
    //Reset sender
    DATA1[0] = '\0';
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
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent) {
    if(!pWlanEvent) {
        return;
    }

    switch(pWlanEvent->Event) {
        case SL_WLAN_CONNECT_EVENT: {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'.
            // Applications can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //

            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                       "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                       g_ucConnectionSSID,g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT: {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code) {
                UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                    "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else {
                UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                           "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        default: {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {
    if(!pNetAppEvent) {
        return;
    }

    switch(pNetAppEvent->Event) {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT: {
            SlIpV4AcquiredAsync_t *pEventData = NULL;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            //Ip Acquired Event Data
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

            //Gateway IP address
            g_ulGatewayIP = pEventData->gateway;

            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                       "Gateway=%d.%d.%d.%d\n\r",
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
        }
        break;

        default: {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent, SlHttpServerResponse_t *pHttpResponse) {
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent) {
    if(!pDevEvent) {
        return;
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock) {
    if(!pSock) {
        return;
    }

    switch( pSock->Event ) {
        case SL_SOCKET_TX_FAILED_EVENT:
            switch( pSock->socketAsyncEvent.SockTxFailData.status) {
                case SL_ECLOSE: 
                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                                "failed to transmit all queued packets\n\n", 
                                    pSock->socketAsyncEvent.SockTxFailData.sd);
                    break;
                default: 
                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                                "(%d) \n\n",
                                pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
                  break;
            }
            break;

        default:
            UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
          break;
    }
}


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End breadcrumb: s18_df
//*****************************************************************************


//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    0 on success else error code
//!
//! \return None
//!
//*****************************************************************************
static long InitializeAppVariables() {
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    g_Host = SERVER_NAME;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
    return SUCCESS;
}


//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState() {
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode 
    if (ROLE_STA != lMode) {
        if (ROLE_AP == lMode) {
            // If the device is in AP mode, we need to wait for this event 
            // before doing anything 
            while(!IS_IP_ACQUIRED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
            }
        }

        // Switch to STA role and restart 
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again 
        if (ROLE_STA != lRetVal) {
            // We don't want to proceed if the device is not coming up in STA-mode 
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }
    
    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, 
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);
    
    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig 
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, 
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);

    

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal) {
        // Wait
        while(IS_CONNECTED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, 
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();
    
    return lRetVal; // Success
}



//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void) {
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


//****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint
//!
//!  This function connects to the required AP (SSID_NAME) with Security
//!  parameters specified in te form of macros at the top of this file
//!
//! \param  None
//!
//! \return  0 on success else error code
//!
//! \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long WlanConnect() {
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    UART_PRINT("Attempting connection to access point: ");
    UART_PRINT(SSID_NAME);
    UART_PRINT("... ...");
    lRetVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT(" Connected!!!\n\r");


    // Wait for WLAN Event
    while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))) {
        // Toggle LEDs to Indicate Connection Progress
        _SlNonOsMainLoopTask();
        GPIO_IF_LedOff(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
        _SlNonOsMainLoopTask();
        GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
    }

    return SUCCESS;

}

//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time() {
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

//*****************************************************************************
//
//! This function demonstrates how certificate can be used with SSL.
//! The procedure includes the following steps:
//! 1) connect to an open AP
//! 2) get the server name via a DNS request
//! 3) define all socket options and point to the CA certificate
//! 4) connect to the server via TCP
//!
//! \param None
//!
//! \return  0 on success else error code
//! \return  LED1 is turned solid in case of success
//!    LED2 is turned solid in case of failure
//!
//*****************************************************************************
static int tls_connect() {
    SlSockAddrIn_t    Addr;
    int    iAddrSize;
    unsigned char    ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiIP,uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
    long lRetVal = -1;
    int iSockID;

    lRetVal = sl_NetAppDnsGetHostByName(g_Host, strlen((const char *)g_Host),
                                    (unsigned long*)&uiIP, SL_AF_INET);

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't retrieve the host name \n\r", lRetVal);
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(GOOGLE_DST_PORT);
    Addr.sin_addr.s_addr = sl_Htonl(uiIP);
    iAddrSize = sizeof(SlSockAddrIn_t);
    //
    // opens a secure socket 
    //
    iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, SL_SEC_SOCKET);
    if( iSockID < 0 ) {
        return printErrConvenience("Device unable to create secure socket \n\r", lRetVal);
    }

    //
    // configure the socket as TLS1.2
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,\
                               sizeof(ucMethod));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }
    //
    //configure the socket as ECDHE RSA WITH AES256 CBC SHA
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &uiCipher,\
                           sizeof(uiCipher));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //
    //configure the socket with CA certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                           SL_SO_SECURE_FILES_CA_FILE_NAME, \
                           SL_SSL_CA_CERT, \
                           strlen(SL_SSL_CA_CERT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Client Certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME, \
                                    SL_SSL_CLIENT, \
                           strlen(SL_SSL_CLIENT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Private Key - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
            SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME, \
            SL_SSL_PRIVATE, \
                           strlen(SL_SSL_PRIVATE));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }


    /* connect to the peer device - Google server */
    lRetVal = sl_Connect(iSockID, ( SlSockAddr_t *)&Addr, iAddrSize);

    if(lRetVal < 0) {
        UART_PRINT("Device couldn't connect to server:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
        return printErrConvenience("Device couldn't connect to server \n\r", lRetVal);
    }
    else {
        UART_PRINT("Device has connected to the website:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
    }

    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
    return iSockID;
}



long printErrConvenience(char * msg, long retVal) {
    UART_PRINT(msg);
    GPIO_IF_LedOn(MCU_RED_LED_GPIO);
    return retVal;
}



int connectToAccessPoint() {
    long lRetVal = -1;
    GPIO_IF_LedConfigure(LED1|LED3);

    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);

    lRetVal = InitializeAppVariables();
    ASSERT_ON_ERROR(lRetVal);

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0) {
      if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
          UART_PRINT("Failed to configure the device in its default state \n\r");

      return lRetVal;
    }

    UART_PRINT("Device is configured in default state \n\r");

    CLR_STATUS_BIT_ALL(g_ulStatus);

    ///
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //
    UART_PRINT("Opening sl_start\n\r");
    lRetVal = sl_Start(0, 0, 0);
    if (lRetVal < 0 || ROLE_STA != lRetVal) {
        UART_PRINT("Failed to start the device \n\r");
        return lRetVal;
    }

    UART_PRINT("Device started as STATION \n\r");

    //
    //Connecting to WLAN AP
    //
    lRetVal = WlanConnect();
    if(lRetVal < 0) {
        UART_PRINT("Failed to establish connection w/ an AP \n\r");
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }

    UART_PRINT("Connection established w/ AP and IP is aquired \n\r");
    return 0;
}

//*****************************************************************************
//
//! Main 
//!
//! \param  none
//!
//! \return None
//!
//*****************************************************************************

void main() {
    //
    // Initialize board configuration
    //
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

    InitTerm();
    ClearTerm();
    UART_PRINT("My terminal works!\n\r");

    //Connect the CC3200 to the local access point
    lRetVal = connectToAccessPoint();
    //Set time so that encryption can be used
    lRetVal = set_time();
    if(lRetVal < 0) {
        UART_PRINT("Unable to set time in the device");
        LOOP_FOREVER();
    }
    //Connect to the website with TLS encryption
    lRetVal = tls_connect();
    if(lRetVal < 0) {
        ERR_PRINT(lRetVal);
    }
    //Start wait timer
    Timer_IF_Start(wait_base, TIMER_A, USER_DELAY_TIMEOUT_MS);
    while(1){

    }

    http_post(lRetVal);

    sl_Stop(SL_STOP_TIMEOUT);
    LOOP_FOREVER();
}
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

static int http_post(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int dataLength = strlen(DATA1);

    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcpy(pcBufHeaders, DATA1);
    pcBufHeaders += strlen(DATA1);

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

static int http_get(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");


    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

