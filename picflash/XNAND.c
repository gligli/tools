#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "XSPI.h"

WORD XNANDReadStart(DWORD block)
{
	WORD res;
	WORD tries = 0x1000;
	BYTE tmp[4];

	XSPIRead(4, tmp);
	XSPIWrite(4, tmp);

	block <<= 9;

	tmp[0] = ((BYTE *)&block)[0];
	tmp[1] = ((BYTE *)&block)[1];
	tmp[2] = ((BYTE *)&block)[2];
	tmp[3] = ((BYTE *)&block)[3];
	XSPIWrite(0x0C, tmp);

	XSPIWriteByte(0x08, 0x03);

	while(tries--) {
		XSPIRead(0x04, tmp);
		
		if (!(tmp[0] & 0x01))
			break;
	}

	((BYTE *)&res)[0] = tmp[1];
	((BYTE *)&res)[1] = tmp[0];

	if (tmp[0] & 0x01)
		res |= 0x8000;

	XSPIWrite0(0x0C);

	return res;
}

void XNANDReadProcess(BYTE *buffer, BYTE words) {
	BYTE tmp[4];

	while (words--) {
		XSPIWrite0(0x08);
		XSPIRead(0x10, buffer);
		buffer+=4;
	}
}

WORD XNANDErase(DWORD block)
{
	WORD res;
	WORD tries = 0x1000;
	BYTE tmp[4];

	while (tries--) {
		XSPIRead(4, tmp);
		XSPIWrite(4, tmp);
	
		XSPIRead(0, tmp);
		tmp[0] |= 0x08;
		XSPIWrite(0, tmp);
	
		block <<= 9;
	
		tmp[0] = ((BYTE *)&block)[0];
		tmp[1] = ((BYTE *)&block)[1];
		tmp[2] = ((BYTE *)&block)[2];
		tmp[3] = ((BYTE *)&block)[3];
		XSPIWrite(0x0C, tmp);
	
		XSPIWriteByte(0x08, 0xAA);
		XSPIWriteByte(0x08, 0x55);
		XSPIWriteByte(0x08, 0x5);

		XSPIRead(0x04, tmp);
		
		if (!(tmp[0] & 0x01))
			break;
	}

	((BYTE *)&res)[0] = tmp[1];
	((BYTE *)&res)[1] = tmp[0];

	if ((tmp[0] != 0x00) || (tmp[1] != 0x02))
		res |= 0x8000;

	return res;
}

void XNANDWriteStart()
{
	BYTE tmp[4];

	XSPIRead(4, tmp);
	XSPIWrite(4, tmp);

	XSPIWrite0(0x0C);
}

void XNANDWriteProcess(BYTE *buffer, BYTE words) {
	BYTE tmp[4];
	while (words--) {
		tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0x55;
		XSPIWrite(0x10, buffer);
		XSPIWriteByte(0x08, 0x01);
		buffer += 4;
	}
}

WORD XNANDWriteExecute(DWORD block) {
	WORD res;
	WORD tries = 0x1000;
	BYTE tmp[4];
	block <<= 9;

	tmp[0] = ((BYTE *)&block)[0];
	tmp[1] = ((BYTE *)&block)[1];
	tmp[2] = ((BYTE *)&block)[2];
	tmp[3] = ((BYTE *)&block)[3];
	XSPIWrite(0x0C, tmp);

	XSPIWriteByte(0x08, 0x55);
	XSPIWriteByte(0x08, 0xAA);
	XSPIWriteByte(0x08, 0x4);

	while(tries--) {
		XSPIRead(0x04, tmp);
		
		if (!(tmp[0] & 0x01))
			break;
	}

	((BYTE *)&res)[0] = tmp[1];
	((BYTE *)&res)[1] = tmp[0];

	if ((tmp[0] != 0x00) || (tmp[1] != 0x02))
		res |= 0x8000;

	return res;	
}
