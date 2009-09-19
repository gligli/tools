/*********************************************************************
 *
 *      Microchip USB C18 Firmware -  USB Bootloader Version 1.00
 *
 *********************************************************************
 * FileName:        boot.c
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC18
 * Compiler:        C18 2.30.01+
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PICmicro® Microcontroller is intended and
 * supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Rawin Rojvanit       11/19/04    Original. USB Bootloader
 ********************************************************************/

/******************************************************************************
 * -boot.c-
 * This file contains functions necessary to carry out bootloading tasks.
 * The only 2 USB specific functions are BootInitEP() and BootService().
 * All other functions can be reused with other communication methods.
 *****************************************************************************/

/** I N C L U D E S **********************************************************/
#include <p18cxxx.h>
#include "typedefs.h"
#include "usb.h"
#include "io_cfg.h"

/** V A R I A B L E S ********************************************************/
#pragma udata
byte counter;
byte byteTemp;
byte trf_state;

word big_counter;

/** D E C L A R A T I O N S **************************************************/
#pragma code

/** C L A S S  S P E C I F I C  R E Q ****************************************/

/** U S E R  A P I ***********************************************************/

/******************************************************************************
 * Function:        void BootInitEP(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        BootInitEP initializes bootloader endpoints, buffer
 *                  descriptors, internal state-machine, and variables.
 *                  It should be called after the USB host has sent out a
 *                  SET_CONFIGURATION request.
 *                  See USBStdSetCfgHandler() in usb9.c for examples.
 *
 * Note:            None
 *****************************************************************************/
void BootInitEP(void)
{   
    trf_state = WAIT_FOR_CMD;
    BOOT_UEP = EP_OUT_IN|HSHK_EN;               // Enable 2 data pipes

    /*
     * Do not have to init Cnt of IN pipes here.
     * Reason:  Number of bytes to send to the host
     *          varies from one transaction to
     *          another. Cnt should equal the exact
     *          number of bytes to transmit for
     *          a given IN transaction.
     *          This number of bytes will only
     *          be known right before the data is
     *          sent.
     */
    BOOT_BD_OUT.Cnt = sizeof(dataPacket);   // Set buffer size
    BOOT_BD_OUT.ADR = (byte*)&dataPacket;   // Set buffer address
    BOOT_BD_OUT.Stat._byte = _USIE|_DAT0|_DTSEN;// Set status

    BOOT_BD_IN.ADR = (byte*)&dataPacket;    // Set buffer address
    BOOT_BD_IN.Stat._byte = _UCPU|_DAT1;    // Set buffer status

}//end BootInitEP

void StartWrite(void)
{
    /*
     * A write command can be prematurely terminated by MCLR or WDT reset
     */
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1_WR = 1;
}//end StartWrite

void ReadVersion(void) //TESTED: Passed
{
    dataPacket._byte[2] = MINOR_VERSION;
    dataPacket._byte[3] = MAJOR_VERSION;
}//end ReadVersion

void ReadProgMem(void) //TESTED: Passed
{
    for (counter = 0; counter < dataPacket.len; counter++)
    {
        //2 separate inst prevents compiler from using RAM stack
        byteTemp = *((dataPacket.ADR.pAdr)+counter);
        dataPacket.data[counter] = byteTemp;
    }//end for
    
    TBLPTRU = 0x00;         // forces upper byte back to 0x00
                            // optional fix is to set large code model
}//end ReadProgMem

void WriteProgMem(void) //TESTED: Passed
{
    /*
     * The write holding register for the 18F4550 family is
     * actually 32-byte. The code below only tries to write
     * 16-byte because the GUI program only sends out 16-byte
     * at a time.
     * This limitation will be fixed in the future version.
     */
	rom far char *user_start = 0x800UL;

	counter = 0;
	//do not overwrite the bootloader!
	while (((dataPacket.ADR.pAdr)+counter) < user_start)
		counter++;

    dataPacket.ADR.low &= 0b11110000;  //Force 16-byte boundary
    EECON1 = 0b10000100;        //Setup writes: EEPGD=1,WREN=1

    //LEN = # of byte to write

    for (counter = 0; counter < (dataPacket.len); counter++)
    {
        *((dataPacket.ADR.pAdr)+counter) = \
        dataPacket.data[counter];
        if ((counter & 0b00001111) == 0b00001111)
        {
            StartWrite();
        }//end if
    }//end for
}//end WriteProgMem

