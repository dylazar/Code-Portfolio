/*****************************************************************************
* File Name: main.c
*
* Created: 5/16/18
* Revised: 5/28/18
* Created by: Stephanie Salazar and Ivonne Fajardo
*
* Description: 
* This project communicates via UART in Full duplex mode. The UART has
* receiver(RX) and transmitter(TX)]. The data received by RX is used for
* FSK transmission. 
*
*  This code has been modified from PSoC's UART_Full_Duplex example code.
*  This code example project demonstrates how to communicate between 
*  the PC and UART component in Full duplex mode. The UART has receiver(RX) and 
*  transmitter(TX) part. The data received by RX is looped back to the TX.
*
*******************************************************************************
* Copyright (2017), Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
*
* Related Document: 
* CE210741_UART_Full_Duplex_and_printf_Support_with_PSoC_3_4_5LP.pdf
*
* Hardware Dependency: See 
* CE210741_UART_Full_Duplex_and_printf_Support_with_PSoC_3_4_5LP.pdf
*
*******************************************************************************
*/

#include <project.h>
#include <stdio.h>
#include "stdlib.h"

/***************************************
* UART/TESTING MACRO
***************************************/
#define UART    ENABLED                                                                                                      

/* Character LCD String Length */
#define LINE_STR_LENGTH     (20u)                                                              
/* Change data size for sending longer data (n-1) */
#define DATA_SIZE           (7u)
/* Change max crabs to correlate with data size 2^(n) - 1 */
#define MAX_CRABS           (15)
/* Error used for user error */
#define ERROR               (333u)

/*Definitions*/
#define CLOCK_FREQ 1000000
#define FREQ(x) (CLOCK_FREQ/x)-1

/*PWM Frequencies*/
#define ONE_FREQ     42000
#define ZERO_FREQ    37000
#define AUDIBLE_FREQ 12000

#define BIT_0_MASK 0x01
#define BIT_1_MASK 0x02
#define BIT_2_MASK 0x04
#define BIT_3_MASK 0x08
#define BIT_4_MASK 0x10
#define BIT_5_MASK 0x20
#define BIT_6_MASK 0x40
#define BIT_7_MASK 0x80

#define ZERO              0x0
#define ONE               0x1
#define TRUE              0x1
#define FALSE             0x0
#define ENABLED           1u
#define DISABLED          0u
#define DATA_LENGTH       8
#define DECODE_VALUE      0x01
#define PREFIX_BIT_LENGTH 6
#define PREFIX_MESSAGE    0xFF
#define MAX_DATA_SENDING  3
#define MAX_SLEEP_COUNT   5
#define FiveSecs          5000
#define ON                1
#define OFF               0

/*Enumerations*/
enum state{
    Encoding_Byte,
    Data,
    Parity,
    Decoding_Byte,
};

/*Function Prototypes*/
int Byte(unsigned int hex_value, int bT);
int FindParity(void);
void goToSleep(void);
void wakeUp(void);

CY_ISR_PROTO(isr_sec); // High F Interrupt
CY_ISR_PROTO(watchDogCheck); //reset watchDog timer before reset
CY_ISR_PROTO(RxIsr); // RX Interrupt
CY_ISR_PROTO(wakeUpIsr); // sleep timer interrupt
CY_ISR_PROTO(RxWakeUp); // sleep timer interrupt from UART

/*Global Variables*/
static int error = 0; // flag for input error
static int i = 2; // to iterate through data array
static int sleepCount = 0;
uint16 count;
char8 lineStr[LINE_STR_LENGTH];
char8 data[LINE_STR_LENGTH];
uint8 newDataflag = FALSE;
static int bitTime = 0;
static int currentByte = Encoding_Byte;
static int prefixTime = 0;
static int sendDataCount = 0;
static int ParityFlag = FALSE;
static int maxDataFlag = FALSE;
static int wakeUpData = FALSE;

/* UART Global Variables */
uint8 errorStatus = 0u; // No error at beginning
uint8 crabsToSend = 0x1; // Start at 1 for testing


