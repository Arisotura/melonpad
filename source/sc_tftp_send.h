#ifndef _SC_TFTP_SEND_H_
#define _SC_TFTP_SEND_H_

extern sScreen scTftpSend;

void ScTftpSend_Open(void* data);
void ScTftpSend_Close();
void ScTftpSend_Activate();
void ScTftpSend_Update();

#endif // _SC_TFTP_SEND_H_
