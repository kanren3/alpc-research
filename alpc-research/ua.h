#ifndef _UA_H_
#define _UA_H_

EXTERN_C_START

#define ALPC_PORFLG_LPC_MODE 0x1000
#define ALPC_PORFLG_ALLOW_IMPERSONATION 0x10000
#define ALPC_PORFLG_ALLOW_LPC_REQUESTS 0x20000
#define ALPC_PORFLG_WAITABLE_PORT 0x40000
#define ALPC_PORFLG_ALLOW_DUP_OBJECT 0x80000
#define ALPC_PORFLG_SYSTEM_PROCESS 0x100000
#define ALPC_PORFLG_WAKE_POLICY1 0x200000
#define ALPC_PORFLG_WAKE_POLICY2 0x400000
#define ALPC_PORFLG_WAKE_POLICY3 0x800000
#define ALPC_PORFLG_DIRECT_MESSAGE 0x1000000
#define ALPC_PORFLG_ALLOW_MULTIHANDLE_ATTRIBUTE 0x2000000

#define ALPC_PORFLG_OBJECT_TYPE_FILE 0x0001
#define ALPC_PORFLG_OBJECT_TYPE_INVALID 0x0002
#define ALPC_PORFLG_OBJECT_TYPE_THREAD 0x0004
#define ALPC_PORFLG_OBJECT_TYPE_SEMAPHORE 0x0008
#define ALPC_PORFLG_OBJECT_TYPE_EVENT 0x0010
#define ALPC_PORFLG_OBJECT_TYPE_PROCESS 0X0020
#define ALPC_PORFLG_OBJECT_TYPE_MUTEX 0x0040
#define ALPC_PORFLG_OBJECT_TYPE_SECTION 0x0080
#define ALPC_PORFLG_OBJECT_TYPE_REGKEY 0x0100
#define ALPC_PORFLG_OBJECT_TYPE_TOKEN 0x0200
#define ALPC_PORFLG_OBJECT_TYPE_COMPOSITION 0x0400
#define ALPC_PORFLG_OBJECT_TYPE_JOB 0x0800
#define ALPC_PORFLG_OBJECT_TYPE_ALL \
    (ALPC_PORFLG_OBJECT_TYPE_FILE | ALPC_PORFLG_OBJECT_TYPE_THREAD | \
     ALPC_PORFLG_OBJECT_TYPE_SEMAPHORE | ALPC_PORFLG_OBJECT_TYPE_EVENT | \
     ALPC_PORFLG_OBJECT_TYPE_PROCESS | ALPC_PORFLG_OBJECT_TYPE_MUTEX | \
     ALPC_PORFLG_OBJECT_TYPE_SECTION | ALPC_PORFLG_OBJECT_TYPE_REGKEY | \
     ALPC_PORFLG_OBJECT_TYPE_TOKEN | ALPC_PORFLG_OBJECT_TYPE_COMPOSITION | \
     ALPC_PORFLG_OBJECT_TYPE_JOB)

#define LPC_REQUEST 1
#define LPC_REPLY 2
#define LPC_DATAGRAM 3
#define LPC_LOST_REPLY 4
#define LPC_PORT_CLOSED 5
#define LPC_CLIENT_DIED 6
#define LPC_EXCEPTION 7
#define LPC_DEBUG_EVENT 8
#define LPC_ERROR_EVENT 9
#define LPC_CONNECTION_REQUEST 10
#define LPC_CONTINUATION_REQUIRED 0x2000

typedef char CCHAR;          // winnt
typedef short CSHORT;
typedef ULONG CLONG;

typedef CCHAR *PCCHAR;
typedef CSHORT *PCSHORT;
typedef CLONG *PCLONG;

typedef struct _PORT_MESSAGE {
    union {
        struct {
            CSHORT DataLength;
            CSHORT TotalLength;
        } s1;
        ULONG Length;
    } u1;
    union {
        struct {
            CSHORT Type;
            CSHORT DataInfoOffset;
        } s2;
        ULONG ZeroInit;
    } u2;
    union {
        CLIENT_ID ClientId;
        double DoNotUseThisField;
    };
    ULONG MessageId;
    union {
        SIZE_T ClientViewSize;
        ULONG CallbackId;
    };
} PORT_MESSAGE, *PPORT_MESSAGE;

