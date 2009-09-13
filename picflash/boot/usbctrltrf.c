/*********************************************************************
 *
 *                Microchip USB C18 Firmware Version 1.2+.MCHPBootload.Unique
 *
 *********************************************************************
 * FileName:        usbctrltrf.c
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC18
 * Compiler:        C18 3.11+
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
 * Rawin Rojvanit       11/19/04    Original.
 * Rawin Rojvanit       05/14/07    Bug fixes.
 * Fritz Schlunder		05/07/09	Bug fix.
 * Fritz Schlunder		07/23/09	Bug fix.
 ********************************************************************/
 
/******************************************************************************
 * -usbctrltrf.c-
 * This file contains functions which handle the Control Transfer mechanism.
 * The only function that is of interest to an end user is the
 * USBCtrlTrfSetupHandler(), which has a section that calls different
 * USB request handlers. A default call to the USBCheckStdRequest always
 * exist and cannot be removed. USBCheckStdRequest contains handlers
 * for the requests specified in chapter 9 of the USB specification.
 * If a request cannot be serviced by USBCheckStdRequest, then it means the
 * request is a class specific request. All device class request handlers
 * in a project should be called from USBCtrlTrfSetupHandler().
 * If no handlers in the project know how to service a request, then the
 * ctrl_trf_session_owner variable would still have the value of MUID_NULL.
 * The MUID_NULL value would cause the USBCtrlEPServiceComplete() to stall
 * the control endpoint, effectively telling the USB host that the
 * request cannot be serviced.
 *
 * This is a mini-version of the actual usbctrltrf.c from the Microchip USB
 * Library. Some functions were optimized to handle byte only operation instead
 * of supporting both byte and word operations.
 * This helps to decrease the the bootload application size to fit
 * under 2KB, which is the size of the boot block.
 * 
 *****************************************************************************/

/** I N C L U D E S **********************************************************/
#include <p18cxxx.h>
#include "typedefs.h"
#include "usb.h"

/** V A R I A B L E S ********************************************************/
#pragma udata
byte ctrl_trf_state;                // Control Transfer State
byte ctrl_trf_session_owner;        // Current transfer session owner

POINTER pSrc;                       // Data source pointer
POINTER pDst;                       // Data destination pointer
WORD wCount;                        // Data counter

/** P R I V A T E  P R O T O T Y P E S ***************************************/
void USBCtrlTrfSetupHandler(void);
void USBCtrlTrfOutHandler(void);
void USBCtrlTrfInHandler(void);

/** D E C L A R A T I O N S **************************************************/
#pragma code
/******************************************************************************
 * Function:        void USBCtrlEPService(void)
 *
 * PreCondition:    USTAT is loaded with a valid endpoint address.
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        USBCtrlEPService checks for three transaction types that
 *                  it knows how to service and services them:
 *                  1. EP0 SETUP
 *                  2. EP0 OUT
 *                  3. EP0 IN
 *                  It ignores all other types (i.e. EP1, EP2, etc.)
 *
 * Note:            None
 *****************************************************************************/
void USBCtrlEPService(void)
{   
    if(USTAT == EP00_OUT)
    {
		//If the current EP0 OUT buffer has a SETUP packet
        if(ep0Bo.Stat.PID == SETUP_TOKEN)
        {
	        ep0Bi.Stat.UOWN = 0;	//Clear the UOWN bit for EP0 IN, in case it was set (ex: due to a stall)
	        
	        //Check if the SETUP transaction data went into the CtrlTrfData buffer.
	        //If so, need to copy it to the SetupPkt buffer so that it can be 
	        //processed correctly by USBCtrlTrfSetupHandler().
	        if(ep0Bo.ADR == (byte*)(&CtrlTrfData))	
	        {
		        unsigned char setup_cnt;
		
		        ep0Bo.ADR = (byte*)(&SetupPkt);
		        for(setup_cnt = 0; setup_cnt < sizeof(CTRL_TRF_SETUP); setup_cnt++)
		        {
		            *(((byte*)&SetupPkt)+setup_cnt) = *(((byte*)&CtrlTrfData)+setup_cnt);
		        }//end for
		    } 
	        
			//Handle the control transfer (parse the 8-byte SETUP command and figure out what to do)
            USBCtrlTrfSetupHandler();
        }
        else
        {
			//Handle the DATA transfer
            USBCtrlTrfOutHandler();
        }
    }
    else if(USTAT == EP00_IN)                       // EP0 IN
        USBCtrlTrfInHandler();
    
}//end USBCtrlEPService

