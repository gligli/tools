#!/usr/bin/env python
import usb
import sys
import struct
import pprint
from  optparse import OptionParser

pp = pprint.PrettyPrinter()

class ConsoleUI:
  def opStart(self, name):
    sys.stdout.write(name.ljust(40))
    
  def opProgress(self,progress, total=-1):
    if (total >= 0): 
      prstr = "0x%04x / 0x%04x" % (progress, total)
    else:
      prstr = "0x%04x" % (progress)
      
    sys.stdout.write(prstr.ljust(20))
    sys.stdout.write('\x08' * 20)
    sys.stdout.flush()
    
  def opEnd(self, result):
    sys.stdout.write(result.ljust(20))
    sys.stdout.write("\n")

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

  def flashPowerOn(self):
    self.cmd(0x10)

  def flashShutdown(self):
    self.cmd(0x11)
    
  def update(self):
    self.cmd(0xF0)
      
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
    
  def flashErase(self, block):
    self.cmd(0x06, block)
    return self.flashStatus()
    
  def flashReadBlock(self, block):
    self.cmd(0x01, block, 528 * 32)
    
    buffer = self.devhandle.bulkRead(self.ep_in, 528 * 32, 1000)
    buffer = ''.join([chr(x) for x in buffer])

    status = self.flashStatus()
    
    return (status, buffer)
    
  def flashWriteBlock(self, block, buffer):
    self.cmd(0x02, block, len(buffer))
    
    self.devhandle.bulkWrite(self.ep_out, buffer, 1000)

    return self.flashStatus()    
    
def calcecc(data):
	assert len(data) == 0x210
	val = 0
	for i in range(0x1066):
		if not i & 31:
			v = ~struct.unpack("<L", data[i/8:i/8+4])[0]
		val ^= v & 1
		v >>= 1
		if val & 1:
			val ^= 0x6954559
		val >>= 1

	val = ~val
	return data[:-4] + struct.pack("<L", (val << 6) & 0xFFFFFFFF)

def addecc(data, block = 0, off_8 = "\x00" * 4):
	res = ""
	while len(data):
		d = (data[:0x200] + "\x00" * 0x200)[:0x200]
		data = data[0x200:]
		
		d += struct.pack("<L4B4s4s", block / 32, 0, 0xFF, 0, 0, off_8, "\0\0\0\0")
		d = calcecc(d)
		block += 1
		res += d
	return res

op = OptionParser(usage="usage: %prog [options] action [file] [start] [length]\n\nactions: r[ead] w[rite] e[rase] u[pdate] p[oweron] s[hutdown]")
ui = ConsoleUI()

op.add_option('-s', '--size', action='store', nargs=1, dest='flashsize', default=16, type=int, 
                              help='flash size in MBytes [default: %default]')

(options, args) = op.parse_args()

if not args:
  op.error("no action given")

action = args[0]
args = args[1:]
file = None

if action in ('w', 'write'):
  if not args:
    op.error("no filename given")
    
  file = open(args[0], 'r')
  args = args[1:]
  
if action in ('r', 'read'):
  if not args:
    op.error("no filename given")
    
  file = open(args[0], 'w')
  args = args[1:]

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

if action in ('e', 'erase', 'w', 'write', 'r', 'read'):
  try:
    print "FlashConfig: 0x%08x" % (xf.flashInit())
  except:
    xf.flashDeInit()

if action in ('e', 'erase'):
  start = 0
  end = (options.flashsize * 1024) / 16
  
  ui.opStart('Erase')
  
  for b in range(start, end):
    ui.opProgress(b, end - 1)

  ui.opEnd('0x%04x blocks OK' % (end))

if action in ('r', 'read'):
  start = 0
  end = (options.flashsize * 1024) / 16

  ui.opStart('Read')
  
  for b in range(start, end):
    ui.opProgress(b, end - 1)
    (status, buffer) = xf.flashReadBlock(b)
    file.write(buffer)

if action in ('w', 'write'):
  start = 0
  end = (options.flashsize * 1024) / 16 
  blocksize = 528 * 32

  ui.opStart('Write')

  for b in range(start, end):
  	ui.opProgress(b, end -1)  
        buffer = file.read(blocksize)
        
        if len(buffer) < blocksize:
  	  buffer += ('\xFF' * (blocksize-len(buffer)))

	status = xf.flashWriteBlock(b, buffer)

if action in ('u', 'update'):
  xf.update()
  
if action in ('p', 'poweron'):
  xf.flashPowerOn()

if action in ('s', 'shutdown'):
  xf.flashShutdown()
