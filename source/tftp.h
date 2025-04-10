#ifndef _TFTP_H_
#define _TFTP_H_

#include <wup/wup.h>
#include <lwip/sockets.h>

#define TFTP_PORT 69

typedef int (*fnTftpStartCB)(const char* filename, const char* mode);
typedef int (*fnTftpReadCB)(int position, void* data, int len);
typedef void (*fnTftpFinishCB)();
typedef void (*fnTftpErrorCB)(u16 code, const char* msg);

typedef struct sTftpContext
{
    void* Thread;
    int Socket;
    int Port;
    struct sockaddr_in Client;

    fnTftpStartCB StartCB;
    fnTftpReadCB ReadCB;
    fnTftpFinishCB FinishCB;
    fnTftpErrorCB ErrorCB;

    volatile u8 Abort;

} sTftpContext;

int TftpRecvFrame(sTftpContext* ctx, int chk_addr, void* data, int len);
int TftpSendData(sTftpContext* ctx, u16 blkid, const void* data, int len);
int TftpSendError(sTftpContext* ctx, u16 code, const char* msg);
void TftpHandleError(sTftpContext* ctx, int senderror, u16 code, const char* msg);

void* TftpSendStart(int port, fnTftpStartCB startcb, fnTftpReadCB readcb, fnTftpFinishCB finishcb, fnTftpErrorCB errorcb);
void TftpFinish(void* ctx);
void TftpAbort(void* ctx);

#endif // _TFTP_H_
