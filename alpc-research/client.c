#include <ntum.h>
#include "runtime.h"
#include "client.h"
#include <stdio.h>

VOID NTAPI
CliInitializePortAttributes (
    OUT PALPC_PORT_ATTRIBUTES PortAttributes
)
{
    RtlZeroMemory(PortAttributes, sizeof(ALPC_PORT_ATTRIBUTES));

    PortAttributes->MaxMessageLength = ALPC_MAX_MESSAGE_LENGTH;
}

VOID NTAPI
CliInitializePortMessage (
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
CliTestDatagram (
    IN LPCWSTR PortName
)
{
    NTSTATUS Status;
    UNICODE_STRING PortNameString;
    HANDLE CommunicationPortHandle;
    PPORT_MESSAGE PortMessage;
    LARGE_INTEGER DelayInterval;
    ALPC_PORT_ATTRIBUTES PortAttributes;
    SIZE_T BufferLength;
    LARGE_INTEGER Counter = { 0 };

    DelayInterval.QuadPart = Int32x32To64(1000, -10 * 1000);
    NtDelayExecution(FALSE, &DelayInterval);

    PortMessage = RtAllocateZeroHeap(ALPC_MAX_MESSAGE_LENGTH);

    if (NULL == PortMessage) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlInitUnicodeString(&PortNameString, PortName);

    CliInitializePortAttributes(&PortAttributes);
    CliInitializePortMessage(PortMessage, NULL, 0);

    BufferLength = ALPC_MAX_MESSAGE_LENGTH;

    Status = NtAlpcConnectPort(&CommunicationPortHandle,
                               &PortNameString,
                               NULL,
                               &PortAttributes,
                               ALPC_MSGFLG_SYNC_REQUEST,
                               NULL,
                               PortMessage,
                               &BufferLength,
                               NULL,
                               NULL,
                               NULL);

    if (FALSE == NT_SUCCESS(Status)) {
        RtFreeHeap(PortMessage);
        return Status;
    }

    while (Counter.QuadPart < 10000000) {
        CliInitializePortMessage(PortMessage, &Counter, sizeof(Counter));
 
        Status = NtAlpcSendWaitReceivePort(CommunicationPortHandle,
                                           ALPC_MSGFLG_RELEASE_MESSAGE,
                                           PortMessage,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
 
        Counter.QuadPart += 1;
    }

    DelayInterval.QuadPart = Int32x32To64(15000, -10 * 1000);
    NtDelayExecution(FALSE, &DelayInterval);

    NtClose(CommunicationPortHandle);
    RtFreeHeap(PortMessage);

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
CliTestSyncRequest (
    IN LPCWSTR PortName
)
{
    NTSTATUS Status;
    UNICODE_STRING PortNameString;
    HANDLE CommunicationPortHandle;
    PPORT_MESSAGE PortMessage;
    LARGE_INTEGER DelayInterval;
    ALPC_PORT_ATTRIBUTES PortAttributes;
    SIZE_T BufferLength;
    LARGE_INTEGER Counter = { 0 };

    DelayInterval.QuadPart = Int32x32To64(1000, -10 * 1000);
    NtDelayExecution(FALSE, &DelayInterval);

    PortMessage = RtAllocateZeroHeap(ALPC_MAX_MESSAGE_LENGTH);

    if (NULL == PortMessage) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlInitUnicodeString(&PortNameString, PortName);

    CliInitializePortAttributes(&PortAttributes);
    CliInitializePortMessage(PortMessage, NULL, 0);

    BufferLength = ALPC_MAX_MESSAGE_LENGTH;

    Status = NtAlpcConnectPort(&CommunicationPortHandle,
                               &PortNameString,
                               NULL,
                               &PortAttributes,
                               ALPC_MSGFLG_SYNC_REQUEST,
                               NULL,
                               PortMessage,
                               &BufferLength,
                               NULL,
                               NULL,
                               NULL);

    if (FALSE == NT_SUCCESS(Status)) {
        RtFreeHeap(PortMessage);
        return Status;
    }

    while (Counter.QuadPart < 10000000) {
        CliInitializePortMessage(PortMessage, &Counter, sizeof(Counter));

        BufferLength = ALPC_MAX_MESSAGE_LENGTH;

        Status = NtAlpcSendWaitReceivePort(CommunicationPortHandle,
                                           ALPC_MSGFLG_SYNC_REQUEST,
                                           PortMessage,
                                           NULL,
                                           PortMessage,
                                           &BufferLength,
                                           NULL,
                                           NULL);

        Counter.QuadPart += 1;
    }

    NtClose(CommunicationPortHandle);
    RtFreeHeap(PortMessage);

    return STATUS_SUCCESS;
}
