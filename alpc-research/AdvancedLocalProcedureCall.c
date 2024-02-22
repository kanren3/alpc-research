#include <Native.h>
#include "Runtime.h"
#include "AdvancedLocalProcedureCall.h"

VOID
NTAPI
SrvInitializePortAttributes (
    OUT PALPC_PORT_ATTRIBUTES PortAttributes
)
{
    RtlZeroMemory(PortAttributes, sizeof(ALPC_PORT_ATTRIBUTES));

    PortAttributes->MaxMessageLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;
}

NTSTATUS
NTAPI
SrvAlpcRequest (
    IN PSERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;

    ServerObject->OnRequest(ServerObject->PortMessage);

    Status = NtAlpcSendWaitReceivePort(ServerObject->ConnectionPortHandle,
                                       ALPC_MSGFLG_RELEASE_MESSAGE,
                                       ServerObject->PortMessage,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

    return Status;
}

NTSTATUS
NTAPI
SrvAlpcDatagram (
    IN PSERVER_OBJECT ServerObject
)
{
    PALPC_CONTEXT_ATTR ContextAttribute;

    ContextAttribute = AlpcGetMessageAttribute(ServerObject->MessageAttributes,
                                               ALPC_MESSAGE_CONTEXT_ATTRIBUTE);

    if (NULL == ContextAttribute) {
        return STATUS_UNSUCCESSFUL;
    }

    ServerObject->OnDatagram(ServerObject->PortMessage);

    if (0 != (ServerObject->PortMessage->u2.s2.Type & LPC_CONTINUATION_REQUIRED)) {
        NtAlpcCancelMessage(ServerObject->ConnectionPortHandle, 0, ContextAttribute);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
SrvAlpcDisconnect (
    IN PSERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;
    PALPC_CONTEXT_ATTR ContextAttribute;
    PPORT_CONTEXT PortContext;

    ContextAttribute = AlpcGetMessageAttribute(ServerObject->MessageAttributes,
                                               ALPC_MESSAGE_CONTEXT_ATTRIBUTE);

    if (NULL == ContextAttribute) {
        return STATUS_UNSUCCESSFUL;
    }

    PortContext = ContextAttribute->PortContext;

    Status = NtClose(PortContext->CommunicationPortHandle);

    RtFreeHeap(PortContext);

    return Status;
}

NTSTATUS
NTAPI
SrvAlpcConnect (
    IN PSERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;
    PPORT_CONTEXT PortContext;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE CommunicationPortHandle;

    InitializeObjectAttributes(&ObjectAttributes,
                               NULL,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    if (FALSE != ServerObject->OnConnect(ServerObject->PortMessage)) {
        PortContext = RtAllocateZeroHeap(sizeof(PORT_CONTEXT));

        if (NULL != PortContext) {
            PortContext->ClientId = ServerObject->PortMessage->ClientId;

            Status = NtAlpcAcceptConnectPort(&PortContext->CommunicationPortHandle,
                                             ServerObject->ConnectionPortHandle,
                                             0,
                                             &ObjectAttributes,
                                             &ServerObject->PortAttributes,
                                             PortContext,
                                             ServerObject->PortMessage,
                                             NULL,
                                             TRUE);

            if (FALSE == NT_SUCCESS (Status)) {
                Status = NtAlpcAcceptConnectPort(&CommunicationPortHandle,
                                                 ServerObject->ConnectionPortHandle,
                                                 0,
                                                 &ObjectAttributes,
                                                 &ServerObject->PortAttributes,
                                                 NULL,
                                                 ServerObject->PortMessage,
                                                 NULL,
                                                 FALSE);

                RtFreeHeap(PortContext);
            }
        }
        else {
            Status = NtAlpcAcceptConnectPort(&CommunicationPortHandle,
                                             ServerObject->ConnectionPortHandle,
                                             0,
                                             &ObjectAttributes,
                                             &ServerObject->PortAttributes,
                                             NULL,
                                             ServerObject->PortMessage,
                                             NULL,
                                             FALSE);
        }
    }
    else {
        Status = NtAlpcAcceptConnectPort(&CommunicationPortHandle,
                                         ServerObject->ConnectionPortHandle,
                                         0,
                                         &ObjectAttributes,
                                         &ServerObject->PortAttributes,
                                         NULL,
                                         ServerObject->PortMessage,
                                         NULL,
                                         FALSE);
    }

    return Status;
}

NTSTATUS
NTAPI
SrvProcessMessage (
    IN PSERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;

    switch (LOBYTE(ServerObject->PortMessage->u2.s2.Type)) {
    case LPC_REQUEST:
        Status = SrvAlpcRequest(ServerObject);
        break;
    case LPC_DATAGRAM:
        Status = SrvAlpcDatagram(ServerObject);
        break;
    case LPC_PORT_CLOSED:
        Status = SrvAlpcDisconnect(ServerObject);
        break;
    case LPC_CLIENT_DIED:
        Status = SrvAlpcDisconnect(ServerObject);
        break;
    case LPC_CONNECTION_REQUEST:
        Status = SrvAlpcConnect(ServerObject);
        break;
    default:
        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

NTSTATUS
NTAPI
SrvServerWorker (
    IN PSERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;
    SIZE_T BufferLength;

    while (TRUE) {
        BufferLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;

        Status = NtAlpcSendWaitReceivePort(ServerObject->ConnectionPortHandle,
                                           0,
                                           NULL,
                                           NULL,
                                           ServerObject->PortMessage,
                                           &BufferLength,
                                           ServerObject->MessageAttributes,
                                           NULL);

        if (FALSE == NT_SUCCESS(Status)) {
            break;
        }

        Status = SrvProcessMessage(ServerObject);

        if (FALSE == NT_SUCCESS(Status)) {
            break;
        }
    }

    return Status;
}

VOID
NTAPI
SrvTerminateServer (
    IN PSERVER_OBJECT ServerObject
)
{
    NtClose(ServerObject->ConnectionPortHandle);
    NtWaitForSingleObject(ServerObject->ServerThreadHandle, FALSE, NULL);
    RtFreeHeap(ServerObject->PortMessage);
    RtFreeHeap(ServerObject->MessageAttributes);
    RtFreeHeap(ServerObject);
}

PSERVER_OBJECT
NTAPI
SrvCreateServer (
    IN LPCWSTR PortName,
    IN REQUEST_PROCEDURE OnRequest,
    IN DATAGRAM_PROCEDURE OnDatagram,
    IN CONNECTION_PROCEDURE OnConnect
)
{
    NTSTATUS Status;
    PSERVER_OBJECT ServerObject;
    ULONG MessageAttributesMask;
    SIZE_T MessageAttributesSize;
    SIZE_T RequiredBufferSize;
    OBJECT_ATTRIBUTES ObjectAttributes;

    ServerObject = RtAllocateZeroHeap(sizeof(SERVER_OBJECT));

    if (NULL == ServerObject) {
        goto Cleanup;
    }

    ServerObject->OnRequest = OnRequest;
    ServerObject->OnDatagram = OnDatagram;
    ServerObject->OnConnect = OnConnect;

    MessageAttributesMask = ALPC_MESSAGE_VIEW_ATTRIBUTE | \
        ALPC_MESSAGE_CONTEXT_ATTRIBUTE;

    MessageAttributesSize = AlpcGetHeaderSize(MessageAttributesMask);

    ServerObject->MessageAttributes = RtAllocateZeroHeap(MessageAttributesSize);

    if (NULL == ServerObject->MessageAttributes) {
        goto Cleanup;
    }

    Status = AlpcInitializeMessageAttribute(MessageAttributesMask,
                                            ServerObject->MessageAttributes,
                                            MessageAttributesSize,
                                            &RequiredBufferSize);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    ServerObject->PortMessage = RtAllocateZeroHeap(ALPC_MAX_ALLOWED_MESSAGE_LENGTH);

    if (NULL == ServerObject->PortMessage) {
        goto Cleanup;
    }

    SrvInitializePortAttributes(&ServerObject->PortAttributes);

    RtlInitUnicodeString(&ServerObject->PortName, PortName);

    InitializeObjectAttributes(&ObjectAttributes,
                               &ServerObject->PortName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = NtAlpcCreatePort(&ServerObject->ConnectionPortHandle,
                              &ObjectAttributes,
                              &ServerObject->PortAttributes);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    Status = RtCreateThread(&ServerObject->ServerThreadHandle,
                            SrvServerWorker,
                            ServerObject);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    return ServerObject;

Cleanup:

    if (NULL != ServerObject) {
        if (NULL != ServerObject->ConnectionPortHandle) {
            NtClose(ServerObject->ConnectionPortHandle);
        }

        if (NULL != ServerObject->MessageAttributes) {
            RtFreeHeap(ServerObject->MessageAttributes);
        }

        if (NULL != ServerObject->PortMessage) {
            RtFreeHeap(ServerObject->PortMessage);
        }

        RtFreeHeap(ServerObject);
    }

    return NULL;
}

BOOLEAN
NTAPI
SrvSendSynchronousRequest (
    IN PCLIENT_OBJECT ClientObject,
    IN OUT PVOID RequestDataBuffer,
    IN USHORT RequestDataLength
)
{
    NTSTATUS Status;
    USHORT TotalLength;
    PPORT_MESSAGE PortMessage;
    PVOID DataBuffer;
    SIZE_T BufferLength;

    TotalLength = sizeof(PORT_MESSAGE) + RequestDataLength;

    if (TotalLength > ALPC_MAX_ALLOWED_MESSAGE_LENGTH) {
        return FALSE;
    }

    PortMessage = RtAllocateZeroHeap(TotalLength);

    if (NULL == PortMessage) {
        return FALSE;
    }

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = RequestDataLength;

    DataBuffer = PortMessage + 1;

    RtlCopyMemory(DataBuffer, RequestDataBuffer, RequestDataLength);

    BufferLength = TotalLength;

    Status = NtAlpcSendWaitReceivePort(ClientObject->CommunicationPortHandle,
                                       ALPC_MSGFLG_SYNC_REQUEST,
                                       PortMessage,
                                       NULL,
                                       PortMessage,
                                       &BufferLength,
                                       NULL,
                                       NULL);

    RtlCopyMemory(RequestDataBuffer, DataBuffer, RequestDataLength);

    RtFreeHeap(PortMessage);

    return NT_SUCCESS(Status);
}

BOOLEAN
NTAPI
SrvSendDatagram (
    IN PCLIENT_OBJECT ClientObject,
    IN PVOID DatagramBuffer,
    IN USHORT DatagramLength
)
{
    NTSTATUS Status;
    USHORT TotalLength;
    PPORT_MESSAGE PortMessage;
    PVOID DataBuffer;

    TotalLength = sizeof(PORT_MESSAGE) + DatagramLength;

    if (TotalLength > ALPC_MAX_ALLOWED_MESSAGE_LENGTH) {
        return FALSE;
    }

    PortMessage = RtAllocateZeroHeap(TotalLength);

    if (NULL == PortMessage) {
        return FALSE;
    }

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = DatagramLength;

    DataBuffer = PortMessage + 1;

    RtlCopyMemory(DataBuffer, DatagramBuffer, DatagramLength);

    Status = NtAlpcSendWaitReceivePort(ClientObject->CommunicationPortHandle,
                                       ALPC_MSGFLG_RELEASE_MESSAGE,
                                       PortMessage,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

    RtFreeHeap(PortMessage);

    return NT_SUCCESS(Status);
}

VOID
NTAPI
SrvDisconnectServer (
    IN PCLIENT_OBJECT ClientObject
)
{
    NtClose(ClientObject->CommunicationPortHandle);
    RtFreeHeap(ClientObject);
}

PCLIENT_OBJECT
NTAPI
SrvConnectServer (
    IN LPCWSTR PortName,
    IN PVOID ConnectionDataBuffer OPTIONAL,
    IN USHORT ConnectionDataLength
)
{
    NTSTATUS Status;
    PCLIENT_OBJECT ClientObject;
    SIZE_T BufferLength;
    USHORT TotalLength;
    PPORT_MESSAGE PortMessage;
    PVOID DataBuffer;

    ClientObject = RtAllocateZeroHeap(sizeof(CLIENT_OBJECT));

    if (NULL == ClientObject) {
        goto Cleanup;
    }

    TotalLength = sizeof(PORT_MESSAGE) + ConnectionDataLength;

    if (TotalLength > ALPC_MAX_ALLOWED_MESSAGE_LENGTH) {
        goto Cleanup;
    }

    PortMessage = RtAllocateZeroHeap(TotalLength);

    if (NULL == PortMessage) {
        goto Cleanup;
    }

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = ConnectionDataLength;

    DataBuffer = PortMessage + 1;

    RtlCopyMemory(DataBuffer, ConnectionDataBuffer, ConnectionDataLength);

    SrvInitializePortAttributes(&ClientObject->PortAttributes);

    RtlInitUnicodeString(&ClientObject->PortName, PortName);

    BufferLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;

    Status = NtAlpcConnectPort(&ClientObject->CommunicationPortHandle,
                               &ClientObject->PortName,
                               NULL,
                               &ClientObject->PortAttributes,
                               ALPC_MSGFLG_SYNC_REQUEST,
                               NULL,
                               PortMessage,
                               &BufferLength,
                               NULL,
                               NULL,
                               NULL);

    RtFreeHeap(PortMessage);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    return ClientObject;

Cleanup:

    if (NULL != ClientObject) {
        RtFreeHeap(ClientObject);
    }

    return NULL;
}