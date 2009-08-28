#
# This tools creates the 1921 CD from a 1920 CD and a 1921 CB.
# It basically applies a series of binary patches to create the
# resulting binary, which is identical to the real binary.
#
# Several output builds are supported (not just 1921), so this can
# generate the CD files for Zephyr and Falcon, too. (But not Jasper)
#
# The required patches were found out manually by looking at what has
# changed from CB1920 to CB1921, and applying this knowledge to a CD1920.
# until it matched the target hash.

import struct, sys

TARGET = int(sys.argv[1])

MEMDIFF = 1
NO_HANG = 2

targets = {
	1920: ("\xc4\x5e\x1a\x1b\x5c\xc2\xab\x59\x3f\x81\x24\xf2\xcc\xaa\xe7\xdc\x38\xca\xa8\xd2", 0),
	1921: ("\xa9\xf9\x0c\x32\x56\x10\x84\xa3\x03\x4e\xae\x9c\x86\x7b\x65\x9c\x98\x6b\xc7\x67", MEMDIFF),
	4558: ("\x49\xe8\x73\xbf\x10\x48\x64\x35\x69\xea\xe6\x76\xa2\xe3\xf0\x41\x65\x71\xd2\x21", NO_HANG),
	5761: ("\x1a\x1c\xd4\xfb\x8b\x23\x54\x4f\xff\x08\x08\x54\x3e\x0c\x18\xd9\x39\xa1\x65\xb1", NO_HANG),
	5766: ("\x3d\x49\xe6\x11\xf5\x85\x12\x92\x5c\x0d\x16\xf4\xc5\x39\x68\xb9\x08\xf4\x3f\x4a", MEMDIFF|NO_HANG),
	5770: ("\xa1\xd5\xc2\x0f\x4a\x63\x28\xb3\x7c\x45\x9a\xa5\x5a\x7b\x2e\xd6\x76\xdb\x39\xe0", MEMDIFF|NO_HANG),
}

target_data = targets[TARGET]

cd1920 = open("input/CD.1920", "rb").read()
cb1921 = open("input/CB.1921", "rb").read()

def set(x, v):
	global cd1920
	cd1920 = cd1920[:x] + v + cd1920[x+len(v):]

set(0x2, struct.pack(">H", TARGET)) # set build
# r1 is a lookup table to convert old into new offset.
r1 = range(len(cd1920))

if target_data[1] & MEMDIFF:
	set(0xbb7, chr(ord(cd1920[0xbb7]) * 4)) # memdiff works on bytes, not on dwords
	set(0xbc8, chr(ord(cd1920[0xbc8]) ^ 1))  # memcmpd is inverted to memdiff

	# grab memdiff and some glue code from CB.1921
	memdiff = cb1921[0x9090:0x90d0]
	check_hash_glue = cb1921[0x83ec:0x8404]

	# now remove memcmp, memcmpd and add memdiff. Also add the modified glue code for memcmpd.
	cd1920 = cd1920[0:0xf90] + cd1920[0xfd0:0x193c] + check_hash_glue + cd1920[0x1958:0x197c] + cd1920[0x1980:0x1bd0] + "\x00" * 8 + cd1920[0x1bd0:0x2690] +  memdiff + cd1920[0x2690:0x56c0]

	set(0xc, struct.pack(">L", len(cd1920))) # set new length

	# update our lookup table
	r1 = r1[0:0xf90] + r1[0xfd0:0x193c] + len(check_hash_glue) * [-1] + r1[0x1958:0x197c] + r1[0x1980:0x1bd0] + [-1] * 8 + r1[0x1bd0:0x2690] +  len(memdiff)  * [-1] + r1[0x2690:0x56c0]

	# the gluecode jumps to memdiff, which we added, so fixup that offset.
	set(0x1912, "\x0d\x41") # jump to memdiff

assert len(r1) == len(cd1920)

# now we need to fixup jumps, so convert to long
b1 = struct.unpack(">%dI" % (len(cd1920)/4), cd1920)

result =""

nr_fix_memcpy = 0

for i in range(len(b1)):
	a1 = b1[i]

	target = 0
	off = i * 4
	
	d = 0

	# if we have a relative jump/call, and that call doesn't come from newly inserted code (we fixed up that one manually), process it.
	if a1 & 0xFC000000 == 0x48000000 and r1[off] != -1:
		# first, calculate the jump target. Keep in mind that we relocated code, so use the old offset as base.
		target = r1[off] + a1 & 0xFFFC
		
		if target_data[1] & NO_HANG and not (a1 & 0xFFFC):
			if nr_fix_memcpy == 0:
				target -= 0x150
			elif nr_fix_memcpy == 1:
				target -= 0xfc
			elif nr_fix_memcpy == 2:
				target -= 0xdc
			nr_fix_memcpy += 1
		
		# signed jumps
		if target > 0x4000000:
			target -= 0x8000000

		# now calculate relocated jump target, i.e. the new offset of the old target
		# (unless the jump was to memcmp, which doesn't exist anymore, so redirect that to memdiff)
		if target not in r1:
			new_target = 0x2650
		else:
			new_target = r1.index(target)

		# update branch
		a1 = ((new_target - off) & 0x03fffffc) | (a1 & 0xFC000003L)

	result += struct.pack(">I", a1)

# check if hash matches

import sha, struct, sys

MASK64 = 0xFFFFFFFFFFFFFFFF

def rotate(data, c):
	return ((data << c) | (data >> (64 - c))) & MASK64

def n(x):
	return x ^ MASK64

def rotsum(data):
	v0, v1, v2, v3 = 0,0,0,0
	
	while len(data):
		w = struct.unpack(">Q", data[:8])[0]
		data = data[8:]
		v0 += w
		v0 &= MASK64

		if v0 < w:
			v1 += 1
		
		v0 = rotate(v0, 29)
		
		v2 -= w
		v2 &= MASK64
		
		if v2 > w:
			v3 -= 1

		v2 = rotate(v2, 31)

	v2 &= MASK64
	v3 &= MASK64
	return struct.pack(">QQQQ", v1, v0, v3, v2), struct.pack(">QQQQ", n(v1), n(v0), n(v3), n(v2))

p4 = result
rs = rotsum(p4[0:0x10] + p4[0x120:])

p4_hash = rs[0] * 2+ p4[0:0x10] + p4[0x120:] + rs[1] * 2
p4_hash_cal = sha.new(p4_hash).digest()

if p4_hash_cal == target_data[0]:
	print "great, hash matches!"
	open("input/CD.%d" % TARGET, "wb").write(result)
else:
	print "Something failed, hash mismatch"
