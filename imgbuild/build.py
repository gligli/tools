# we need to build an image with:

# SMC fitting to system type + hack
# CB = 1920+, zeropaired
# CD for CB
# CE 
# CF 4558, zeropaired
# CG
# Xell
# exploit buffer

# where in flash to find the XELL image

# you need to fill in this
secret_1BL = None

XELL_BASE_FLASH = 0xc0000
CODE_BASE = 0x1c000000
EXPLOIT_BASE = 0x200

CROSS_COMPILE = "xenon-"

# don't change anything from here.

# so we can do updates properly
SCRIPT_VERSION = 0x00

Keyvault = None
SMC = None
CB = None
CD = None
CE = None
CF = None
CG = None
Xell = ""
Exploit = None

if secret_1BL is None:
	secret_1BL = open("key_1BL.bin", "rb").read()

 # Import Psyco if available
try:
	import psyco
	psyco.full()
except ImportError:
	pass

# first, unpack base input image. We are ignoring any updates here
import hmac, sha, struct, sys
try:
	import Crypto.Cipher.ARC4 as RC4
except ImportError:
	print "Error importing Crypto.Cipher.ARC4 - please install python-crypto!"
	print "You can get it from http://www.dlitz.net/software/pycrypto/"
	sys.exit(-1)

def unpack_base_image(image):
	global SMC, CB, CD, CE, Keyvault
	
	if image[0x205] == "\xFF" or image[0x415] == "\xFF" or image[0x200] == "\xFF":
		print "ECC'ed - will unecc."
		res = ""
		for s in range(0, len(image), 528):
			res += image[s:s+512]
		image = res

	unpackstring = "!HHLLL64s5LLLLLLLL"
	(id1, build, flags, bloffset, size0, copyright, z0, z1, z2, z3, r7, size1, r3, r4, z5, z6, smc_len, smc_start) = struct.unpack(unpackstring, image[:struct.calcsize(unpackstring)])
	assert not (z0 or z1 or z2 or z3 or z5 or z6), "zeros are not zero."

	block_offset = bloffset

	SMC = image[smc_start:smc_start+smc_len]
	Keyvault = image[0x4000:0x8000]

	assert smc_len == 0x3000, "never saw an SMC != 0x3000 bytes"

	for block in range(30):
		(block_id, block_build, block_flags, block_entry_point, block_size) = struct.unpack("!2sHLLL", image[block_offset:block_offset+16])
		block_size += 0xF
		block_size &= ~0xF
		id = ord(block_id[1]) & 0xF

		print "Found %dBL (build %d) at %08x" % (id, block_build, block_offset)
		data = image[block_offset:block_offset+block_size]
		
		if id == 2:
			CB = data
		elif id == 4:
			CD = data
		elif id == 5:
			CE = data
		
		block_offset += block_size

		if id == 5:
			break
	
	assert CB and CD and CE

def unpack_update(image):
	global CF, CG
	
	block_offset = 0
	for block in range(30):
		(block_id, block_build, block_flags, block_entry_point, block_size) = struct.unpack("!2sHLLL", image[block_offset:block_offset+16])
		block_size += 0xF
		block_size &= ~0xF
		id = ord(block_id[1]) & 0xF

		print "Found %dBL (build %d) at %08x" % (id, block_build, block_offset)
		data = image[block_offset:block_offset+block_size]
		
		if id == 6:
			CF = data
		elif id == 7:
			CG = data

		block_offset += block_size

		if id == 7:
			break

def build(data):
	return struct.unpack(">H", data[2:4])[0]

def decrypt_CB(CB):
	secret = secret_1BL
	key = hmac.new(secret, CB[0x10:0x20], sha).digest()[0:0x10]
	CB = CB[0:0x10] + key + RC4.new(key).decrypt(CB[0x20:])
	return CB

