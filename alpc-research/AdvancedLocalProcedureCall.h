#ifndef _ADVANCED_LOCAL_PROCEDURE_CALL_H_
#define _ADVANCED_LOCAL_PROCEDURE_CALL_H_

EXTERN_C_START

#ifndef _WIN64
#define ALPC_MAX_ALLOWED_MESSAGE_LENGTH 0xFFEF
#else
#define ALPC_MAX_ALLOWED_MESSAGE_LENGTH 0xFFFF
#endif

typedef struct _PORT_CONTEXT {
    CLIENT_ID ClientId;
    HANDLE CommunicationPortHandle;
} PORT_CONTEXT, *PPORT_CONTEXT;

typedef
VOID
(NTAPI *REQUEST_PROCEDURE)(
    IN OUT PPORT_MESSAGE PortMessage
    );

typedef
VOID
(NTAPI *DATAGRAM_PROCEDURE)(
    IN PPORT_MESSAGE PortMessage
    );

typedef
BOOLEAN
(NTAPI *CONNECTION_PROCEDURE)(
    IN PPORT_MESSAGE PortMessage
    );

typedef struct _SERVER_OBJECT {
    UNICODE_STRING PortName;
    REQUEST_PROCEDURE OnRequest;
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
    IN REQUEST_PROCEDURE OnRequest,
    IN DATAGRAM_PROCEDURE OnDatagram,
    IN CONNECTION_PROCEDURE OnConnect
);

BOOLEAN
NTAPI
SrvSendSynchronousRequest (
    IN PCLIENT_OBJECT ClientObject,
    IN OUT PVOID RequestDataBuffer,
    IN USHORT RequestDataLength
);

BOOLEAN
NTAPI
SrvSendDatagram (
    IN PCLIENT_OBJECT ClientObject,
    IN PVOID DatagramBuffer,
    IN USHORT DatagramLength
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
    IN PVOID ConnectionDataBuffer OPTIONAL,
    IN USHORT ConnectionDataLength
);

EXTERN_C_END

#endif