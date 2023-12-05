#include <stdio.h>
#include <ntum.h>
#include "runtime.h"
#include "server.h"

HANDLE SrvServerThreadHandle;

VOID NTAPI
SrvInitializePortAttributes (
    OUT PALPC_PORT_ATTRIBUTES PortAttributes
)
{
    RtlZeroMemory(PortAttributes, sizeof(ALPC_PORT_ATTRIBUTES));

    PortAttributes->MaxMessageLength = ALPC_MAX_MESSAGE_LENGTH;
}

VOID NTAPI
SrvInitializePortMessage (
    OUT PPORT_MESSAGE PortMessage,
    IN PVOID DataBuffer OPTIONAL,
    IN SHORT DataLength
)
{
    CSHORT TotalLength;
    PVOID MessageBody;

    MessageBody = PortMessage + 1;

    TotalLength = sizeof(PORT_MESSAGE) + DataLength;

    RtlZeroMemory(PortMessage, TotalLength);

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = DataLength;

    if (NULL != DataBuffer) {
        RtlCopyMemory(MessageBody, DataBuffer, DataLength);
    }
}

NTSTATUS NTAPI
OnAlpcRequest (
    IN HANDLE ConnectionPortHandle,
    IN PPORT_MESSAGE PortMessage,
    IN PALPC_MESSAGE_ATTRIBUTES MessageAttributes
)
{
    NTSTATUS Status;
    PVOID ContentBuffer;
    CSHORT ContentLength;
    PLARGE_INTEGER Counter;

    ContentBuffer = PortMessage + 1;
    ContentLength = PortMessage->u1.s1.DataLength;

    Counter = ContentBuffer;

    if (0 == (Counter->QuadPart % 1000000)) {
        printf("Request Receive Counter %lld.\n", Counter->QuadPart);
    }

    Status = NtAlpcSendWaitReceivePort(ConnectionPortHandle,
                                       ALPC_MSGFLG_RELEASE_MESSAGE,
                                       PortMessage,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

    return Status;
}

NTSTATUS NTAPI
OnAlpcDatagram (
    IN HANDLE ConnectionPortHandle,
    IN PPORT_MESSAGE PortMessage,
    IN PALPC_MESSAGE_ATTRIBUTES MessageAttributes
)
{
    PVOID ContentBuffer;
    CSHORT ContentLength;
    PLARGE_INTEGER Counter;

    ContentBuffer = PortMessage + 1;
    ContentLength = PortMessage->u1.s1.DataLength;

    Counter = ContentBuffer;

    if (0 == (Counter->QuadPart % 1000000)) {
        printf("Datagram Receive Counter %lld.\n", Counter->QuadPart);
    }

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
OnAlpcDisconnection (
    IN HANDLE ConnectionPortHandle,
    IN PPORT_MESSAGE PortMessage,
    IN PALPC_MESSAGE_ATTRIBUTES MessageAttributes
)
{
    NTSTATUS Status;
    PALPC_CONTEXT_ATTR ContextAttribute;
    PPORT_CONTEXT PortContext;

    ContextAttribute = AlpcGetMessageAttribute(MessageAttributes,
                                               ALPC_MESSAGE_CONTEXT_ATTRIBUTE);

    if (NULL == ContextAttribute) {
        return STATUS_UNSUCCESSFUL;
    }

    PortContext = ContextAttribute->PortContext;

    Status = NtClose(PortContext->CommunicationPortHandle);

    RtFreeHeap(PortContext);

    return Status;
}

NTSTATUS NTAPI
OnAlpcConnection (
    IN HANDLE ConnectionPortHandle,
    IN PPORT_MESSAGE PortMessage,
    IN PALPC_MESSAGE_ATTRIBUTES MessageAttributes
)
{
    NTSTATUS Status;
    PPORT_CONTEXT PortContext;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ALPC_PORT_ATTRIBUTES PortAttributes;

    PortContext = RtAllocateZeroHeap(sizeof(PORT_CONTEXT));

    if (NULL == PortContext) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    InitializeObjectAttributes(&ObjectAttributes,
                               NULL,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    SrvInitializePortAttributes(&PortAttributes);

    PortContext->ClientId = PortMessage->ClientId;

    Status = NtAlpcAcceptConnectPort(&PortContext->CommunicationPortHandle,
                                     ConnectionPortHandle,
                                     0,
                                     &ObjectAttributes,
                                     &PortAttributes,
                                     PortContext,
                                     PortMessage,
                                     NULL,
                                     TRUE);

    if (FALSE == NT_SUCCESS (Status)) {
        Status = NtAlpcAcceptConnectPort(&PortContext->CommunicationPortHandle,
                                         ConnectionPortHandle,
                                         0,
                                         &ObjectAttributes,
                                         &PortAttributes,
                                         PortContext,
                                         PortMessage,
                                         NULL,
                                         FALSE);
    }

    return Status;
}

NTSTATUS NTAPI
SrvProcessMessage (
    IN HANDLE ConnectionPortHandle,
    IN PPORT_MESSAGE PortMessage,
    IN PALPC_MESSAGE_ATTRIBUTES MessageAttributes
)
{
    NTSTATUS Status;

    switch (LOBYTE(PortMessage->u2.s2.Type)) {
    case LPC_REQUEST:
        Status = OnAlpcRequest(ConnectionPortHandle, PortMessage, MessageAttributes);
        break;
    case LPC_DATAGRAM:
        Status = OnAlpcDatagram(ConnectionPortHandle, PortMessage, MessageAttributes);
        break;
    case LPC_PORT_CLOSED:
        Status = OnAlpcDisconnection(ConnectionPortHandle, PortMessage, MessageAttributes);
        break;
    case LPC_CLIENT_DIED:
        Status = OnAlpcDisconnection(ConnectionPortHandle, PortMessage, MessageAttributes);
        break;
    case LPC_CONNECTION_REQUEST:
        Status = OnAlpcConnection(ConnectionPortHandle, PortMessage, MessageAttributes);
        break;
    default:
        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

NTSTATUS NTAPI
SrvServerWorker (
    IN HANDLE ConnectionPortHandle
)
{
    NTSTATUS Status;
    ULONG MessageAttributesMask;
    SIZE_T MessageAttributesSize;
    PALPC_MESSAGE_ATTRIBUTES MessageAttributes;
    SIZE_T RequiredBufferSize;
    PPORT_MESSAGE PortMessage;
    SIZE_T BufferLength;

    MessageAttributesMask = ALPC_MESSAGE_VIEW_ATTRIBUTE | \
        ALPC_MESSAGE_CONTEXT_ATTRIBUTE ;

    MessageAttributesSize = AlpcGetHeaderSize(MessageAttributesMask);
    MessageAttributes = RtAllocateZeroHeap(MessageAttributesSize);

    if (NULL == MessageAttributes) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = AlpcInitializeMessageAttribute(MessageAttributesMask,
                                            MessageAttributes,
                                            MessageAttributesSize,
                                            &RequiredBufferSize);

    if (FALSE == NT_SUCCESS(Status)) {
        RtFreeHeap(MessageAttributes);
        return Status;
    }

    PortMessage = RtAllocateZeroHeap(ALPC_MAX_MESSAGE_LENGTH);

    if (NULL == PortMessage) {
        RtFreeHeap(MessageAttributes);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    while (TRUE) {
        BufferLength = ALPC_MAX_MESSAGE_LENGTH;

        Status = NtAlpcSendWaitReceivePort(ConnectionPortHandle,
                                           0,
                                           NULL,
                                           NULL,
                                           PortMessage,
                                           &BufferLength,
                                           MessageAttributes,
                                           NULL);

        if (FALSE == NT_SUCCESS(Status)) {
            break;
        }

        Status = SrvProcessMessage(ConnectionPortHandle,
                                   PortMessage,
                                   MessageAttributes);

        if (FALSE == NT_SUCCESS(Status)) {
            break;
        }
    }

    RtFreeHeap(PortMessage);
    RtFreeHeap(MessageAttributes);

    return Status;
}

NTSTATUS NTAPI
SrvInitializeChannel (
    IN LPCWSTR PortName
)
{
    NTSTATUS Status;
    UNICODE_STRING PortNameString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ALPC_PORT_ATTRIBUTES PortAttributes;
    HANDLE ConnectionPortHandle;

    RtlInitUnicodeString(&PortNameString, PortName);

    InitializeObjectAttributes(&ObjectAttributes,
                               &PortNameString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    SrvInitializePortAttributes(&PortAttributes);

    Status = NtAlpcCreatePort(&ConnectionPortHandle,
                              &ObjectAttributes,
                              &PortAttributes);

    if (FALSE == NT_SUCCESS(Status)) {
        return Status;
    }

    Status = RtCreateThread(&SrvServerThreadHandle,
                            SrvServerWorker,
                            ConnectionPortHandle);

    if (FALSE == NT_SUCCESS(Status)) {
        NtClose(ConnectionPortHandle);
        return Status;
    }

    return STATUS_SUCCESS;
}
