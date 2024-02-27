#include <ntifs.h>
#include <windef.h>
#include <ntstrsafe.h>
#include <ntimage.h>
#include <intrin.h>
#include "ka.h"

PVOID
NTAPI
KaAllocateZeroHeap (
    IN SIZE_T Size
)
{
    PVOID BaseAddress;

    BaseAddress = ExAllocatePoolZero(PagedPool, Size, 'cplK');

    if (NULL != BaseAddress) {
        RtlZeroMemory(BaseAddress, Size);
    }

    return BaseAddress;
}

VOID
NTAPI
KaFreeHeap (
    IN PVOID BaseAddress
)
{
    ExFreePoolWithTag(BaseAddress, 'cplK');
}

VOID
NTAPI
KaInitializePortAttributes (
    OUT PALPC_PORT_ATTRIBUTES PortAttributes
)
{
    RtlZeroMemory(PortAttributes, sizeof(ALPC_PORT_ATTRIBUTES));

    PortAttributes->Flags = ALPC_PORFLG_SYSTEM_PROCESS;
    PortAttributes->MaxMessageLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;
}

NTSTATUS
NTAPI
KaAlpcRequest (
    IN PKA_SERVER_OBJECT ServerObject
)
{
    ServerObject->OnRequest(ServerObject->PortMessage);

    ZwAlpcSendWaitReceivePort(ServerObject->ConnectionPortHandle,
                              ALPC_MSGFLG_RELEASE_MESSAGE,
                              ServerObject->PortMessage,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL);

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
KaAlpcDatagram (
    IN PKA_SERVER_OBJECT ServerObject
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
        ZwAlpcCancelMessage(ServerObject->ConnectionPortHandle, 0, ContextAttribute);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
KaAlpcDisconnect (
    IN PKA_SERVER_OBJECT ServerObject
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

    Status = ZwClose(PortContext->CommunicationPortHandle);

    KaFreeHeap(PortContext);

    return Status;
}

NTSTATUS
NTAPI
KaAlpcConnect (
    IN PKA_SERVER_OBJECT ServerObject
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
        PortContext = KaAllocateZeroHeap(sizeof(PORT_CONTEXT));

        if (NULL != PortContext) {
            PortContext->ClientId = ServerObject->PortMessage->ClientId;

            Status = ZwAlpcAcceptConnectPort(&PortContext->CommunicationPortHandle,
                                             ServerObject->ConnectionPortHandle,
                                             0,
                                             &ObjectAttributes,
                                             &ServerObject->PortAttributes,
                                             PortContext,
                                             ServerObject->PortMessage,
                                             NULL,
                                             TRUE);

            if (FALSE == NT_SUCCESS (Status)) {
                ZwAlpcAcceptConnectPort(&CommunicationPortHandle,
                                        ServerObject->ConnectionPortHandle,
                                        0,
                                        &ObjectAttributes,
                                        &ServerObject->PortAttributes,
                                        NULL,
                                        ServerObject->PortMessage,
                                        NULL,
                                        FALSE);

                KaFreeHeap(PortContext);
            }
        }
        else {
            ZwAlpcAcceptConnectPort(&CommunicationPortHandle,
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
        ZwAlpcAcceptConnectPort(&CommunicationPortHandle,
                                ServerObject->ConnectionPortHandle,
                                0,
                                &ObjectAttributes,
                                &ServerObject->PortAttributes,
                                NULL,
                                ServerObject->PortMessage,
                                NULL,
                                FALSE);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
KaProcessMessage (
    IN PKA_SERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;

    switch (LOBYTE(ServerObject->PortMessage->u2.s2.Type)) {
    case LPC_REQUEST:
        Status = KaAlpcRequest(ServerObject);
        break;
    case LPC_DATAGRAM:
        Status = KaAlpcDatagram(ServerObject);
        break;
    case LPC_PORT_CLOSED:
        Status = KaAlpcDisconnect(ServerObject);
        break;
    case LPC_CLIENT_DIED:
        Status = KaAlpcDisconnect(ServerObject);
        break;
    case LPC_CONNECTION_REQUEST:
        Status = KaAlpcConnect(ServerObject);
        break;
    default:
        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

VOID
NTAPI
KaServerWorker (
    IN PKA_SERVER_OBJECT ServerObject
)
{
    NTSTATUS Status;
    SIZE_T BufferLength;

    while (TRUE) {
        BufferLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;

        Status = ZwAlpcSendWaitReceivePort(ServerObject->ConnectionPortHandle,
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

        Status = KaProcessMessage(ServerObject);

        if (FALSE == NT_SUCCESS(Status)) {
            break;
        }
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID
NTAPI
KaTerminateServer (
    IN PKA_SERVER_OBJECT ServerObject
)
{
    ZwClose(ServerObject->ConnectionPortHandle);
    ZwWaitForSingleObject(ServerObject->ServerThreadHandle, FALSE, NULL);

    KaFreeHeap(ServerObject->PortMessage);
    KaFreeHeap(ServerObject->MessageAttributes);
    KaFreeHeap(ServerObject);
}

PKA_SERVER_OBJECT
NTAPI
KaCreateServer (
    IN LPCWSTR PortName,
    IN REQUEST_PROCEDURE OnRequest,
    IN DATAGRAM_PROCEDURE OnDatagram,
    IN CONNECTION_PROCEDURE OnConnect
)
{
    NTSTATUS Status;
    PKA_SERVER_OBJECT ServerObject;
    ULONG MessageAttributesMask;
    SIZE_T MessageAttributesSize;
    SIZE_T RequiredBufferSize;
    OBJECT_ATTRIBUTES ObjectAttributes;

    ServerObject = KaAllocateZeroHeap(sizeof(KA_SERVER_OBJECT));

    if (NULL == ServerObject) {
        goto Cleanup;
    }

    ServerObject->OnRequest = OnRequest;
    ServerObject->OnDatagram = OnDatagram;
    ServerObject->OnConnect = OnConnect;

    MessageAttributesMask = ALPC_MESSAGE_VIEW_ATTRIBUTE | \
        ALPC_MESSAGE_CONTEXT_ATTRIBUTE;

    MessageAttributesSize = AlpcGetHeaderSize(MessageAttributesMask);

    ServerObject->MessageAttributes = KaAllocateZeroHeap(MessageAttributesSize);

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

    ServerObject->PortMessage = KaAllocateZeroHeap(ALPC_MAX_ALLOWED_MESSAGE_LENGTH);

    if (NULL == ServerObject->PortMessage) {
        goto Cleanup;
    }

    KaInitializePortAttributes(&ServerObject->PortAttributes);

    RtlInitUnicodeString(&ServerObject->PortName, PortName);

    InitializeObjectAttributes(&ObjectAttributes,
                               &ServerObject->PortName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwAlpcCreatePort(&ServerObject->ConnectionPortHandle,
                              &ObjectAttributes,
                              &ServerObject->PortAttributes);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    Status = PsCreateSystemThread(&ServerObject->ServerThreadHandle,
                                  THREAD_ALL_ACCESS,
                                  &ObjectAttributes,
                                  NULL,
                                  NULL,
                                  KaServerWorker,
                                  NULL);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    return ServerObject;

Cleanup:

    if (NULL != ServerObject) {
        if (NULL != ServerObject->ConnectionPortHandle) {
            ZwClose(ServerObject->ConnectionPortHandle);
        }

        if (NULL != ServerObject->MessageAttributes) {
            KaFreeHeap(ServerObject->MessageAttributes);
        }

        if (NULL != ServerObject->PortMessage) {
            KaFreeHeap(ServerObject->PortMessage);
        }

        KaFreeHeap(ServerObject);
    }

    return NULL;
}

BOOLEAN
NTAPI
KaSendSynchronousRequest (
    IN PKA_CLIENT_OBJECT ClientObject,
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

    PortMessage = KaAllocateZeroHeap(TotalLength);

    if (NULL == PortMessage) {
        return FALSE;
    }

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = RequestDataLength;

    DataBuffer = PortMessage + 1;

    RtlCopyMemory(DataBuffer, RequestDataBuffer, RequestDataLength);

    BufferLength = TotalLength;

    Status = ZwAlpcSendWaitReceivePort(ClientObject->CommunicationPortHandle,
                                       ALPC_MSGFLG_SYNC_REQUEST,
                                       PortMessage,
                                       NULL,
                                       PortMessage,
                                       &BufferLength,
                                       NULL,
                                       NULL);

    RtlCopyMemory(RequestDataBuffer, DataBuffer, RequestDataLength);

    KaFreeHeap(PortMessage);

    return NT_SUCCESS(Status);
}

BOOLEAN
NTAPI
KaSendDatagram (
    IN PKA_CLIENT_OBJECT ClientObject,
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

    PortMessage = KaAllocateZeroHeap(TotalLength);

    if (NULL == PortMessage) {
        return FALSE;
    }

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = DatagramLength;

    DataBuffer = PortMessage + 1;

    RtlCopyMemory(DataBuffer, DatagramBuffer, DatagramLength);

    Status = ZwAlpcSendWaitReceivePort(ClientObject->CommunicationPortHandle,
                                       ALPC_MSGFLG_RELEASE_MESSAGE,
                                       PortMessage,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

    KaFreeHeap(PortMessage);

    return NT_SUCCESS(Status);
}

VOID
NTAPI
KaDisconnectServer (
    IN PKA_CLIENT_OBJECT ClientObject
)
{
    ZwClose(ClientObject->CommunicationPortHandle);
    KaFreeHeap(ClientObject);
}

PKA_CLIENT_OBJECT
NTAPI
KaConnectServer (
    IN LPCWSTR PortName,
    IN PVOID ConnectionDataBuffer OPTIONAL,
    IN USHORT ConnectionDataLength
)
{
    NTSTATUS Status;
    PKA_CLIENT_OBJECT ClientObject;
    SIZE_T BufferLength;
    USHORT TotalLength;
    PPORT_MESSAGE PortMessage;
    PVOID DataBuffer;

    ClientObject = KaAllocateZeroHeap(sizeof(KA_CLIENT_OBJECT));

    if (NULL == ClientObject) {
        goto Cleanup;
    }

    TotalLength = sizeof(PORT_MESSAGE) + ConnectionDataLength;

    if (TotalLength > ALPC_MAX_ALLOWED_MESSAGE_LENGTH) {
        goto Cleanup;
    }

    PortMessage = KaAllocateZeroHeap(TotalLength);

    if (NULL == PortMessage) {
        goto Cleanup;
    }

    PortMessage->u1.s1.TotalLength = TotalLength;
    PortMessage->u1.s1.DataLength = ConnectionDataLength;

    DataBuffer = PortMessage + 1;

    RtlCopyMemory(DataBuffer, ConnectionDataBuffer, ConnectionDataLength);

    KaInitializePortAttributes(&ClientObject->PortAttributes);

    RtlInitUnicodeString(&ClientObject->PortName, PortName);

    BufferLength = ALPC_MAX_ALLOWED_MESSAGE_LENGTH;

    Status = ZwAlpcConnectPort(&ClientObject->CommunicationPortHandle,
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

    KaFreeHeap(PortMessage);

    if (FALSE == NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    return ClientObject;

Cleanup:

    if (NULL != ClientObject) {
        KaFreeHeap(ClientObject);
    }

    return NULL;
}
