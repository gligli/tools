
extern void XSPIInit(void);

extern void XSPIEnterFlashMode(void);
extern void XSPILeaveFlashMode(void);

extern BYTE XSPI(BYTE o);
extern void XSPIRead(BYTE reg, BYTE *data);
extern void XSPIWrite(BYTE reg, BYTE *data);

//This are optimized special cases for the functions above
extern void XSPIW(BYTE o);
extern void XSPIW0(void);
extern BYTE XSPIR(void);
extern void XSPIWrite0(BYTE reg);
extern void XSPIWriteByte(BYTE reg, BYTE d);