/******************************************************************************
 * Function:        void USBCtrlTrfSetupHandler(void)
 *
 * PreCondition:    SetupPkt buffer is loaded with valid USB Setup Data
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This routine is a task dispatcher and has 3 stages.
 *                  1. It initializes the control transfer state machine.
 *                  2. It calls on each of the module that may know how to
 *                     service the Setup Request from the host.
 *                     Module Example: USB9, HID, CDC, MSD, ...
 *                     As new classes are added, ClassReqHandler table in
 *                     usbdsc.c should be updated to call all available
 *                     class handlers.
 *                  3. Once each of the modules has had a chance to check if
 *                     it is responsible for servicing the request, stage 3
 *                     then checks direction of the transfer to determine how
 *                     to prepare EP0 for the control transfer.
 *                     Refer to USBCtrlEPServiceComplete() for more details.
 *
 * Note:            Microchip USB Firmware has three different states for
 *                  the control transfer state machine:
 *                  1. WAIT_SETUP
 *                  2. CTRL_TRF_TX
 *                  3. CTRL_TRF_RX
 *                  Refer to firmware manual to find out how one state
 *                  is transitioned to another.
 *
 *                  A Control Transfer is composed of many USB transactions.
 *                  When transferring data over multiple transactions,
 *                  it is important to keep track of data source, data
 *                  destination, and data count. These three parameters are
 *                  stored in pSrc,pDst, and wCount. A flag is used to
 *                  note if the data source is from ROM or RAM.
 *
 *****************************************************************************/
void USBCtrlTrfSetupHandler(void)
{
    byte i;
    
    /* Stage 1 */
    ctrl_trf_state = WAIT_SETUP;
    ctrl_trf_session_owner = MUID_NULL;     // Set owner to NULL
    wCount._word = 0;
    
    /* Stage 2 */
    USBCheckStdRequest();                   // See system\usb9\usb9.c

    /* Modifiable Section */
    
    /* Insert other USB Device Class Request Handlers here */
    
    /* End Modifiable Section */

    /* Stage 3 */
    USBCtrlEPServiceComplete();
    
}//end USBCtrlTrfSetupHandler

/******************************************************************************
 * Function:        void USBCtrlTrfOutHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This routine handles an OUT transaction according to
 *                  which control transfer state is currently active.
 *
 * Note:            Note that if the the control transfer was from
 *                  host to device, the session owner should be notified
 *                  at the end of each OUT transaction to service the
 *                  received data.
 *
 *****************************************************************************/
void USBCtrlTrfOutHandler(void)
{
    if(ctrl_trf_state == CTRL_TRF_RX)
    {
		//USBCtrlTrfRxService();
	    //Don't really need a USBCtrlTrfRxService() handler in this bootloader firmware. 
		//The PC application doesn't use control transfers for sending data to the device.
	
		ep0Bo.ADR = (byte*)(&CtrlTrfData);
		ep0Bo.Cnt = EP0_BUFF_SIZE;
        if(ep0Bo.Stat.DTS == 0)
            ep0Bo.Stat._byte = _USIE|_DAT1|_DTSEN;
        else
            ep0Bo.Stat._byte = _USIE|_DAT0|_DTSEN;
    }
    else //In this case the last OUT transaction must have been a status stage of a CTRL_TRF_TX
    {
	    //Prepare EP0 OUT for the next SETUP transaction.
		USBPrepareForNextSetupTrf();
        ep0Bo.Cnt = EP0_BUFF_SIZE;
        ep0Bo.ADR = (byte*)(&SetupPkt);
        ep0Bo.Stat._byte = _USIE|_DAT0|_DTSEN|_BSTALL;			
    }
  
}//end USBCtrlTrfOutHandler

/******************************************************************************
 * Function:        void USBCtrlTrfInHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This routine handles an IN transaction according to
 *                  which control transfer state is currently active.
 *
 *
 * Note:            A Set Address Request must not change the acutal address
 *                  of the device until the completion of the control
 *                  transfer. The end of the control transfer for Set Address
 *                  Request is an IN transaction. Therefore it is necessary
 *                  to service this unique situation when the condition is
 *                  right. Macro mUSBCheckAdrPendingState is defined in
 *                  usb9.h and its function is to specifically service this
 *                  event.
 *****************************************************************************/
