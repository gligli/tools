#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "usb.h"
#include "Flasher.h"
#include "XNAND.h"
#include "XSPI.h"

#define FLASH_TX_READY                0
#define FLASH_TX_BUSY                 1
#define FLASH_TX_COMPLETING           2

#pragma udata USB_VARIABLES=0x470
BYTE FlashVendorReqBuffer[8];

#pragma udata USB_BUFFERS=0x500
unsigned char FlashRxBuffer[NAND_RX_EP_SIZE];
unsigned char FlashTxBuffer[NAND_TX_EP_SIZE];

#pragma udata

BYTE FlashConfigBuffer[4];

DWORD gNextBlock;
BYTE gWordsLeft;
WORD gBytesWritten;

DWORD gCmdArgA;
DWORD gCmdArgB;

BYTE gNextCommand     = 0xFF;
BYTE gCurrentCommand  = 0xFF;
BYTE gCommandProgress = 0x00;
WORD gGlobalStatus    = 0x00;

WORD FlashTxLen;
BYTE FlashTxState = FLASH_TX_READY;
void (*FlashTxCBF)(BYTE *data, BYTE size);

USB_HANDLE FlashDataOutHandle;
USB_HANDLE FlashDataInHandle;

static void FlashDataStatus(void);
static void FlashDataInit(void);
static void PowerUp(void);
static void Shutdown(void);
static void Update(void);
static void FlashDataRead(void);
static void FlashDataErase(void);
static void FlashDataWrite(void);
static void FlashDataDeInit(void);

void FlashVendorReqHandler(void)
{
	((BYTE *)&gCmdArgA)[3] = FlashVendorReqBuffer[3];
	((BYTE *)&gCmdArgA)[2] = FlashVendorReqBuffer[2];
	((BYTE *)&gCmdArgA)[1] = FlashVendorReqBuffer[1];
	((BYTE *)&gCmdArgA)[0] = FlashVendorReqBuffer[0];

	((BYTE *)&gCmdArgB)[3] = FlashVendorReqBuffer[7];
	((BYTE *)&gCmdArgB)[2] = FlashVendorReqBuffer[6];
	((BYTE *)&gCmdArgB)[1] = FlashVendorReqBuffer[5];
	((BYTE *)&gCmdArgB)[0] = FlashVendorReqBuffer[4];

	gCommandProgress = 0x00;
	gCurrentCommand  = gNextCommand;
}

void FlashCheckVendorReq(void)
{
    if(SetupPkt.RequestType != VENDOR) return;
    
    switch(SetupPkt.bRequest)
    {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
        case 0x06:
		case 0x10:
		case 0x11:
		case 0xF0:
			gNextCommand = SetupPkt.bRequest;

            outPipes[0].wCount.Val = SetupPkt.wLength;
            outPipes[0].pDst.bRam = FlashVendorReqBuffer;
            outPipes[0].pFunc = FlashVendorReqHandler;
            outPipes[0].info.bits.busy = 1;
			break;

    }
}

