/*******************************************************************************
* File Name: main.c
* Editer: Stephanie Salazar
* Created: 5/12/18
* Revision: 5/15/18
*
* Description:
*   Receives data from the hyper terminal up to MAX_CRABS.
*   FSK is then started using the input data and then prompts the user for
*   more data. The LCD Display shows the number of crabs sent.
*
*  This code was taken from PSoC's USBFS_UART example code and edited to store
*  a number for sending to another PSoC
*
********************************************************************************
* Description:
*   The component is enumerated as a Virtual Com port. Receives data from the 
*   hyper terminal, then sends back the received data.
*   For PSoC3/PSoC5LP, the LCD shows the line settings.
*
* Related Document:
*  Universal Serial Bus Specification Revision 2.0
*  Universal Serial Bus Class Definitions for Communications Devices
*  Revision 1.2
*
********************************************************************************
* Copyright 2015, Cypress Semiconductor Corporation. All rights reserved.
* This software is owned by Cypress Semiconductor Corporation and is protected
* by and subject to worldwide patent and copyright laws and treaties.
* Therefore, you may use this software only as provided in the license agreement
* accompanying the software package from which you obtained this software.
* CYPRESS AND ITS SUPPLIERS MAKE NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* WITH REGARD TO THIS SOFTWARE, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
********************************************************************************
*/


#include <project.h>
#include "stdio.h"
#include "stdlib.h"

#define USBFS_DEVICE    (0u)

/* The buffer size is equal to the maximum packet size of the IN and OUT bulk
* endpoints.
*/
#define USBUART_BUFFER_SIZE (64u)
#define LINE_STR_LENGTH     (20u)
/* Change data size for sending longer data (n-1) */
#define DATA_SIZE           (7u)
/* Change max crabs to correlate with data size 2^(n) - 1 */
#define MAX_CRABS           (127)
/* Error used for user error */
#define ERROR               (333u)

#define ZERO 0x0
#define ONE 0x1
#define TRUE 0x1
#define FALSE 0x0
#define DATA_LENGTH 4
#define DECODE_VALUE 0x01
#define WAITTIME 80
#define PREFIX_BIT_LENGTH 6
#define PREFIX_MESSAGE 0xFF

/*Function Prototypes*/
int GetCrabs(void);
int CalculateCrabs(void);
void DisplayCrabs(int);

CY_ISR_PROTO(tx_done); // sleep timer interrupt from UART

/*Global Variables*/
int prompt = 1;
int endFlag = 0; // flag for end of user input
int oneDigit = 0; // flag for end of input with one character
int twoDigit = 0; // flag for end of input with two characters
int error = 0; // flag for input error
int i = 2; // to iterate through data array
int dataDone = TRUE; // check if data is done sending
int sendReady = FALSE;
uint16 count;
uint16 countTx = 0;
char8 lineStr[LINE_STR_LENGTH];
uint8 buffer[USBUART_BUFFER_SIZE];
uint8 data[3] = {0};


/*******************************************************************************
* Function Name: main
********************************************************************************
*/
int main()
{
    int crabs = 0;
    int gettingData = TRUE;

    CyGlobalIntEnable; /* Enable global interrupts. */
    /*Block initializations*/
    LCD_Start();

    /* Start USBFS and UART  */
    USBUART_Start(USBFS_DEVICE, USBUART_5V_OPERATION);
    UART_Start();     
    
    tx_done_StartEx(tx_done);

    /* Clear LCD line. */
    LCD_Position(0u, 0u);
    LCD_PrintString("                    ");

    /* Output string on LCD. */
    LCD_Position(0u, 0u);
    LCD_PrintString("Hello");

    for(;;)
    {
        gettingData = 1;
        /* Start UART interface and fill array with 3 parameters until valid */
        while(gettingData){
            while(0u == GetCrabs()){
                if(sendReady == TRUE){
                    while (0u == USBUART_CDCIsReady())
                    {
                    }
                    USBUART_PutString("Data Ready");
                    sendReady = FALSE;
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }
                    USBUART_PutCRLF();
                }
            }
            crabs = CalculateCrabs();
            if(crabs != ERROR){
                DisplayCrabs(crabs);
                gettingData = 0;
            }
        }
        UART_PutChar(crabs); 
        dataDone = FALSE;
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutString("Data Sent");
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutCRLF();
        Data_Timer_Start();
            
    } // end for(;;)
} // end main


/*******************************************************************************************
 * function: int GetCrabs()
 * parameters: hex_value - an 8 bit (1 byte) value specifying what data you want to send
 *             bT - the current bit time
 * returns: bitCase - a high or low signal to be sent to an output pin
 * description: This function starts UART interface and waits for a valid amount of crabs
 * entered by user
 *******************************************************************************************
 */
