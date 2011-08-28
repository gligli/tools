#include "xenonsprs.h"

.macro PATCH Address
	.set	Pc, \Address
	.quad	\Address
	.long	(9f-8f)/4 //Length of patch in words
.endm

_start:
.globl _start

bl init_regs

/* POST = 0x10 */
li	%r3, 0x10
rldicr	%r3, %r3, 56, 7
std	%r3, 0(%r7)

bl init_pci

	/* send SMC query for powerup reason */
stw	%r9, 0x1084(%r8)  /* 00000004 (byteswapped) */
stw	%r10, 0x1080(%r8) /* 01000000 */
stw	%r11, 0x1080(%r8) /* 00000000 */
stw	%r11, 0x1080(%r8) /* 00000000 */
stw	%r11, 0x1080(%r8) /* 00000000 */
stw	%r11, 0x1084(%r8) /* 00000000 */

	/* wait for SMC answer */
1:
	lwz	%r12, 0x1094(%r8)
	and.	%r12, %r12, %r9   /* check for 04 (swapped) */
	beq	1b
	stw	%r9, 0x1094(%r8)  /* 00000004 (byteswapped) */
	lwz	%r12, 0x1090(%r8)
	lwz	%r3, 0x1090(%r8)
	lwz	%r3, 0x1090(%r8)
	lwz	%r3, 0x1090(%r8)
	stw	%r11, 0x1094(%r8) /* 00000000 */
	rlwinm	%r3, %r12, 8, 24, 31
	cmpwi	%r3, 0x1
	bne	1b

	/* set HRMOR=0 for main core */
mtspr	hrmor,%r11

/* POST = 0x11 */
li	%r3, 0x11
rldicr	%r3, %r3, 56, 7
std	%r3, 0(%r7)

	/* Copy from Flash, src = %r5, dst = %r6 */

rlwinm	%r3, %r12, 16, 24, 31
cmpwi	%r3, 0x5a /* poweron by pressing guide button while connected to the back USB (lowest one on slim) */
beq	backup_xell
cmpwi	%r3, 0x24 /* poweron by pressing the windows button on the IR remote */
beq	backup_xell

mr	%r5,%r15

backup_xell:

mr 	%r9,%r6
lis	%r4, 1 /* 256k */
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

/* POST = 0x12 */
li	%r3, 0x12
rldicr	%r3, %r3, 56, 7
std	%r3, 0(%r7)

	/* Clear IR/DR and jump to Xell*/
li	%r4, 0x30
mfmsr	%r3
andc	%r3, %r3, %r4
mtsrr1	%r3
mtsrr0	%r9
rfid

init_regs:
	/* init regs */
	li	%r5,0x200
	oris	%r5,%r5,0x8000
	rldicr	%r5,%r5,32,31
	oris 	%r5,%r5,0xc80c

	li	%r15,0x200
	oris	%r15,%r15,0x8000
	rldicr	%r15,%r15,32,31
	oris 	%r15,%r15,0xc810

	lis	%r6,0x8000
	rldicr	%r6,%r6,32,31
	oris 	%r6,%r6,0x1c00

	li	%r7,0x200
	oris	%r7,%r7,0x8000
	rldicr	%r7,%r7,32,31
	ori	%r7,%r7,0x1010
	oris 	%r7,%r7,6

	li	%r8,0x200
	oris	%r8,%r8,0x8000
	rldicr	%r8,%r8,32,31
	oris 	%r8,%r8,0xea00

	lis	%r9,0x0400

	lis	%r10,0x0100

	li	%r11,0

	lis	%r20,0x8000
	ori	%r20,%r20,0x17c
	rldicr	%r20,%r20,32,31
	oris	%r20,%r20,0x400
	ori	%r20,%r20,0x270 /* offset to CD code start : 0x8000.017c.0400.0270 */

	blr

init_pci:
	/* init PCI devices */
	addi	%r3,%r20,pci_inits-_start-4
	
	li	%r30,0x200
	oris	%r30,%r30,0x8000
	rldicr	%r30,%r30,32,31
	pci_start:
		lwzu	%r4,4(%r3)
		cmpwi	%r4,-1
		beqlr
		lwzu    %r31,4(%r3)
		stwbrx	%r31,%r30,%r4
		li	%r31,0xff
		andc	%r4,%r4,%r31
		ori	%r4,%r4,4
		lwbrx	%r31,%r30,%r4
		lwzu    %r29,4(%r3)
		or	%r31,%r31,%r29
		stwbrx	%r31,%r30,%r4
		b pci_start
		
pci_inits:
/* pci-pci bridge */
.long 0xd0000010,0xea000000,0x156
/* host bridge */
.long 0xd0008010,0xe0000000,2
/* GPU */
.long 0xd0010010,0xec800000,2
/* 1414:5801 */
.long 0xd0100010,0xea001800,2
/* SATA */
.long 0xd0108010,0xea001200,6
.long 0xd0108014,0xea001220,6
.long 0xd0110010,0xea001300,6
.long 0xd0110014,0xea001320,6
/* USB */
.long 0xd0120010,0xea002000,0x156
.long 0xd0121010,0xea003000,0x106
.long 0xd0128010,0xea004000,0x156
.long 0xd0129010,0xea005000,0x106
/* Ethernet */
.long 0xd0138010,0xea001400,6
/* System flash controller */
.long 0xd0140010,0xea00c000,6
.long 0xd0140014,0xc8000000,6
/* Audio */
.long 0xd0148010,0xea001600,6
/* SMC */
.long 0xd0150010,0xea001000,2
.long 0xffffffff /* structure end */
