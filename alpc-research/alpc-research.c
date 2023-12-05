#include <stdio.h>
#include <ntum.h>
#include "runtime.h"
#include "server.h"
#include "client.h"

int main(int argc, char *argv[])
{
    NTSTATUS Status;

    Status = SrvInitializeChannel(L"\\ArCommendPort");

    if (FALSE != NT_SUCCESS(Status)) {
        CliTestDatagram(L"\\ArCommendPort");
        CliTestSyncRequest(L"\\ArCommendPort");
    }
    else {
        printf("SrvInitializeChannel Failed:%08x\n", Status);
    }

    getchar();
    return 0;
}