typedef struct _ALPC_PORT_ATTRIBUTES {
    ULONG Flags;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    SIZE_T MaxMessageLength;
    SIZE_T MemoryBandwidth;
    SIZE_T MaxPoolUsage;
    SIZE_T MaxSectionSize;
    SIZE_T MaxViewSize;
    SIZE_T MaxTotalSectionSize;
    ULONG DupObjectTypes;
#ifdef _WIN64
    ULONG Reserved;
#endif
} ALPC_PORT_ATTRIBUTES, *PALPC_PORT_ATTRIBUTES;

typedef struct _ALPC_MESSAGE_ATTRIBUTES {
    ULONG AllocatedAttributes;
    ULONG ValidAttributes;
} ALPC_MESSAGE_ATTRIBUTES, *PALPC_MESSAGE_ATTRIBUTES;

#define ALPC_MESSAGE_SECURITY_ATTRIBUTE 0x80000000
#define ALPC_MESSAGE_VIEW_ATTRIBUTE 0x40000000
#define ALPC_MESSAGE_CONTEXT_ATTRIBUTE 0x20000000
#define ALPC_MESSAGE_HANDLE_ATTRIBUTE 0x10000000

typedef struct _ALPC_CONTEXT_ATTR {
    PVOID PortContext;
    PVOID MessageContext;
    ULONG Sequence;
    ULONG MessageId;
    ULONG CallbackId;
} ALPC_CONTEXT_ATTR, *PALPC_CONTEXT_ATTR;

#define ALPC_MSGFLG_REPLY_MESSAGE 0x1
#define ALPC_MSGFLG_LPC_MODE 0x2
#define ALPC_MSGFLG_RELEASE_MESSAGE 0x10000
#define ALPC_MSGFLG_SYNC_REQUEST 0x20000
#define ALPC_MSGFLG_TRACK_PORT_REFERENCES 0x40000
#define ALPC_MSGFLG_WAIT_USER_MODE 0x100000
#define ALPC_MSGFLG_WAIT_ALERTABLE 0x200000
#define ALPC_MSGFLG_WOW64_CALL 0x80000000

NTSYSAPI
PVOID
NTAPI
AlpcGetMessageAttribute(
    _In_ PALPC_MESSAGE_ATTRIBUTES Buffer,
    _In_ ULONG AttributeFlag
);

NTSYSAPI
ULONG
NTAPI
AlpcGetHeaderSize(
    _In_ ULONG Flags
);

