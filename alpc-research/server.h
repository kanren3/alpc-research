#ifndef _SERVER_H_
#define _SERVER_H_

EXTERN_C_START

typedef struct _PORT_CONTEXT {
    CLIENT_ID ClientId;
    HANDLE CommunicationPortHandle;
} PORT_CONTEXT, *PPORT_CONTEXT;

NTSTATUS NTAPI
SrvInitializeChannel (
    IN LPCWSTR PortName
);

EXTERN_C_END

#endif
