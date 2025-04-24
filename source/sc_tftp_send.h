#ifndef _SC_TFTP_SEND_H_
#define _SC_TFTP_SEND_H_

typedef struct sTftpSendArgs
{
    char Title[64];
    int (*StartCB)(void* ctx, const char* filename, const char* mode);
    int (*ReadCB)(void* ctx, int pos, void* data, int len);
    void (*FinishCB)(void* ctx);
    void (*ErrorCB)(void* ctx, u16 code, const char* msg);

} sTftpSendArgs;

extern sScreen scTftpSend;

void ScTftpSend_Open(void* data);
void ScTftpSend_Close();
void ScTftpSend_Activate();
void ScTftpSend_Update();

#endif // _SC_TFTP_SEND_H_
