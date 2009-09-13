#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "usb_device.h"
#include "usb.h"
#include "XSPI.h"
#include "Flasher.h"

/** V A R I A B L E S ********************************************************/
#pragma udata

void YourHighPriorityISRCode();
void YourLowPriorityISRCode();

/** VECTOR REMAPPING ***********************************************/
#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x1000
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x1008
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x1018
#elif defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)	
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x800
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x808
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x818
#else	
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x00
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x08
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x18
#endif
	
#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)||defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
#pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS
void Remapped_High_ISR (void)
{
     _asm goto YourHighPriorityISRCode _endasm
}
#pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS
void Remapped_Low_ISR (void)
{
     _asm goto YourLowPriorityISRCode _endasm
}
#pragma code


//These are your actual interrupt handling routines.
#pragma interrupt YourHighPriorityISRCode
void YourHighPriorityISRCode()
{
	//Check which interrupt flag caused the interrupt.
	//Service the interrupt
	//Clear the interrupt flag
	//Etc.
       #if defined(USB_INTERRUPT)
        USBDeviceTasks();
       #endif

}	//This return will be a "retfie fast", since this is in a #pragma interrupt section 
#pragma interruptlow YourLowPriorityISRCode
void YourLowPriorityISRCode()
{
	//Check which interrupt flag caused the interrupt.
	//Service the interrupt
	//Clear the interrupt flag
	//Etc.

}	//This return will be a "retfie", since this is in a #pragma interruptlow section 

/** DECLARATIONS ***************************************************/
#pragma code

static void CheckUSBState(void);

void main(void)
{   
	// All pins to digital
    ADCON1 |= 0x0F; 

	XSPIInit();

    USBDeviceInit();
    USBDeviceAttach();

    while(1) {
		CheckUSBState();
		FlashPollProc();
    }
}

void CheckUSBState()
{
	if (USB_BUS_SENSE) USBDeviceAttach();
	else               USBDeviceDetach();
}

BOOL USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, WORD size)
{
    switch(event)
    {
        case EVENT_CONFIGURED: 
		    FlashInitEP();
            break;

        case EVENT_EP0_REQUEST:
			FlashCheckVendorReq();
            break;
    }      

    return TRUE; 
}