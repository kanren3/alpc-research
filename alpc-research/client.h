#ifndef _CLIENT_H_
#define _CLIENT_H_

EXTERN_C_START

#define ALPC_MAX_MESSAGE_LENGTH   MAXUINT16

NTSTATUS NTAPI
CliTestDatagram (
    IN LPCWSTR PortName
);

NTSTATUS NTAPI
CliTestSyncRequest (
    IN LPCWSTR PortName
);

EXTERN_C_END

#endif