/*******************************************************************************
* Function Name: main()
*******************************************************************************/
int main()
{
    /*Variable initializations*/
    int bitCase = 0;
    int data_turn = 0;

#if(UART == ENABLED)
    isr_rx_StartEx(RxIsr);
#endif /* UART == ENABLED */
    
    /* Enable global interrupts. */
    CyGlobalIntEnable;
    /* Start Watchdog and its check timer */
    CyWdtStart(CYWDT_1024_TICKS, CYWDT_LPMODE_NOCHANGE); // 2.048 - 3.072 s
    checkWatchDogTimer_Start();
    watchDogCheck_StartEx(watchDogCheck);
    
    /*Block initializations*/
    UART_Start(); 
    LCD_Start();
    PWM_Modulator_Start();
    PWM_Switch_Timer_Start();
    
    /* Start Interrupts */
    isr_sec_StartEx(isr_sec);
    Sleep_ISR_StartEx(wakeUpIsr);
    
    /* Clear LCD line. */
    LCD_Position(0u, 0u);
    LCD_PrintString("                    ");

    /* Output string on LCD. */
    LCD_Position(0u, 0u);
    LCD_PrintString("Hello");
    CyDelay(FiveSecs);

    for(;;)
    {
        if(errorStatus != 0u)
        {
            /* Clear error status */
            errorStatus = 0u;
        }
        switch(currentByte){
            case Encoding_Byte:
                bitCase = Byte(PREFIX_MESSAGE, bitTime);
                break;
            case Data:
                bitCase = Byte(crabsToSend, bitTime);
                break;
            case Parity:
                ParityFlag = TRUE;
                bitCase = FindParity();
                break;
            case Decoding_Byte:
                bitCase = Byte(DECODE_VALUE, bitTime);
                break;
 
            default:
                /* Switch data or repeat data depending on sendDataCount */
                sendDataCount++; // count how many times we send data
                newDataflag = FALSE; // Turn sending off until new data from UART
                prefixTime = 0;
#if(UART == ENABLED)
                data_turn++;
                if (data_turn == DATA_LENGTH) {
                    data_turn = 0;
                }
#else 
                // Restart data if sent MAX_DATA_SENDING
                if(sendDataCount >= MAX_DATA_SENDING){
                    
                    crabsToSend <<= 1; // Move over data a bit
                    data_turn++;
                    maxDataFlag = TRUE; // flag for extra 2 second delay between new data
                }
                //Once data to be sent can't be contained in a byte, reset to 0x1
                if (data_turn >= DATA_LENGTH-1) {
                    data_turn = 0;
                    crabsToSend = ONE;
                }
                
                /* Clear LCD line. */
                LCD_Position(0u, 0u);
                sprintf(data,"Crabs: %d", crabsToSend);
                LCD_PrintString("             ");

                /* Output string on LCD. */
                LCD_Position(0u, 0u);
                LCD_PrintString(data);
 
#endif /* UART == ENABLED */

                // Turn off PWM and stop timer 
                PWM_Modulator_Stop();
                PWM_Switch_Timer_Stop();
                HighVoltage_Write(0); // Turn High Voltage off while delaying
                CyDelay(20);
                SignalBase_Write(0);               
                
                if(sendDataCount >= MAX_DATA_SENDING){
                    
                    SleepTimer_Start();
                    goToSleep();
                    // PSoC Sleep command. To adjust sleep time, change in the hardware
                    CyPmSleep(PM_SLEEP_TIME_NONE, PM_SLEEP_SRC_CTW);
                }

#if(UART == ENABLED)
                /* Check if data has been sent 3 time */
                if(sendDataCount >= MAX_DATA_SENDING){
                    RxWakeUp_StartEx(RxWakeUp); //Start UART interrupt while in sleep mode
                    sendDataCount = 0; // reset for sending new data
                    /* Wait for new data before sending out data */
                    while(newDataflag == FALSE){
                        CyWdtClear(); // Clear watchdog timer while in sleep
                        
                    }
                    //New Transmission, wake up PSOC
                    RxWakeUp_Stop();
                    SleepTimer_Stop();
                    wakeUp(); 
                }else{
                    /* Delay 1 s before sending out for MAX_DATA_SENDING times */
                    CyDelay(1000);
                }
#else 
                if(maxDataFlag == TRUE){
                    sendDataCount = 0;
                    /* Delay in ms and send data after without waiting for UART */
                    while(wakeUpData == FALSE){
                        CyWdtClear(); // Clear watchdog timer while in sleep
                        // PSoC Sleep command. To adjust sleep time, change in the hardware
                        CyPmSleep(PM_SLEEP_TIME_NONE, PM_SLEEP_SRC_CTW);
                    }
                    maxDataFlag = FALSE;
                    wakeUpData = FALSE;
                    //New Transmission, wake up PSOC
                    SleepTimer_Stop();
                    wakeUp(); 
                }else{
                    // Sending data again, pause 1 s in between
                    CyDelay(1000);
                }
                
#endif /* UART == ENABLED */

                /* New data: Turn on circuitry and begin transmission */
                HighVoltage_Write(1);
                CyDelay(20); // Give voltage booster time to charge up
                SignalBase_Write(1);
                /* Reset PWM blocks and bitTime for new transmission */
                bitTime = 0; 
                PWM_Modulator_Start();
                PWM_Switch_Timer_Start();
                currentByte = Encoding_Byte;
                break;
         } //end switch(bitTime) 
        
        /* Send out frequency depending on bit is 1 or 0 */
        if(bitCase == ONE){
            SignalBase_Write(1);
            PWM_Modulator_Start();
            PWM_Modulator_WritePeriod(FREQ(ONE_FREQ));
            PWM_Modulator_WriteCompare((FREQ(ONE_FREQ))/2); // Sets pulse width
        }else if(bitCase == ZERO){
            PWM_Modulator_Stop();
            SignalBase_Write(0);
        } // end if statement
    } // end for loop
} // end main

