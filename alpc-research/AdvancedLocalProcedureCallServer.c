#include <Native.h>
#include "Runtime.h"
#include "AdvancedLocalProcedureCallServer.h"

VOID
NTAPI
SrvInitializePortAttributes (
    OUT PALPC_PORT_ATTRIBUTES PortAttributes
)
{
    RtlZeroMemory(PortAttributes, sizeof(ALPC_PORT_ATTRIBUTES));

    PortAttributes->MaxMessageLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;
}

BOOLEAN
NTAPI
SrvInitializePortMessage (
    OUT PPORT_MESSAGE PortMessage,
    IN PVOID DataBuffer OPTIONAL,
    IN SHORT DataLength
)
{
    SHORT TotalLength;
    PVOID BodyBuffer;

    TotalLength = sizeof(PORT_MESSAGE) + DataLength;

    if (TotalLength > ALPC_MAX_ALLOWED_MESSAGE_LENGTH) {
        return FALSE;
    }

    RtlZeroMemory(PortMessage, TotalLength);

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = DataLength;

    if (NULL != DataBuffer) {
        BodyBuffer = PortMessage + 1;
        RtlCopyMemory(BodyBuffer, DataBuffer, DataLength);
    }

    return TRUE;
}

NTSTATUS
NTAPI
SrvAlpcRequest (
    IN PSERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;

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
    PVOID DatagramBuffer;
    SHORT DatagramLength;

    ContextAttribute = AlpcGetMessageAttribute(ServerObject->MessageAttributes,
                                               ALPC_MESSAGE_CONTEXT_ATTRIBUTE);

    if (NULL == ContextAttribute) {
        return STATUS_UNSUCCESSFUL;
    }

    DatagramBuffer = ServerObject->PortMessage + 1;
    DatagramLength = ServerObject->PortMessage->u1.s1.DataLength;

    ServerObject->OnDatagram(DatagramBuffer, DatagramLength);

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
    PVOID ConnectionDataBuffer;
    SHORT ConnectionDataLength;

    InitializeObjectAttributes(&ObjectAttributes,
                               NULL,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    ConnectionDataBuffer = ServerObject->PortMessage + 1;
    ConnectionDataLength = ServerObject->PortMessage->u1.s1.DataLength;

    if (FALSE != ServerObject->OnConnect(ConnectionDataBuffer, ConnectionDataLength)) {
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
SrvSendDatagram (
    IN PCLIENT_OBJECT ClientObject,
    IN PVOID DatagramBuffer,
    IN SHORT DatagramLength
)
{
    NTSTATUS Status;
    BOOLEAN Result;

    Result = SrvInitializePortMessage(ClientObject->PortMessage,
                                      DatagramBuffer,
                                      DatagramLength);

    if (FALSE == Result) {
        return FALSE;
    }

    Status = NtAlpcSendWaitReceivePort(ClientObject->CommunicationPortHandle,
                                       ALPC_MSGFLG_RELEASE_MESSAGE,
                                       ClientObject->PortMessage,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

    return NT_SUCCESS(Status);
}

VOID
NTAPI
SrvDisconnectServer (
    IN PCLIENT_OBJECT ClientObject
)
{
    NtClose(ClientObject->CommunicationPortHandle);
    RtFreeHeap(ClientObject->PortMessage);
    RtFreeHeap(ClientObject);
}

PCLIENT_OBJECT
NTAPI
SrvConnectServer (
    IN LPCWSTR PortName,
    IN PVOID ConnectionDataBuffer OPTIONAL,
    IN SHORT ConnectionDataLength
)
{
    NTSTATUS Status;
    PCLIENT_OBJECT ClientObject;
    SIZE_T BufferLength;
    BOOLEAN Result;

    ClientObject = RtAllocateZeroHeap(sizeof(CLIENT_OBJECT));

    if (NULL == ClientObject) {
        goto Cleanup;
    }

    ClientObject->PortMessage = RtAllocateZeroHeap(ALPC_MAX_ALLOWED_MESSAGE_LENGTH);

    if (NULL == ClientObject->PortMessage) {
        goto Cleanup;
    }

    Result = SrvInitializePortMessage(ClientObject->PortMessage,
                                      ConnectionDataBuffer,
                                      ConnectionDataLength);

    if (FALSE == Result) {
        goto Cleanup;
    }

    SrvInitializePortAttributes(&ClientObject->PortAttributes);

    RtlInitUnicodeString(&ClientObject->PortName, PortName);

    BufferLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;

    Status = NtAlpcConnectPort(&ClientObject->CommunicationPortHandle,
                               &ClientObject->PortName,
                               NULL,
                               &ClientObject->PortAttributes,
                               ALPC_MSGFLG_SYNC_REQUEST,
                               NULL,
                               ClientObject->PortMessage,
                               &BufferLength,
                               NULL,
                               NULL,
                               NULL);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    return ClientObject;

Cleanup:

    if (NULL != ClientObject) {
        if (NULL != ClientObject->PortMessage) {
            RtFreeHeap(ClientObject->PortMessage);
        }

        RtFreeHeap(ClientObject);
    }

    return NULL;
}