void USBCtrlTrfInHandler(void)
{
    mUSBCheckAdrPendingState();         // Must check if in ADR_PENDING_STATE
    
    if(ctrl_trf_state == CTRL_TRF_TX)
    {
        USBCtrlTrfTxService();
        
        if(ep0Bi.Stat.DTS == 0)
            ep0Bi.Stat._byte = _USIE|_DAT1|_DTSEN;
        else
            ep0Bi.Stat._byte = _USIE|_DAT0|_DTSEN;
    }
    else // CTRL_TRF_RX
        USBPrepareForNextSetupTrf();

}//end USBCtrlTrfInHandler

/******************************************************************************
 * Function:        void USBCtrlTrfTxService(void)
 *
 * PreCondition:    pSrc, wCount, and usb_stat.ctrl_trf_mem are setup properly.
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This routine should be called from only two places.
 *                  One from USBCtrlEPServiceComplete() and one from
 *                  USBCtrlTrfInHandler(). It takes care of managing a
 *                  transfer over multiple USB transactions.
 *
 * Note:            This routine works with isochronous endpoint larger than
 *                  256 bytes and is shown here as an example of how to deal
 *                  with BC9 and BC8. In reality, a control endpoint can never
 *                  be larger than 64 bytes.
 *****************************************************************************/
void USBCtrlTrfTxService(void)
{    
    byte byte_to_send;
    
    /*
     * First, have to figure out how many byte of data to send.
     */
    if(wCount._word < EP0_BUFF_SIZE)
        byte_to_send = wCount._word;
    else
        byte_to_send = EP0_BUFF_SIZE;
    
    ep0Bi.Cnt = byte_to_send;
    
    /*
     * Subtract the number of bytes just about to be sent from the total.
     */
    wCount._word -= byte_to_send;
    
    pDst.bRam = (byte*)&CtrlTrfData;        // Set destination pointer

    while(byte_to_send)
    {
        if(usb_stat.ctrl_trf_mem == _ROM)
        {
            *pDst.bRam = *pSrc.bRom;
            pSrc.bRom++;
        }
        else
        {
            *pDst.bRam = *pSrc.bRam;
            pSrc.bRam++;
        }//end if else
        
        pDst.bRam++;
        byte_to_send--;
    }//end while
    
}//end USBCtrlTrfTxService


/******************************************************************************
 * Function:        void USBCtrlEPServiceComplete(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This routine wrap up the ramaining tasks in servicing
 *                  a Setup Request. Its main task is to set the endpoint
 *                  controls appropriately for a given situation. See code
 *                  below.
 *                  There are three main scenarios:
 *                  a) There was no handler for the Request, in this case
 *                     a STALL should be sent out.
 *                  b) The host has requested a read control transfer,
 *                     endpoints are required to be setup in a specific way.
 *                  c) The host has requested a write control transfer, or
 *                     a control data stage is not required, endpoints are
 *                     required to be setup in a specific way.
 *
 *                  Packet processing is resumed by clearing PKTDIS bit.
 *
 * Note:            None
 *****************************************************************************/
