# modified by GliGli and Tiros for the reset glitch hack

# you might need to fill in this
secret_1BL = ""
cpukey = ""

XELL_BASE_FLASH = 0xc0000
CODE_BASE = 0x1c000000

# don't change anything from here.

# so we can do updates properly
SCRIPT_VERSION = 0x01

Keyvault = None
SMC = None
CB_A = None
CB_B = None
CD = None
CD_plain = None
CE = None
CF = None
CG = None
Xell = ""
Exploit = None

#if secret_1BL is None:
#	secret_1BL = open("key_1BL.bin", "rb").read()

 # Import Psyco if available
try:
	import psyco
	psyco.full()
except ImportError:
	pass

# first, unpack base input image. We are ignoring any updates here
import hmac, sha, struct, sys, binascii
try:
	import Crypto.Cipher.ARC4 as RC4
except ImportError:
	print "Error importing Crypto.Cipher.ARC4 - please install python-crypto!"
	print "You can get it from http://www.dlitz.net/software/pycrypto/"
	sys.exit(-1)

def unpack_base_image(image):
	global SMC, CB_A, CB_B, CD, CE, Keyvault

	if image[0x205] == "\xFF" or image[0x415] == "\xFF" or image[0x200] == "\xFF":
		print "ECC'ed - will unecc."
		res = ""
		for s in range(0, len(image), 528):
			res += image[s:s+512]
		image = res

	unpackstring = "!HHLLL64s5LLLLLLLL"
	(id1, build, flags, bloffset, size0, copyright, z0, z1, z2, z3, r7, size1, r3, r4, z5, z6, smc_len, smc_start) = struct.unpack(unpackstring, image[:struct.calcsize(unpackstring)])
	#assert not (z0 or z1 or z2 or z3 or z5 or z6), "zeros are not zero."

	block_offset = bloffset

	SMC = image[smc_start:smc_start+smc_len]
	Keyvault = image[0x4000:0x8000]

	assert smc_len == 0x3000, "never saw an SMC != 0x3000 bytes"
	semi = 0
	for block in range(30):
		(block_id, block_build, block_flags, block_entry_point, block_size) = struct.unpack("!2sHLLL", image[block_offset:block_offset+16])
		block_size += 0xF
		block_size &= ~0xF
		id = ord(block_id[1]) & 0xF

		print "Found %dBL (build %d) at %08x" % (id, block_build, block_offset)
		data = image[block_offset:block_offset+block_size]
		
		if id == 2:
			if semi == 0:
				CB_A = data
				semi = 1
			elif semi ==1:
				CB_B = data
				semi = 0

		elif id == 4:
			CD = data
		elif id == 5:
			CE = data
		
		block_offset += block_size

		if id == 5:
			break

	assert CB_A and CD

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

def decrypt_CB_Cpu(CB):
	assert cpukey
	secret = CB_A[0x10:0x20]
	h = hmac.new(secret,None, sha);
	h.update(CB[0x10:0x20]);
	h.update(cpukey);
	key = h.digest()[0:0x10]
	CB = CB[0:0x10] +key+ RC4.new(key).decrypt(CB[0x20:])
	return CB

def decrypt_CD(CD, CB):
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

def encrypt_CB_Cpu(CB):
	assert cpukey
	secret = CB_A[0x10:0x20]
	h = hmac.new(secret,None, sha);
	h.update(CB[0x10:0x20]);
	h.update(cpukey);
	key = h.digest()[0:0x10]
	CB = CB[0:0x20] + RC4.new(key).encrypt(CB[0x20:])
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

# CB_patches is an array of patchsets, a patchset is a version number followed by an array of patches, a patch is 3 ints: [offset,plaintext,patch]

CB_patches = [[9188,[[0x4f08,0x409a0010,0x60000000],[0x5618,0x480018e1,0x60000000],[0x5678,0x480000b9,0x60000000]]]]

def int_to_str(i):
	return [chr((i>>24) & 0xff),chr((i>>16) & 0xff),chr((i>>8) & 0xff),chr(i & 0xff)]

def patch_CB(CB):
	found = False

	for versions in CB_patches:
		if build(CB) == versions[0]:
			print "patchset for %d found, %d patch(es)" % (versions[0],len(versions[1]))
			found  = True
			for patches in versions[1]:
				plain = int_to_str(patches[1])
				patch = int_to_str(patches[2])

				patched = ""

				for i in range(4):
					keystream = ord(plain[i]) ^ ord(CB[i+patches[0]])
					patched = patched + chr(keystream ^ ord(patch[i]))

				CB = CB[:patches[0]] + patched + CB[patches[0]+4:]

	assert found,"can't patch that CB"

	return CB

# SMC_patches is an array of patchsets, a patchset is a crc32 of the image minus first 4 bytes, human readable version info and an array of patches, a patch is: [offset,byte0,byte1,...]

