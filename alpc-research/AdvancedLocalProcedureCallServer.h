#ifndef _ADVANCED_LOCAL_PROCEDURE_CALL_SERVER_H_
#define _ADVANCED_LOCAL_PROCEDURE_CALL_SERVER_H_

EXTERN_C_START

#define ALPC_MAX_ALLOWED_MESSAGE_LENGTH 0xFFEF

typedef struct _PORT_CONTEXT {
    CLIENT_ID ClientId;
    HANDLE CommunicationPortHandle;
} PORT_CONTEXT, *PPORT_CONTEXT;

typedef
VOID
(NTAPI *DATAGRAM_PROCEDURE)(
    IN PVOID DatagramBuffer,
    IN SHORT DatagramLength
    );

typedef
BOOLEAN
(NTAPI *CONNECTION_PROCEDURE)(
    IN PVOID ConnectionDataBuffer,
    IN SHORT ConnectionDataLength
    );

typedef struct _SERVER_OBJECT {
    UNICODE_STRING PortName;
    DATAGRAM_PROCEDURE OnDatagram;
    CONNECTION_PROCEDURE OnConnect;
    PPORT_MESSAGE PortMessage;
    ALPC_PORT_ATTRIBUTES PortAttributes;
    PALPC_MESSAGE_ATTRIBUTES MessageAttributes;
    HANDLE ConnectionPortHandle;
    HANDLE ServerThreadHandle;
} SERVER_OBJECT, *PSERVER_OBJECT;

typedef struct _CLIENT_OBJECT {
    UNICODE_STRING PortName;
    PPORT_MESSAGE PortMessage;
    ALPC_PORT_ATTRIBUTES PortAttributes;
    HANDLE CommunicationPortHandle;
} CLIENT_OBJECT, *PCLIENT_OBJECT;

VOID
NTAPI
SrvTerminateServer (
    IN PSERVER_OBJECT ServerObject
);

PSERVER_OBJECT
NTAPI
SrvCreateServer (
    IN LPCWSTR PortName,
    IN DATAGRAM_PROCEDURE OnDatagram,
    IN CONNECTION_PROCEDURE OnConnect
);

BOOLEAN
NTAPI
SrvSendDatagram (
    IN PCLIENT_OBJECT ClientObject,
    IN PVOID DatagramBuffer,
    IN SHORT DatagramLength
);

VOID
NTAPI
SrvDisconnectServer (
    IN PCLIENT_OBJECT ClientObject
);

PCLIENT_OBJECT
NTAPI
SrvConnectServer (
    IN LPCWSTR PortName,
    IN PVOID DataBuffer OPTIONAL,
    IN SHORT DataLength
);

EXTERN_C_END

#endif