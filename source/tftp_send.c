#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tftp.h"

static void TftpSendThread(void* userdata);


void* TftpSendStart(int port, fnTftpStartCB startcb, fnTftpReadCB readcb, fnTftpFinishCB finishcb, fnTftpErrorCB errorcb)
{
    sTftpContext* ctx = (sTftpContext*)malloc(sizeof(sTftpContext));
    if (!ctx)
        return NULL;

    memset(ctx, 0, sizeof(sTftpContext));
    ctx->Port = port;
    ctx->StartCB = startcb;
    ctx->ReadCB = readcb;
    ctx->FinishCB = finishcb;
    ctx->ErrorCB = errorcb;

    ctx->Thread = Thread_Create(TftpSendThread, ctx, 0x1000, 6, "tftp_send");
    if (!ctx->Thread)
    {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void TftpFinish(void* _ctx)
{
    sTftpContext* ctx = (sTftpContext*)_ctx;
    Thread_Delete(ctx->Thread);
    free(ctx);
}

void TftpAbort(void* _ctx)
{
    sTftpContext* ctx = (sTftpContext*)_ctx;
    ctx->Abort = 1;

    Thread_Wait(ctx->Thread, NoTimeout);
    Thread_Delete(ctx->Thread);
    free(ctx);
}

static void TftpSendThread(void* userdata)
{
    sTftpContext* ctx = (sTftpContext*)userdata;
    u8 rxdata[1024];
    u8 txdata[512];
    struct timeval tv;
    int res;

    ctx->Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx->Socket < 0)
    {
        if (ctx->ErrorCB) ctx->ErrorCB(0, "Failed to create socket");
        return;
    }

    struct sockaddr_in saddr = {
        .sin_family = AF_INET,
        .sin_port = htons(ctx->Port),
        .sin_addr = (struct in_addr){INADDR_ANY}
    };

    if (bind(ctx->Socket, (struct sockaddr*)&saddr, sizeof(saddr)) < 0)
    {
        TftpHandleError(ctx, 0, 0, "Failed to bind socket");
        return;
    }

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ctx->Socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
    {
        TftpHandleError(ctx, 0, 0, "Failed to set send timeout");
        return;
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(ctx->Socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        TftpHandleError(ctx, 0, 0, "Failed to set receive timeout");
        return;
    }

    rxdata[sizeof(rxdata) - 1] = 0;

    // wait for RRQ frame
    for (;;)
    {
        res = TftpRecvFrame(ctx, 0, rxdata, sizeof(rxdata) - 1);
        if (res > 0) break;
        if (!res)
        {
            TftpHandleError(ctx, 1, 0, "Communication failure");
            return;
        }
        if (ctx->Abort)
        {
            close(ctx->Socket);
            return;
        }
    }

    u16 op = ntohs(*(u16*)&rxdata[0]);

    if (op == 5)
    {
        u16 errcode = ntohs(*(u16*)&rxdata[2]);
        char* errmsg = (char*)(rxdata + 4);
        TftpHandleError(ctx, 0, errcode, errmsg);
        return;
    }

    char* filename = (char*)(rxdata + 2);
    char* mode = filename + strlen(filename) + 1;

    if (op != 1)
    {
        TftpHandleError(ctx, 1, 2, "Wrong operation (not a read command)");
        return;
    }

    if (strcmp(mode, "octet"))
    {
        TftpHandleError(ctx, 1, 0, "Client not using octet mode");
        return;
    }

    if (ctx->StartCB)
    {
        if (!ctx->StartCB(filename, mode))
        {
            TftpHandleError(ctx, 1, 1, "Could not open file");
            return;
        }
    }

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(ctx->Socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pos = 0;
    u16 blkid = 1;
    int blklen = 512;
    for (;;)
    {
        int thisblk = ctx->ReadCB(pos, txdata, blklen);
        if (thisblk < 0)
        {
            TftpSendError(ctx, 0, "failed to read data");
            if (ctx->ErrorCB) ctx->ErrorCB(0, "Failed to read data");
            close(ctx->Socket);
            return;
        }

        int good = 0;
        for (int retries = 0; retries < 5; retries++)
        {
            if (TftpSendData(ctx, blkid, txdata, thisblk) < 1)
            {
                TftpHandleError(ctx, 1, 0, "Communication failure");
                return;
            }

            // receive ack
            res = TftpRecvFrame(ctx, 1, rxdata, sizeof(rxdata) - 1);
            if (res < 0)
                continue;
            if (!res)
            {
                TftpHandleError(ctx, 1, 0, "Communication failure");
                return;
            }

            u16 ackop = ntohs(*(u16*)&rxdata[0]);

            if (ackop == 5)
            {
                u16 errcode = ntohs(*(u16*)&rxdata[2]);
                char* errmsg = (char*)(rxdata + 4);
                TftpHandleError(ctx, 0, errcode, errmsg);
                return;
            }
            if (ackop != 4)
            {
                TftpHandleError(ctx, 1, 4, "Received wrong frame type");
                return;
            }

            u16 ackblk = ntohs(*(u16*)&rxdata[2]);

            if (ackblk != blkid)
                continue;

            good = 1;
            break;
        }

        if (!good)
        {
            TftpHandleError(ctx, 1, 0, "Communication failure");
            return;
        }

        if (thisblk < blklen)
            break;

        blkid++;
        pos += blklen;
    }

    close(ctx->Socket);
    if (ctx->FinishCB) ctx->FinishCB();
}
