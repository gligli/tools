extern WORD XNANDReadStart(DWORD block);
extern void XNANDReadProcess(BYTE *buffer, BYTE words);

extern WORD XNANDErase(DWORD block);

extern void XNANDWriteStart(void);
extern void XNANDWriteProcess(BYTE *buffer, BYTE words);
extern WORD XNANDWriteExecute(DWORD block);