def decrypt_CD(CD, CB, cpukey = None):
# enable this code if you want to extract CD from a flash image and you know the cup key.
# disable this when this is a zero-paired image.
#	assert cpukey or build(CD) < 1920
	secret = CB[0x10:0x20]
	key = hmac.new(secret, CD[0x10:0x20], sha).digest()[0:0x10]
#	if build(CD) >= 1920:
#		key = hmac.new(cpukey, key, sha).digest()[0:0x10]
	CD = CD[0:0x10] + key + RC4.new(key).decrypt(CD[0x20:])
	return CD

def decrypt_CE(CE, CD):
	secret = CD[0x10:0x20]
	key = hmac.new(secret, CE[0x10:0x20], sha).digest()[0:0x10]
	CE = CE[0:0x10] + key + RC4.new(key).decrypt(CE[0x20:])
	return CE

def decrypt_CF(CF):
	secret = secret_1BL
	key = hmac.new(secret, CF[0x20:0x30], sha).digest()[0:0x10]
	CF = CF[0:0x20] + key + RC4.new(key).decrypt(CF[0x30:])
	return CF

def decrypt_CG(CG, CF):
	secret = CF[0x330:0x330+0x10]
	key = hmac.new(secret, CG[0x10:0x20], sha).digest()[0:0x10]
	CG = CG[:0x10] + key + RC4.new(key).decrypt(CG[0x20:])
	return CG

def decrypt_SMC(SMC):
	key = [0x42, 0x75, 0x4e, 0x79]
	res = ""
	for i in range(len(SMC)):
		j = ord(SMC[i])
		mod = j * 0xFB
		res += chr(j ^ (key[i&3] & 0xFF))
		key[(i+1)&3] += mod
		key[(i+2)&3] += mod >> 8
	return res

def encrypt_CB(CB, random):
	secret = secret_1BL
	key = hmac.new(secret, random, sha).digest()[0:0x10]
	CB = CB[0:0x10] + random + RC4.new(key).encrypt(CB[0x20:])
	return CB, key

def encrypt_CD(CD, CB_key, random):
	secret = CB_key
	key = hmac.new(secret, random, sha).digest()[0:0x10]
	CD = CD[0:0x10] + random + RC4.new(key).encrypt(CD[0x20:])
	return CD, key

def encrypt_CE(CE, CD_key, random):
	secret = CD_key
	key = hmac.new(secret, random, sha).digest()[0:0x10]
	CE = CE[0:0x10] + random + RC4.new(key).encrypt(CE[0x20:])
	return CE

def encrypt_CF(CF, random):
	secret = secret_1BL
	key = hmac.new(secret, random, sha).digest()[0:0x10]
	CF_key = CF[0x330:0x330+0x10]
	CF = CF[0:0x20] + random + RC4.new(key).encrypt(CF[0x30:])
	return CF, CF_key

def encrypt_CG(CG, CF_key, random):
	secret = CF_key
	key = hmac.new(secret, random, sha).digest()[0:0x10]
	CG = CG[:0x10] + random + RC4.new(key).encrypt(CG[0x20:])
	return CG

def encrypt_SMC(SMC):
	key = [0x42, 0x75, 0x4e, 0x79]
	res = ""
	for i in range(len(SMC)):
		j = ord(SMC[i]) ^ (key[i&3] & 0xFF)
		mod = j * 0xFB
		res += chr(j)
		key[(i+1)&3] += mod
		key[(i+2)&3] += mod >> 8
	return res

import sys

