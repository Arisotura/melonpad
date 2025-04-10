#include <string.h>
#include <unistd.h>
#include "tftp.h"


static int _retval(int res)
{
    if (res < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -1;
        return 0;
    }
    return 1;
}

int TftpRecvFrame(sTftpContext* ctx, int chk_addr, void* data, int len)
{
    struct sockaddr_in raddr;
    socklen_t raddr_size = sizeof(struct sockaddr_in);

    for (;;)
    {
        int res = recvfrom(ctx->Socket, data, len, 0, (struct sockaddr*)&raddr, &raddr_size);
        if (res < 0) return _retval(res);

        if (chk_addr)
        {
            if ((raddr.sin_family != ctx->Client.sin_family) ||
                (raddr.sin_port != ctx->Client.sin_port) ||
                (memcmp(&raddr.sin_addr, &ctx->Client.sin_addr, sizeof(struct in_addr))))
            {
                // bad address, ignore
                continue;
            }
        }
        else
            memcpy(&ctx->Client, &raddr, sizeof(struct sockaddr_in));

        return 1;
    }
}

int TftpSendData(sTftpContext* ctx, u16 blkid, const void* data, int len)
{
    int txlen = 4 + len;
    u8 txdata[516];
    *(u16*)&txdata[0] = htons(3);
    *(u16*)&txdata[2] = htons(blkid);
    if (len) memcpy(&txdata[4], data, len);

    int res = sendto(ctx->Socket, txdata, txlen, 0, (struct sockaddr*)&ctx->Client, sizeof(struct sockaddr_in));
    return _retval(res);
}

int TftpSendError(sTftpContext* ctx, u16 code, const char* msg)
{
    int txlen = 4 + strlen(msg) + 1;
    u8 txdata[256] = {0};
    *(u16*)&txdata[0] = htons(5);
    *(u16*)&txdata[2] = htons(code);
    strncpy((char*)&txdata[4], msg, 251);

    int res = sendto(ctx->Socket, txdata, txlen, 0, (struct sockaddr*)&ctx->Client, sizeof(struct sockaddr_in));
    return _retval(res);
}

void TftpHandleError(sTftpContext* ctx, int senderror, u16 code, const char* msg)
{
    if (senderror) TftpSendError(ctx, code, msg);
    close(ctx->Socket);
    if (ctx->ErrorCB) ctx->ErrorCB(code, msg);
}
