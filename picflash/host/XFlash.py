#!/usr/bin/env python
import usb
import sys
import struct
import pprint

pp = pprint.PrettyPrinter()

class XFlash:
  def __init__(self, usbdev):
    self.devhandle = usbdev.open()
    self.devhandle.setConfiguration(1)
    self.devhandle.claimInterface(0)
    
    self.ep_out = 0x05
    self.ep_in  = 0x82

  def __del__(self):
    try:
      self.devhandle.releaseInterface(0)
      del self.devhandle
    except:
      pass
            
  def cmd(self, cmd, argA=0, argB=0):
    buffer = struct.pack("LL", argA, argB)
    
    self.devhandle.controlMsg(requestType = usb.TYPE_VENDOR,
                              request     = cmd,
                              value       = 0x00,
                              index       = 0x00,
                              buffer      = buffer)
  
  def flashInit(self):
    self.cmd(0x03)

    buffer = self.devhandle.bulkRead(self.ep_in, 4, 1000)
    buffer = ''.join([chr(x) for x in buffer])

    return struct.unpack("L", buffer)[0]

  def flashDeInit(self):
    self.cmd(0x04)    

  def flashStatus(self):
    self.cmd(0x05)

    buffer = self.devhandle.bulkRead(self.ep_in, 4, 1000)
    buffer = ''.join([chr(x) for x in buffer])
    
    return struct.unpack("L", buffer)[0]
    

usbdev = None

for bus in usb.busses():
  for dev in bus.devices:
    if dev.idVendor == 0xFFFF and dev.idProduct == 4:
    	usbdev = dev
    	
if not usbdev:
  print "XFlash USB hardware not found."
  sys.exit(1)
  
print "Using XFlash @ [%s]" % (usbdev.filename)

xf = XFlash(usbdev)

try:
  print "FlashConfig: 0x%08x" % (xf.flashInit())
  print "FlashStatus: 0x%08x" % (xf.flashStatus())
except:
  xf.flashDeInit()

