#define PHY_OFFSET 0x130360
#define V(x) ((x) + 0x80000000 + PHY_OFFSET)
#define P(x) ((x) + PHY_OFFSET)
// we are at 130360 here.
_start:
.globl _start
. = 0

//.long 0x00000000      // 0
//.long V(stack)        // stack pointer
//.long 0x00000000      // 0
//.long 0x00000000      // 0

.long 0x80131db0 // stack limit
//.long 0x3a009f10
.long V(stack)        // stack pointer
.long 0x00000000
.long 0x02000000

. = 0x10

.long 0x12000000
.long 0x80130374
.long 0x80130374
.long 0x8013037c

. = 0x20

.long 0x8013037c
.long 0x80125020
.long 0x00000001
.long 0x00000000

. = 0x30

.long 0x00000005
//.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0xfdffd7ff

#if 0
. = 0x40

.long 0x00000000
.long 0x02000000
.long 0x00000000
.long 0x00000000

. = 0x50

.long 0x00000000
.long 0x0000007f
.long 0x00000000
.long 0x00000000

. = 0x60

.long 0x80130100
.long 0x80130100
.long 0x00000000
.long 0x00000000

. = 0x70

.long 0x80135db0
.long 0x00120000
.long 0x80130300
.long 0x00000000

. = 0x80

.long 0x00000000
.long 0x8006d2b0
.long 0x00000000
.long 0x800714c8

. = 0x90

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x05000000

. = 0xA0


.long 0x00000000
.long 0x80130404
.long 0x80130404
.long 0x00000002

. = 0xB0

.long 0x80131410
.long 0x80125024
.long 0x00000000
.long 0x00000000

#endif

#if 0
. = 0xC0

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00000000

. = 0xD0

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00000000

. = 0xE0

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00000000

. = 0xF0

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00000000

. = 0x100

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00000000


. = 0x110

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00010000

.long 0x00000000
.long 0x00000000
.long 0x00000000
.long 0x00000000

. = 0x130
#endif 

stack:

#if 0
// 00
.long 0, 0, 0, 0
// 10
.long 0, 0, 0, 0
// 20
.long 0, 0, 0, 0
// 30
.long 0, 0, 0, 0
// 40

#endif

	/* our registers are largely prefilled form the stack; see below. */
exploit_code:

		/* send SMC query for powerup reason */
	stw %r9, 0x1084(%r8)  /* 00000004 (byteswapped) */
	stw %r10, 0x1080(%r8) /* 01000000 */
	stw %r11, 0x1080(%r8) /* 00000000 */ 
	stw %r11, 0x1080(%r8) /* 00000000 */
	stw %r11, 0x1080(%r8) /* 00000000 */
	stw %r11, 0x1084(%r8) /* 00000000 */

		/* wait for SMC answer */
1:	
	lwz %r12, 0x1094(%r8)
	and. %r12, %r12, %r9   /* check for 04 (swapped) */
	beq 1b
	stw %r9, 0x1094(%r8)  /* 00000004 (byteswapped) */
	lwz %r12, 0x1090(%r8)
	lwz %r3, 0x1090(%r8)
	lwz %r3, 0x1090(%r8)
	lwz %r3, 0x1090(%r8)
	stw %r11, 0x1094(%r8) /* 00000000 */
	rlwinm %r3, %r12, 8, 24, 31
	cmpwi %r3, 0x1
	bne 1b

b final /* we don't have more space here, so continue below... */

#if 0
. = stack + 0x40
.long 0, 0, 0, 0
#endif

// 50
. = stack + 0x50
		/* Context restore at 800701c0 loads r0..r12 from here */
.long 0x20000000, 0x00000046+2, 0x00000000, V(stack) // r0, r1

// 60
. = stack + 0x60
.long 0, 0, 0, 0xe0  // r2, r3
// 70
. = stack + 0x70
.long 0x80000000, P(exploit_code), 0x80000200, FLASH_BASE // r4, r5

// 80  r6, r7
.long 0x80000000, CODE_BASE, 0x80000200, 0x61010
// 90 r8, r9
.long 0x80000200, 0xea000000, 0, 0x04000000
// A0 r10, r11
.long 0, 0x01000000, 0, 0
// B0
.long 0, 0, 0, 0
// C0
.long 0, 0, 0, 0
// D0
.long 0, 0, 0, 0
// E0
. = stack + 0xE0
.long 0, 0x80070190, 0, 0 // NIP to context restore
//.long 0, 0x80060c90, 0, 0

final:

	rlwinm %r3, %r12, 16, 24, 31
	cmpwi %r3, 0x5a /* poweron by pressing guide button while connected to the back USB */
	beq flash_loader
	cmpwi %r3, 0x24 /* poweron by pressing the windows button on the IR remote */
	beq flash_loader

good:
	addis %r5, %r5, 0x4

flash_loader:
	/* POST = 0x10 */
li %r3, 0x10
rldicr %r3, %r3, 56, 7
std %r3, 0(%r7)

	/* Copy from Flash, src = %r5, dst = %r6 */
		mtlr %r6
		lis %r4, 1 /* 256k */
		mtctr   %r4

1:	lwz     %r8, 0(%r5)		//Memcopy
		stw     %r8, 0(%r6)
		dcbst   %r0, %r6		//Flush cache to ram
		icbi	%r0, %r6
		sync	0
		isync
		addi    %r6, %r6, 4
		addi    %r5, %r5, 4
		bdnz    1b

	/* POST = 0x11 */
li %r3, 0x11
rldicr %r3, %r3, 56, 7
std %r3, 0(%r7)

		blr

#if 0
// F0
.long 0, 0, 0, 0
// 100
.long 0, 0, 0, 0
// 110
.long 0, 0, 0, 0
// 120
.long 0, 0, 0, 0
// 130
.long 0, 0, 0, 0
// 140
.long 0, 0, 0, 0
// 150
.long 0, 0, 0, 0
// 160
.long 0, 0, 0, 0
// 170
.long 0, 0, 0, 0
// 180
.long 0, 0, 0, 0
// 190
.long 0, 0, 0, 0
#endif
. = stack + 0x1a0
// 1a0
.long 0, 0, 0x80070228, 0x80070228 // NIP, LR, points to SC in kernel
// 1b0
. = stack + 0x1b0
.long 0, 0x9030, 0, 0 // MSR

. = 0x4000