void USBCtrlEPServiceComplete(void)
{
/********************************************************************
Bug Fix: May 14, 2007 (#AF1)
*********************************************************************
See silicon errata for 4550 A3. Now clearing PKTDIS before re-arming
any EP0 endpoints.
********************************************************************/
    /*
     * PKTDIS bit is set when a Setup Transaction is received.
     * Clear to resume packet processing.
     */
    UCONbits.PKTDIS = 0;
/*******************************************************************/

    if(ctrl_trf_session_owner == MUID_NULL)
    {
        /*
         * If no one knows how to service this request then stall.
         * Must also prepare EP0 to receive the next SETUP transaction.
         */
        ep0Bo.Cnt = EP0_BUFF_SIZE;
        ep0Bo.ADR = (byte*)&SetupPkt;
        
        ep0Bo.Stat._byte = _USIE|_BSTALL;
        ep0Bi.Stat._byte = _USIE|_BSTALL;
    }
    else    // A module has claimed ownership of the control transfer session.
    {
        if(SetupPkt.DataDir == DEV_TO_HOST)
        {
            if(SetupPkt.wLength < wCount._word)
                wCount._word = SetupPkt.wLength;
            USBCtrlTrfTxService();
            ctrl_trf_state = CTRL_TRF_TX;
            /*
             * Control Read:
             * <SETUP[0]><IN[1]><IN[0]>...<OUT[1]> | <SETUP[0]>
             * 1. Prepare OUT EP to respond to early termination
             *
             * NOTE:
             * If something went wrong during the control transfer,
             * the last status stage may not be sent by the host.
             * When this happens, two different things could happen
             * depending on the host.
             * a) The host could send out a RESET.
             * b) The host could send out a new SETUP transaction
             *    without sending a RESET first.
             * To properly handle case (b), the OUT EP must be setup
             * to receive either a zero length OUT transaction, or a
             * new SETUP transaction.
             *
             * Since the SETUP transaction requires the DTS bit to be
             * DAT0 while the zero length OUT status requires the DTS
             * bit to be DAT1, the DTS bit check by the hardware should
             * be disabled. This way the SIE could accept either of
             * the two transactions.
             *
             * Furthermore, the Cnt byte should be set to prepare for
             * the SETUP data (8-byte or more), and the buffer address
             * should be pointed to SetupPkt.
             */
            ep0Bo.Cnt = EP0_BUFF_SIZE;
            ep0Bo.ADR = (byte*)&SetupPkt;            
            ep0Bo.Stat._byte = _USIE;           // Note: DTSEN is 0!
    
            /*
             * 2. Prepare IN EP to transfer data, Cnt should have
             *    been initialized by responsible request owner.
             */
            ep0Bi.ADR = (byte*)&CtrlTrfData;
            ep0Bi.Stat._byte = _USIE|_DAT1|_DTSEN;
        }
        else    // (SetupPkt.DataDir == HOST_TO_DEV)
        {
            ctrl_trf_state = CTRL_TRF_RX;
            /*
             * Control Write:
             * <SETUP[0]><OUT[1]><OUT[0]>...<IN[1]> | <SETUP[0]>
             *
             * 1. Prepare IN EP to respond to early termination
             *
             *    This is the same as a Zero Length Packet Response
             *    for control transfer without a data stage
             */
            ep0Bi.Cnt = 0;
            ep0Bi.Stat._byte = _USIE|_DAT1|_DTSEN;

            /*
             * 2. Prepare OUT EP to receive data.
             */
            ep0Bo.Cnt = EP0_BUFF_SIZE;
            ep0Bo.ADR = (byte*)&CtrlTrfData;
            ep0Bo.Stat._byte = _USIE|_DAT1|_DTSEN;
        }//end if(SetupPkt.DataDir == DEV_TO_HOST)
    }//end if(ctrl_trf_session_owner == MUID_NULL)

}//end USBCtrlEPServiceComplete

/******************************************************************************
 * Function:        void USBPrepareForNextSetupTrf(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The routine forces EP0 OUT to be ready for a new Setup
 *                  transaction, and forces EP0 IN to be owned by CPU.
 *
 * Note:            None
 *****************************************************************************/
void USBPrepareForNextSetupTrf(void)
{
    ctrl_trf_state = WAIT_SETUP;            // See usbctrltrf.h
//    ep0Bo.Cnt = EP0_BUFF_SIZE;              // Defined in usbcfg.h
//    ep0Bo.ADR = (byte*)&SetupPkt;
//
///********************************************************************
//Bug Fix: May 14, 2007 (#F1)
//*********************************************************************
//In the original firmware, if an OUT token is sent by the host
//before a SETUP token is sent, the firmware would respond with an ACK.
//This is not a correct response, the firmware should have sent a STALL.
//This is a minor non-compliance since a compliant host should not
//send an OUT before sending a SETUP token. The fix allows a SETUP
//transaction to be accepted while stalling OUT transactions.
//********************************************************************/
//    //ep0Bo.Stat._byte = _USIE|_DAT0|_DTSEN;        // Removed
//    ep0Bo.Stat._byte = _USIE|_DAT0|_DTSEN|_BSTALL;  // Added #F1
//
/********************************************************************
Bug Fix: May 14, 2007 (#F3)
*********************************************************************
In the original firmware, if an IN token is sent by the host
before a SETUP token is sent, the firmware would respond with an ACK.
This is not a correct response, the firmware should have sent a STALL.
This is a minor non-compliance since a compliant host should not
send an IN before sending a SETUP token. The fix allows a SETUP
transaction to be accepted while stalling IN transactions.

Although this fix is known, it is not implemented because it
breaks the #AF1 fix in USBCtrlEPServiceComplete().
Since #AF1 fix is more important, this fix, #F3 is commented out.
********************************************************************/
    ep0Bi.Stat._byte = _UCPU;               // Should be removed
    //ep0Bi.Stat._byte = _USIE|_BSTALL;     // Should be added #F3

}//end USBPrepareForNextSetupTrf

/** EOF usbctrltrf.c *********************************************************/