/*
 * function: int Byte(unsigned int hex_value, int bT)
 * parameters: hex_value - a byte specifying what data you want to send
 *             bT - the current bit time
 * returns: bitCase - a high or low signal to be sent to an output pin
 * description: This function takes in a hex value and sends it out a bit at a
 *  time as a high or low signal depending on the bit time.
 */
int Byte(unsigned int hex_value, int bT)
{
    int bitCase;
    switch(bT){
        case 0:
            bitCase = (hex_value & BIT_7_MASK) >> 7;
            break;
        case 1:
            bitCase = (hex_value & BIT_6_MASK) >> 6;
            break; 
        case 2:
            bitCase = (hex_value & BIT_5_MASK) >> 5;
            break; 
        case 3:
            bitCase = (hex_value & BIT_4_MASK) >> 4;
            break;
        case 4:
            bitCase = (hex_value & BIT_3_MASK) >> 3;
            break; 
        case 5:
            bitCase = (hex_value & BIT_2_MASK) >> 2;
            break; 
        case 6:
            bitCase = (hex_value & BIT_1_MASK) >> 1;
            break; 
        case 7:
            bitCase = (hex_value & BIT_0_MASK);
            break;
        default:
            break;
    } //end switch(bT)
    return bitCase;
}//end Byte()


/*
 * function: void FindParity(void)
 * parameters: void
 * returns: void
 * description: XORs each bit of data to get even or odd parity for
 * error checking
 */
int FindParity()
{
    uint8 bitToCheck = crabsToSend; // store crabsToSend in new variable to manipulate
    int i = 0;
    int parity = (bitToCheck & BIT_0_MASK);
    for(i = 0; i < DATA_LENGTH; i++){
        bitToCheck = bitToCheck >> 1; // shift mask over
        parity = (bitToCheck & BIT_0_MASK) ^ parity; // XOR new bit
    }
    return parity;
}//end FindParity()   

/*
 * function: void wakeUp(void)
 * parameters: none
 * returns: none
 * description: wakes up all modules and restores clocks
 *  
 */

void wakeUp(void){
#if(UART == DISABLED)
    CyPmRestoreClocks();
#endif /* UART == ENABLED */
    LCD_Wakeup();
    checkWatchDogTimer_Wakeup();
    PWM_Modulator_Wakeup();
    PWM_Switch_Timer_Wakeup(); 
    PWM_Switch_Timer_Start();
    
}//end wakeUp()

/*
 * function: void goToSleep(void)
 * parameters: none
 * returns: none
 * description: puts all modules to sleep and save clocks
 *  
 */
void goToSleep(){
    LCD_Sleep();
    PWM_Modulator_Sleep();
    PWM_Switch_Timer_Sleep();
    checkWatchDogTimer_Sleep();
    UART_Sleep();
    watchDogCheck_ClearPending();
    isr_sec_ClearPending();
    isr_rx_ClearPending();
    CyPmSaveClocks();

}//end goToSleep()