void EraseProgMem(void) //TESTED: Passed
{
    //The most significant 16 bits of the address pointer points to the block
    //being erased. Bits5:0 are ignored. (In hardware).

    //LEN = # of 64-byte block to erase
    EECON1 = 0b10010100;     //Setup writes: EEPGD=1,FREE=1,WREN=1
    for(counter=0; counter < dataPacket.len; counter++)
    {
        *(dataPacket.ADR.pAdr+(((int)counter) << 6));  //Load TBLPTR
        StartWrite();
    }//end for
    TBLPTRU = 0;            // forces upper byte back to 0x00
                            // optional fix is to set large code model
                            // (for USER ID 0x20 0x00 0x00)
}//end EraseProgMem

void ReadEE(void) //TESTED: Passed
{
    EECON1 = 0x00;
    for(counter=0; counter < dataPacket.len; counter++)
    {
        EEADR = (byte)dataPacket.ADR.pAdr + counter;
        //EEADRH = (BYTE)(((int)dataPacket.FIELD.ADDR.POINTER + counter) >> 8);
        EECON1_RD = 1;
        dataPacket.data[counter] = EEDATA;
    }//end for
}//end ReadEE

void WriteEE(void) //TESTED: Passed
{
    for(counter=0; counter < dataPacket.len; counter++)
    {
        EEADR = (byte)dataPacket.ADR.pAdr + counter;
        //EEADRH = (BYTE)(((int)dataPacket.FIELD.ADDR.POINTER + counter) >> 8);
        EEDATA = dataPacket.data[counter];
        EECON1 = 0b00000100;    //Setup writes: EEPGD=0,WREN=1
        StartWrite();
        while(EECON1_WR);       //Wait till WR bit is clear
    }//end for
}//end WriteEE

void BootService(void)
{
    if((usb_device_state < CONFIGURED_STATE)||(UCONbits.SUSPND==1)) return;
    
    if(trf_state == SENDING_RESP)
    {
        if(!mBootTxIsBusy())
        {
            BOOT_BD_OUT.Cnt = sizeof(dataPacket);
            mUSBBufferReady(BOOT_BD_OUT);
            trf_state = WAIT_FOR_CMD;
        }//end if
        return;
    }//end if
    
    if(!mBootRxIsBusy())
    {
        counter = 0;
        switch(dataPacket.CMD)
        {
            case READ_VERSION:
                ReadVersion();
                counter=0x04;
                break;

            case READ_FLASH:
            case READ_CONFIG:
                ReadProgMem();
                counter+=0x05;
                break;

            case WRITE_FLASH:
                WriteProgMem();
                counter=0x01;
                break;

            case ERASE_FLASH:
                EraseProgMem();
                counter=0x01;
                break;

            case READ_EEDATA:
                ReadEE();
                counter+=0x05;
                break;

            case WRITE_EEDATA:
                WriteEE();
                counter=0x01;
                break;
            
            case RESET:
                //When resetting, make sure to drop the device off the bus
                //for a period of time. Helps when the device is suspended.
                UCONbits.USBEN = 0;
                big_counter = 0;
                while(--big_counter);
                
                Reset();
                break;
                
            default:
                break;
        }//end switch()
        trf_state = SENDING_RESP;
        if(counter != 0)
        {
            BOOT_BD_IN.Cnt = counter;
            mUSBBufferReady(BOOT_BD_IN);
        }//end if
    }//end if
}//end BootService

/** EOF boot.c ***************************************************************/