void FlashInitEP(void)
{
	FlashTxState = FLASH_TX_READY;

   	USBEnableEndpoint(NAND_TX_EP,USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
    USBEnableEndpoint(NAND_RX_EP,USB_OUT_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);

    FlashDataOutHandle = USBRxOnePacket(NAND_RX_EP,(BYTE*)&FlashRxBuffer,sizeof(FlashRxBuffer));
    FlashDataInHandle = NULL;
}

void FlashDoTX(void)
{    
	BYTE SendNow;
    USBMaskInterrupts();

	if (!USBHandleBusy(FlashDataInHandle)) {
		switch (FlashTxState) {
			case FLASH_TX_COMPLETING:
				FlashTxState = FLASH_TX_READY;
				break;
			
			case FLASH_TX_BUSY:
				SendNow = FlashTxLen > sizeof(FlashTxBuffer) ? sizeof(FlashTxBuffer) : FlashTxLen;
				
				FlashTxCBF((BYTE *) &FlashTxBuffer, SendNow);    	  
		    	FlashTxLen -= SendNow;
		        
		        if(FlashTxLen == 0)
		                FlashTxState = FLASH_TX_COMPLETING;

		        FlashDataInHandle = USBTxOnePacket(NAND_TX_EP,(BYTE*)&FlashTxBuffer, SendNow);
				break;
		}
    }

    USBUnmaskInterrupts();
}

void FlashPollProc(void)
{

	if((USBDeviceState >= CONFIGURED_STATE) && (!USBSuspendControl)) 
		FlashDoTX();

	switch(gCurrentCommand) {
		case 0x01: FlashDataRead();	break;
		case 0x02: FlashDataWrite(); break;
		case 0x03: FlashDataInit();	break;
		case 0x04: FlashDataDeInit(); break;
		case 0x05: FlashDataStatus(); break;
		case 0x06: FlashDataErase(); break;

		case 0x10: PowerUp(); break;
		case 0x11: Shutdown(); break;
		case 0xF0: Update(); break;
	}

}

void PowerUp() 
{
	XSPILeaveFlashMode();
	XSPIPowerUp();
	gCurrentCommand = 0xFF;
}

void Shutdown()
{
	XSPILeaveFlashMode();
	XSPIShutdown();
	gCurrentCommand = 0xFF;
}

void Update()
{
	unsigned short c = 0;
	while(--c);

	UCONbits.USBEN = 0;
	while(--c);
	while(--c);
	while(--c);
	while(--c);

	Reset();
}

void FlashReadStatusCB(BYTE *buffer, BYTE len)
{
	memset(buffer, 0, len);

	buffer[0] = ((BYTE *)&gGlobalStatus)[0];
	buffer[1] = ((BYTE *)&gGlobalStatus)[1];
}

void FlashReadZeroCB(BYTE *buffer, BYTE len)
{
	memset(buffer, 0, len);
}

void FlashReadConfigCB(BYTE *buffer, BYTE len)
{
	memcpy(buffer, FlashConfigBuffer, len);
}

void FlashDataStatus()
{
	FlashTxSetCBF(FlashReadStatusCB, 4);
	gCurrentCommand = 0xFF;
}

void FlashDataDeInit()
{
	XSPILeaveFlashMode();
	gCurrentCommand = 0xFF;
}

void FlashDataErase()
{
	gGlobalStatus = XNANDErase(gCmdArgA << 5);
	
	FlashTxSetCBF(FlashReadZeroCB, 4);
	gCurrentCommand = 0xFF;
}

void FlashDataInit()
{
	XSPIEnterFlashMode();
	
	XSPIRead(0, FlashConfigBuffer);
	XSPIRead(0, FlashConfigBuffer);

	FlashTxSetCBF(FlashReadConfigCB, 4);
	gCurrentCommand = 0xFF;
}

void NandReadCB(BYTE *buffer, BYTE len)
{
	len /= 4;

	while (len) {
		BYTE readnow;

		if (!gWordsLeft) {
			gGlobalStatus |= XNANDReadStart(gNextBlock);
			gNextBlock++;
			gWordsLeft = 0x84;
		}

		readnow = (len < gWordsLeft)?len:gWordsLeft;		

		XNANDReadProcess(buffer, readnow);
		buffer+=(readnow*4);
		gWordsLeft-=readnow;
		len-=readnow;
	}
}

void FlashDataRead()
{
	gGlobalStatus = 0;
	gWordsLeft = 0;
	gNextBlock = gCmdArgA << 5;

	FlashTxSetCBF(NandReadCB, gCmdArgB);
	gCurrentCommand = 0xFF;
}

void FlashDataWrite()
{
	BYTE *pData = (BYTE *) &FlashRxBuffer;
	BYTE len;

	if (gCommandProgress==0) {
		gGlobalStatus = XNANDErase(gCmdArgA << 5);
		gNextBlock = gCmdArgA << 5;
		gWordsLeft = 0x84;
		gBytesWritten = 0;
		XNANDWriteStart();
		gCommandProgress = 1;
	}

	if (USBHandleBusy(FlashDataOutHandle)) 
		return;
	
	len = USBHandleGetLength(FlashDataOutHandle);
	len/= 4;

	while (len) {
		BYTE writeNow = len > gWordsLeft?gWordsLeft:len;

		XNANDWriteProcess(pData, writeNow);
		pData += writeNow*4;
		len -= writeNow;
		gWordsLeft -= writeNow;

		if (gWordsLeft == 0) {
			gGlobalStatus |= XNANDWriteExecute(gNextBlock);
			gNextBlock++;
			gWordsLeft = 0x84;
			XNANDWriteStart();
			gBytesWritten += 0x210;
		}
	}

    FlashDataOutHandle = USBRxOnePacket(NAND_RX_EP,(BYTE*)&FlashRxBuffer,sizeof(FlashRxBuffer));

	if (gBytesWritten == gCmdArgB)
		gCurrentCommand = 0xFF;
}