/*******************************************************************************
* Function Name: isr_sec
********************************************************************************
*
* Summary:
* Interrupt triggered on a 0.1s timer timeout
 * This ISR will activate every half second and keep track of what
 *  current bit we are on within a byte. After every 8th bit, it resets and
 *  moves on to a new byte.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(isr_sec)
{
    bitTime++;
    /* Only want to send one bit for parity */
    if(ParityFlag == TRUE){
        bitTime = 0;
        currentByte++;
        ParityFlag = FALSE;
    }
    if (bitTime == 8){
        bitTime = 0;
        currentByte++;
    }
}//end CY_ISR(isr_sec)

/*******************************************************************************
* Function Name: RxIsr
********************************************************************************
*
* Summary:
*  Interrupt Service Routine for RX portion of the UART taken from
*  example code and edited for storing data to send
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(RxIsr)
{
    
    //sleepToggle_Write(ON);
    uint8 rxStatus;   
    do
    {
        /* Read receiver status register */
        rxStatus = UART_RXSTATUS_REG;

        if((rxStatus & (UART_RX_STS_BREAK      | UART_RX_STS_PAR_ERROR |
                        UART_RX_STS_STOP_ERROR | UART_RX_STS_OVERRUN)) != 0u)
        {
            /* ERROR handling. */
            errorStatus |= rxStatus & ( UART_RX_STS_BREAK      | UART_RX_STS_PAR_ERROR | 
                                        UART_RX_STS_STOP_ERROR | UART_RX_STS_OVERRUN);
        }
        
        if((rxStatus & UART_RX_STS_FIFO_NOTEMPTY) != 0u)
        {
            newDataflag = TRUE;
            /* Read data from the RX data register */
            //crabsToSend = UART_RXDATA_REG;
            crabsToSend =  UART_GetByte()  ;
            if(errorStatus == 0u)
            {
                /* Send data backward */
                UART_TXDATA_REG = crabsToSend;
                /* Clear LCD line. */
                LCD_Position(0u, 0u);
                sprintf(data,"Crabs: %d", crabsToSend);
                LCD_PrintString("             ");
                /* Output string on LCD. */
                LCD_Position(0u, 0u);
                LCD_PrintString(data);

            }else{
                isr_rx_SetPending();
                sprintf(data,"%d", errorStatus);
                LCD_PrintString(data);
            }

        }
    // Read FIFO until empty
    }while((rxStatus & UART_RX_STS_FIFO_NOTEMPTY) != 0u);
    
    isr_rx_ClearPending();
    //sleepToggle_Write(OFF);
} //end CY_ISR(RxIsr)

/*******************************************************************************
* Function Name: watchDogCheck
********************************************************************************
*
* Summary:
* Reset watchDog timer every 1.4s
* Watchdog should reset system between 2-3s
* Should not get triggered if system experiencing drift 
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(watchDogCheck){
    
    CyWdtClear(); 
} //CY_ISR(watchDogCheck)


/*******************************************************************************
* Function Name: wakeUpIsr
********************************************************************************
*
* Summary:
* Sleep Timer interrupt to check for new data every 1.024 s
* Also resets watch dog timer when watchDogCheck timer is asleep
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(wakeUpIsr){
    SleepTimer_GetStatus(); // Clears the sleep timer interrupt
    CyWdtClear(); // Clear watchdog timer while in sleep
    sleepCount++;
    if(sleepCount > MAX_SLEEP_COUNT){
        wakeUpData = TRUE;
        sleepCount = 0;
    }

} //end CY_ISR(wakeUpIsr)

/*******************************************************************************
* Function Name: wakeUpIsr
********************************************************************************
*
* Summary:
* Sleep Timer interrupt to wake up PSoC from sleep
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(RxWakeUp){
    sleepToggle_Write(1);
    CyPmRestoreClocks();
    UART_Wakeup();
    UART_Start();
    isr_rx_StartEx(RxIsr);
    RxWakeUp_ClearPending();
    RxWakeUp_Disable();
    isr_rx_SetPending();

} //end CY_ISR(wakeUpIsr)

/* [] END OF FILE */