for i in sys.argv[1:]:
	image = open(i, "rb").read()[:1*1024*1024]
	if image[:2] == "\xFF\x4F":
		print " * found flash image, unpacking and decrypting..."
		unpack_base_image(image)
		CB = decrypt_CB(CB)
		CD = decrypt_CD(CD, CB)
		CE = decrypt_CE(CE, CD)
		SMC = decrypt_SMC(SMC)
	elif image[:2] == "CB":
		print " * found (hopefully) decrypted CB"
		CB = image
	elif image[:2] == "CD":
		print " * found (hopefully) raw CD"
		CD = image
	elif image[:2] == "CF":
		print " * found update"
		unpack_update(image)
		CF = decrypt_CF(CF)
		CG = decrypt_CG(CG, CF)
	elif len(image) == 0x3000 and image.find("<Copyright 2001-") >= 0:
		print " * found decrypted SMC"
		SMC = image
	elif len(image) == 0x3000:
		print " * found encrypted SMC (i hope so)"
		SMC = decrypt_SMC(image)
	elif image[-0x10:] == "x" * 16:
		print " * found XeLL binary, must be linked to %08x" % CODE_BASE
		
		assert len(image) <= 256*1024
		image = (image + "\0" * 256*1024)[:256*1024]
		Xell += image
	else:
		raise " * unknown image found in file %s!" % i

print " * we found the following parts:"
print "CB:", CB and build(CB) or "missing"
print "CD:", CD and build(CD) or "missing"
print "CE:", CE and build(CE) or "missing"
print "CF:", CF and build(CF) or "missing"
print "CG:", CG and build(CG) or "missing"

open("output/CB", "wb").write(CB)
open("output/CD", "wb").write(CD)
open("output/CE", "wb").write(CE)
open("output/CF", "wb").write(CF)
open("output/CG", "wb").write(CG)
open("output/SMC", "wb").write(SMC)

def allzero(string):
	for x in string:
		if ord(x):
			return False
	return True

def allFF(string):
	for x in string:
		if ord(x) != 0xFF:
			return False
	return True

print " * checking if all files decrypted properly...",
assert allzero(CB[0x270:0x390])
assert allzero(CD[0x20:0x230])
assert allzero(CE[0x20:0x28]) 
assert allzero(CF[0x30:0x230])
assert allzero(CG[-0x20:-0x18])
assert allzero(SMC[-4:])
print "ok"

print " * checking required versions...",
assert CB and build(CB) >= 1920, "we need CB of at least 1920 (allowing zeropair-updates)"
assert CD and build(CD) == build(CB), "CD must match CB"
assert CD and build(CD) != 8453, "This CD doesn't allow 4532 or 4548 to be booted."
assert CE and build(CE) == 1888, "CE should always be 1888"
assert CF and build(CF) in [4532, 4548], "CF must be either 4532 or 4548 (exploitable)"
assert CG and build(CG) == build(CF), "CG must match CF"
print "ok"

print " * Fixing up the hacked SMC code with the target address"
offset_jtag = SMC.find("\xea\x00\xc0\x0f")
assert offset_jtag > 0, "SMC does not include the JTAG hack"
SMC = SMC[:offset_jtag+4] + struct.pack(">I", EXPLOIT_BASE) + SMC[offset_jtag+8:]

open("output/SMC", "wb").write(SMC)

xenon_builds = [1920, 1921]
zephyr_builds = [4558]
falcon_builds = [5766, 5770, 5761]
jasper_builds = [6712, 6723]

print " * this image will be valid *only* for:",
if build(CB) in xenon_builds: print "xenon",
if build(CB) in zephyr_builds: print "zephyr",
if build(CB) in falcon_builds: print "falcon",
if build(CB) in jasper_builds: print "jasper",
print

print " * zero-pairing..."

CB = CB[0:0x20] + "\0" * 0x20 + CB[0x40:]
CF = CF[0:0x21c] + "\0" * 4 + CF[0x220:]

print " * constructing new image..."

base_size = 0x8000 + len(CB) + len(CD) + len(CE)
base_size += 16383
base_size &=~16383

patch_offset = base_size

print " * base size: %x" % base_size

Final = ""

