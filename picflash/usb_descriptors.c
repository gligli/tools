#ifndef __USB_DESCRIPTORS_C
#define __USB_DESCRIPTORS_C
 
/** INCLUDES *******************************************************/
#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "usb.h"

/** CONSTANTS ******************************************************/
#pragma romdata

/* Device Descriptor */
ROM USB_DEVICE_DESCRIPTOR device_dsc=
{
    0x12,    // Size of this descriptor in bytes
    USB_DESCRIPTOR_DEVICE,                // DEVICE descriptor type
    0x0200,                 // USB Spec Release Number in BCD format
    0xFF,                   // Class Code
    0x00,                   // Subclass code
    0x00,                   // Protocol code
    USB_EP0_BUFF_SIZE,          // Max packet size for EP0, see usb_config.h
    0xFFFF,                 // Vendor ID
    0x0004,                 // Product ID: Mouse in a circle fw demo
    0x0100,                 // Device release number in BCD format
    0x01,                   // Manufacturer string index
    0x02,                   // Product string index
    0x03,                   // Device serial number string index
    0x01                    // Number of possible configurations
};

/* Configuration 1 Descriptor */
ROM BYTE configDescriptor1[]={
    /* Configuration Descriptor */
    0x09,//sizeof(USB_CFG_DSC),    // Size of this descriptor in bytes
    USB_DESCRIPTOR_CONFIGURATION,                // CONFIGURATION descriptor type
    DESC_CONFIG_WORD(0x0020),   // Total length of data for this cfg
    1,                      // Number of interfaces in this cfg
    1,                      // Index value of this configuration
    0,                      // Configuration string index
    _DEFAULT | _SELF,               // Attributes, see usb_device.h
    50,                     // Max power consumption (2X mA)

    /* Interface Descriptor */
    0x09,//sizeof(USB_INTF_DSC),   // Size of this descriptor in bytes
    USB_DESCRIPTOR_INTERFACE,               // INTERFACE descriptor type
    0,                      // Interface Number
    0,                      // Alternate Setting Number
    2,                      // Number of endpoints in this intf
    0xFF,                   // Class code
    00,                     // Subclass code
    00,                     // Protocol code
    0,                      // Interface string index
    
    /* Endpoint Descriptor */
    0x07,/*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
    0x82,                       //EndpointAddress
    _BULK,                      //Attributes
    DESC_CONFIG_WORD(NAND_TX_EP_SIZE), //size
    0x00,                        //Interval

    /* Endpoint Descriptor */
    0x07,/*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT, 	//Endpoint Descriptor
    0x05,                       //EndpointAddress
    _BULK,                      //Attributes
    DESC_CONFIG_WORD(NAND_RX_EP_SIZE), //size
    0x00                        //Interval
};


//Language code string descriptor
ROM struct{BYTE bLength;BYTE bDscType;WORD string[1];}sd000={
sizeof(sd000),USB_DESCRIPTOR_STRING,{0x0409
}};

//Manufacturer string descriptor
ROM struct{BYTE bLength;BYTE bDscType;WORD string[5];}sd001={
sizeof(sd001),USB_DESCRIPTOR_STRING,
{'P','C','U','S','B'}
};

//Product string descriptor
ROM struct{BYTE bLength;BYTE bDscType;WORD string[12];}sd002={
sizeof(sd002),USB_DESCRIPTOR_STRING,
{'M','e','m','o','r','y','A','c','c','e','s','s'}
};

ROM struct{BYTE bLength;BYTE bDscType;WORD string[8];}sd003={
sizeof(sd003),USB_DESCRIPTOR_STRING,
{'D','E','A','D','C','0','D','E'}
};


//Array of configuration descriptors
ROM BYTE *ROM USB_CD_Ptr[]=
{
    (ROM BYTE *ROM)&configDescriptor1
};

//Array of string descriptors
ROM BYTE *ROM USB_SD_Ptr[]=
{
    (ROM BYTE *ROM)&sd000,
    (ROM BYTE *ROM)&sd001,
    (ROM BYTE *ROM)&sd002,
    (ROM BYTE *ROM)&sd003
};

#pragma code
#endif
