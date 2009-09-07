extern void FlashPollProc(void);
extern void FlashInitEP(void);
extern void FlashCheckVendorReq(void);

#define FlashTxSetCBF(pCBF,len)    \
{                                   \
    FlashTxCBF = pCBF;              \
    FlashTxLen = len;               \
    FlashTxState = FLASH_TX_BUSY;    \
}