c = "zeropair image, version=%02x, CB=%d" % (SCRIPT_VERSION, build(CB))
Header = struct.pack(">HHLLL64s5LLLLLLLL", 0xFF4F, 1888, 0, 0x8000, base_size, c, 0, 0, 0, 0, 0x4000, patch_offset, 0x20712, 0x4000, 0, 0, 0x3000, 0x1000)

Header = (Header + "\0" * 0x1000)[:0x1000]
random = "\0" * 16
SMC = encrypt_SMC(SMC)
CB, CB_key = encrypt_CB(CB, random)
CD, CD_key = encrypt_CD(CD, CB_key, random)
CE = encrypt_CE(CE, CD_key, random)

# CF has an indirection table

update_size = ((len(CF) + len(CG) - 65536) + 16383) / 16384

patch_start = (patch_offset + 65536)/ 16384

table = [update_size] + range(patch_start, patch_start + update_size)

table = ''.join([struct.pack(">H", x) for x in table])

CF = CF[:0x30] + table + CF[0x30+len(table):]

open("output/CF", "wb").write(CF)


CF, CF_key = encrypt_CF(CF, random)
CG = encrypt_CG(CG, CF_key, random)

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

print " * compiling payload stub"

FLASH_BASE = 0xc8000000 + XELL_BASE_FLASH

import os
assert not os.system(CROSS_COMPILE + "gcc -nostdlib payload.S -DFLASH_BASE=0x%08x -DCODE_BASE=0x%08x -o payload.o -Ttext=0" % (FLASH_BASE, CODE_BASE))  >> 8
assert not os.system(CROSS_COMPILE + "objcopy -O binary payload.o output/payload.bin") >> 8

Exploit = open("output/payload.bin").read()

assert len(Final) < XELL_BASE_FLASH, "Please move XELL_BASE_FLASH"


if len(Xell) <= 256*1024:
	print " * No separate recovery Xell available!"
	Xell *= 2

print " * Flash Layout:"

def add_to_flash(d, w, offset = 0):
	global Final
	print "0x%08x..0x%08x (0x%08x bytes) %s" % (offset + len(Final), offset + len(Final) + len(d) - 1, len(d), w)
	Final += d

def pad_to(loc):
	global Final
	pad = "\xFF" * (loc - len(Final))
	add_to_flash(pad, "Padding")

add_to_flash(Header[:0x200], "Header")
add_to_flash(Exploit[:0x200], "Exploit")
pad_to(0x1000)
add_to_flash(SMC, "SMC")
add_to_flash(Keyvault, "Keyvault")
add_to_flash(CB, "CB %d" % build(CB))
add_to_flash(CD, "CD %d" % build(CD))
add_to_flash(CE, "CE %d" % build(CE))
pad_to(patch_offset)
add_to_flash(CF, "CF %d" % build(CF))
add_to_flash(CG, "CG %d" % build(CG))
pad_to(XELL_BASE_FLASH)

add_to_flash(Xell[0:256*1024], "Xell (backup)")
add_to_flash(Xell[256*1024:], "Xell (main)")

print " * Encoding ECC..."

Final = addecc(Final)

exploit_base = EXPLOIT_BASE/0x200*0x210

if exploit_base < len(Final):
	Final = Final[:exploit_base]  + addecc(Final[exploit_base:exploit_base + 0x200], 0, struct.pack(">I", 0x350)) + Final[exploit_base + 0x210:]

open("output/image_00000000.ecc", "wb").write(Final)

print "------------- Written into output/image_00000000.ecc"

if exploit_base >= len(Final):
	Final = ""
	add_to_flash(Exploit, "Exploit buffer", EXPLOIT_BASE)
	open("output/image_%08x.ecc" % EXPLOIT_BASE, "wb").write(addecc(Final, 0x50030000 * 32))
	print "------------- Written into output/image_%08x.ecc" % EXPLOIT_BASE

print " ! please flash output/image_*.ecc, and setup your JTAG device to do the DMA read from %08x" % (EXPLOIT_BASE)