SMC_patches = [[0xf9c96639,"Trinity (for slims), version 3.1",[[0x13b3,0x00,0x00]]],
               [0x5b3aed00,"Jasper (for all hdmi fats), version 2.3",[[0x12ba,0x00,0x00]]]]

def patch_SMC(SMC):
	found = False

	for versions in SMC_patches:
		if binascii.crc32(SMC[4:]) & 0xffffffff == versions[0]:
			print "patchset \"%s\" matches, %d patch(es)" % (versions[1],len(versions[2]))
			found  = True
			for patches in versions[2]:
			    for i in range(len(patches)-1):
					SMC = SMC[:patches[0]+i] + chr(patches[i+1]) + SMC[patches[0]+i+1:]

	if not found:
		print" ! Warning: can't patch that SMC, here are the current supported versions:"
		for versions in SMC_patches:
			print "  - %s" % versions[1]

	return SMC


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

import sys

for i in sys.argv[1:]:
	image = open(i, "rb").read()[:1*1024*1024]
	if image[:2] == "\xFF\x4F":
		print " * found flash image, unpacking..."
		unpack_base_image(image)
		CB_A_crypted = CB_A
		SMC = decrypt_SMC(SMC)
	elif image[:2] == "CD" and allzero(image[0x20:0x230]):
		print " * found decrypted CD"
		CD_plain = image
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
print "SMC: %d.%d" %(ord(SMC[0x101]),ord(SMC[0x102]))
print "CB_A:", CB_A and build(CB_A) or "missing"
print "CB_B:", CB_B and build(CB_B) or "missing"
print "CD (image):", CD and build(CD) or "missing"
print "CD (decrypted):", CD_plain and build(CD_plain) or "missing"

print " * decrypting..."

CB_A = decrypt_CB(CB_A)
print " * checking if all files decrypted properly...",
assert allzero(SMC[-4:])
print "ok"

print " * checking required versions...",
assert CD_plain, "you need a decrypted CD"
print "ok"

xenon_builds = []
zephyr_builds = [4578]
falcon_builds = []
jasper_builds = [6750]
trinity_builds = [9188]

print " * this image will be valid *only* for:",
if build(CB_A) in xenon_builds: print "xenon",
if build(CB_A) in zephyr_builds: print "zephyr",
if build(CB_A) in falcon_builds: print "falcon",
if build(CB_A) in jasper_builds: print "jasper",
if build(CB_A) in trinity_builds: print "trinity (slim)",
print

Final = ""

print " * patching SMC..."
SMC=patch_SMC(SMC)

CD = CD_plain

if CB_B:
	print " * patching CB_B..."

	CB_B = patch_CB(CB_B)

	c = "patched CB img"
else:
	print " * zero-pairing..."

	CB_A = CB_A[0:0x20] + "\0" * 0x20 + CB_A[0x40:]

	c = "zeropair image"

open("output/CB_A.bin", "wb").write(CB_A)
if CB_B:
	open("output/CB_B.bin", "wb").write(CB_B)
open("output/SMC.bin", "wb").write(SMC)

print " * constructing new image..."

base_size = 0x8000 + len(CB_A) + len(CD) + len(CE)
if CB_B:
	base_size += len(CB_B)
base_size += 16383
base_size &=~16383

patch_offset = base_size

print " * base size: %x" % base_size

c += ", version=%02x, CB=%d" % (SCRIPT_VERSION, build(CB_A))
Header = struct.pack(">HHLLL64s5LLLLLLLL", 0xFF4F, 1888, 0, 0x8000, base_size, c, 0, 0, 0, 0, 0x4000, patch_offset, 0x20712, 0x4000, 0, 0, 0x3000, 0x1000)

Header = (Header + "\0" * 0x1000)[:0x1000]
random = "\0" * 16

SMC = encrypt_SMC(SMC)

if not CB_B:
	CB_A, CB_A_key = encrypt_CB(CB_A, random)
	CD, CD_key = encrypt_CD(CD, CB_A_key, random)

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
pad_to(0x1000)
add_to_flash(SMC, "SMC")
add_to_flash(Keyvault, "Keyvault")

if CB_B:
	add_to_flash(CB_A_crypted, "CB_A %d" % build(CB_A))
	add_to_flash(CB_B, "CB_B %d" % build(CB_B))
else:
	add_to_flash(CB_A, "CB_A %d" % build(CB_A))

add_to_flash(CD, "CD %d" % build(CD))
pad_to(XELL_BASE_FLASH)
add_to_flash(Xell[0:256*1024], "Xell (backup)")
add_to_flash(Xell[256*1024:], "Xell (main)")

print " * Encoding ECC..."

Final = addecc(Final)

open("output/image_00000000.ecc", "wb").write(Final)

print "------------- Written into output/image_00000000.ecc"