int GetCrabs()
{
    uint16 crabs = 0;
    /* Host can send double SET_INTERFACE request. */
    if (0u != USBUART_IsConfigurationChanged())
    {
        /* Initialize IN endpoints when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Enumeration is done, enable OUT endpoint to receive data 
            * from host. */
            USBUART_CDC_Init();
        }
    }

        /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
                {
                }
            if(prompt == TRUE){
                USBUART_PutString("Please enter amount of crabs (up to 127). Terminates with carriage return or third character. Any non-integer will be interpreted as a 0.");
            }
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
                {
                }
            if(prompt == TRUE){
                USBUART_PutCRLF();
                prompt = 0;
            }
                
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buffer);
    
                if (strncmp (buffer,"0",1) == 0){
                    //USBUART_PutString("True Zero");
                }
                if (strncmp (buffer,"\r",1) == 0){
                    //USBUART_PutString("Carriage Return");

                    if(i == 1){
                        oneDigit = 1;
                    }else if(i == 0){
                        twoDigit = 1;
                    }
                    endFlag = 1; // set flag to add numbers
                }else{
                    // Convert string to int
                    data[i] = (uint8)atoi(buffer);
                }
                
                /* Make sure data array stays in bounds (size = 3) */
                if(i == 0){
                    i = 2;
                    endFlag = 1; // 3 characters have been entered
                }else{
                    i--;
                }
                
                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to PC */
                    USBUART_PutData(buffer, count);

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }

                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
            } // end (0u != USBUART_DataIsReady())
        } // end (0u != USBUART_GetConfiguration())
    if(endFlag == 1){
        return 1;
    }else{
        return 0;
    }
}//end GetCrabs()

/*******************************************************************************************
 * function: int CalculateCrabs()
 * parameters: none
 * returns: int crabs - amount of crabs from user input 
 * description: This function takes an array of size three and converts
 * to a single number
 *******************************************************************************************
 */
int CalculateCrabs()
{
    int crabs;
    /* Wait until component is ready to send data to host. */
    while (0u == USBUART_CDCIsReady())
    {
    }
    USBUART_PutCRLF();
    /* Shift data if carriage return was pressed */
    if(oneDigit == 1){
        //USBUART_PutString("one digit");
        data[0] = data[2];
        data[2] = 0;
        oneDigit = 0;
    }else if(twoDigit == 1){
        //USBUART_PutString("two digits");
        data[0] = data[1];
        data[1] = data[2];
        data[2] = 0;
        twoDigit = 0;
    }
    /* Apply digit place to integer */
    data[0] = data[0] * 1;
    data[1] = data[1] * 10;
    data[2] = data[2] * 100;
    crabs = data[0] + data[1] + data[2];
    if(dataDone == FALSE){
        error = TRUE;
        /* Wait until component is ready to send data to host. */
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutString("Error. Not Ready for new data.");
        /* Wait until component is ready to send data to host. */
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutCRLF();
    }
    if(crabs > MAX_CRABS){
        crabs = 0;
        error = TRUE;
        /* Wait until component is ready to send data to host. */
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutString("Error. Please enter a number UP TO 127");
        /* Wait until component is ready to send data to host. */
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutCRLF();
    }
    /* reset array */
    data[0] = 0; 
    data[1] = 0;
    data[2] = 0;
    i = 2; // reset indexing for array
    endFlag = 0; // reset endFlag for gathering new data
                    
    if(error == 1){
        error = 0; // reset error checking
        return ERROR;
    }
    else if(dataDone == FALSE){
        return ERROR;
    }else{
        //prompt = TRUE;
        return crabs;
    }
} /* END OF CalculateCrabs() */

/*******************************************************************************************
 * function: void DisplayCrabs()
 * parameters: int crabs
 * returns: void
 * description: Displays the number of crabs on LCD Display
 *******************************************************************************************
 */
void DisplayCrabs(int crabs){
    /* Clear LCD line. */
    LCD_Position(0u, 0u);
    LCD_PrintString("           ");
    /* Reset LCD line position. */
    LCD_Position(0u, 0u);
    /* Store int crabs into a string to print to LCD */
    sprintf(lineStr,"Crabs: %d", crabs);
    LCD_PrintString(lineStr);
}

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
CY_ISR(tx_done){
    countTx++; // every 1.024 seconds
    if(countTx >= WAITTIME){
        dataDone = TRUE;
        sendReady = TRUE;

        countTx = 0;
        Data_Timer_Stop();
    }

} //end CY_ISR(tx_done)



/* [] END OF FILE */