NTSYSAPI
NTSTATUS
NTAPI
AlpcInitializeMessageAttribute(
    _In_ ULONG AttributeFlags,
    _Out_opt_ PALPC_MESSAGE_ATTRIBUTES Buffer,
    _In_ SIZE_T BufferSize,
    _Out_ PSIZE_T RequiredBufferSize
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcSendWaitReceivePort(
    _In_ HANDLE PortHandle,
    _In_ ULONG Flags,
    _In_reads_bytes_opt_(SendMessage->u1.s1.TotalLength) PPORT_MESSAGE SendMessage,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES SendMessageAttributes,
    _Out_writes_bytes_to_opt_(*BufferLength, *BufferLength) PPORT_MESSAGE ReceiveMessage,
    _Inout_opt_ PSIZE_T BufferLength,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES ReceiveMessageAttributes,
    _In_opt_ PLARGE_INTEGER Timeout
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcSendWaitReceivePort(
    _In_ HANDLE PortHandle,
    _In_ ULONG Flags,
    _In_reads_bytes_opt_(SendMessage->u1.s1.TotalLength) PPORT_MESSAGE SendMessage,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES SendMessageAttributes,
    _Out_writes_bytes_to_opt_(*BufferLength, *BufferLength) PPORT_MESSAGE ReceiveMessage,
    _Inout_opt_ PSIZE_T BufferLength,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES ReceiveMessageAttributes,
    _In_opt_ PLARGE_INTEGER Timeout
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCancelMessage(
    _In_ HANDLE PortHandle,
    _In_ ULONG Flags,
    _In_ PALPC_CONTEXT_ATTR MessageContext
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCancelMessage(
    _In_ HANDLE PortHandle,
    _In_ ULONG Flags,
    _In_ PALPC_CONTEXT_ATTR MessageContext
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcAcceptConnectPort(
    _Out_ PHANDLE PortHandle,
    _In_ HANDLE ConnectionPortHandle,
    _In_ ULONG Flags,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PALPC_PORT_ATTRIBUTES PortAttributes,
    _In_opt_ PVOID PortContext,
    _In_reads_bytes_(ConnectionRequest->u1.s1.TotalLength) PPORT_MESSAGE ConnectionRequest,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES ConnectionMessageAttributes,
    _In_ BOOLEAN AcceptConnection
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcAcceptConnectPort(
    _Out_ PHANDLE PortHandle,
    _In_ HANDLE ConnectionPortHandle,
    _In_ ULONG Flags,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PALPC_PORT_ATTRIBUTES PortAttributes,
    _In_opt_ PVOID PortContext,
    _In_reads_bytes_(ConnectionRequest->u1.s1.TotalLength) PPORT_MESSAGE ConnectionRequest,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES ConnectionMessageAttributes,
    _In_ BOOLEAN AcceptConnection
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreatePort(
    _Out_ PHANDLE PortHandle,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PALPC_PORT_ATTRIBUTES PortAttributes
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreatePort(
    _Out_ PHANDLE PortHandle,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PALPC_PORT_ATTRIBUTES PortAttributes
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcConnectPort(
    _Out_ PHANDLE PortHandle,
    _In_ PUNICODE_STRING PortName,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PALPC_PORT_ATTRIBUTES PortAttributes,
    _In_ ULONG Flags,
    _In_opt_ PSID RequiredServerSid,
    _Inout_updates_bytes_to_opt_(*BufferLength, *BufferLength) PPORT_MESSAGE ConnectionMessage,
    _Inout_opt_ PSIZE_T BufferLength,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES OutMessageAttributes,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES InMessageAttributes,
    _In_opt_ PLARGE_INTEGER Timeout
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcConnectPort(
    _Out_ PHANDLE PortHandle,
    _In_ PUNICODE_STRING PortName,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PALPC_PORT_ATTRIBUTES PortAttributes,
    _In_ ULONG Flags,
    _In_opt_ PSID RequiredServerSid,
    _Inout_updates_bytes_to_opt_(*BufferLength, *BufferLength) PPORT_MESSAGE ConnectionMessage,
    _Inout_opt_ PSIZE_T BufferLength,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES OutMessageAttributes,
    _Inout_opt_ PALPC_MESSAGE_ATTRIBUTES InMessageAttributes,
    _In_opt_ PLARGE_INTEGER Timeout
);

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

typedef struct _UA_SERVER {
    UNICODE_STRING PortName;
    REQUEST_PROCEDURE OnRequest;
    DATAGRAM_PROCEDURE OnDatagram;
    CONNECTION_PROCEDURE OnConnect;
    PPORT_MESSAGE PortMessage;
    ALPC_PORT_ATTRIBUTES PortAttributes;
    PALPC_MESSAGE_ATTRIBUTES MessageAttributes;
    HANDLE ConnectionPortHandle;
    HANDLE ServerThreadHandle;
} UA_SERVER, *PUA_SERVER;

VOID
NTAPI
UaTerminateServer (
    IN PUA_SERVER Server
);

PUA_SERVER
NTAPI
UaCreateServer (
    IN LPCWSTR PortName,
    IN REQUEST_PROCEDURE OnRequest,
    IN DATAGRAM_PROCEDURE OnDatagram,
    IN CONNECTION_PROCEDURE OnConnect
);

NTSTATUS
NTAPI
UaSendSynchronousRequest (
    IN HANDLE CommunicationPortHandle,
    IN OUT PVOID RequestDataBuffer,
    IN USHORT RequestDataLength
);

NTSTATUS
NTAPI
UaSendDatagram (
    IN HANDLE CommunicationPortHandle,
    IN PVOID DatagramBuffer,
    IN USHORT DatagramLength
);

VOID
NTAPI
UaDisconnectServer (
    IN HANDLE CommunicationPortHandle
);

NTSTATUS
NTAPI
UaConnectServer (
    OUT PHANDLE CommunicationPortHandle,
    IN LPCWSTR ServerPortName,
    IN PVOID ConnectionDataBuffer OPTIONAL,
    IN USHORT ConnectionDataLength
);

EXTERN_C_END

#endif
