#include <iostream>
#include <future>

#include "../../include/NamedPipeTest.h"
#include "../include/npUtilities.h"

#define BUFSIZE 512

int main()
{
    DWORD  dwThreadId = 0;
    HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;
    LPTSTR lpszPipename = NamepPipe::myPipeName;

    ReplyChecker aChecker;
    std::thread aCheckerThread(&ReplyChecker::check, &aChecker);

    StorageContainer aContainer;
    npProtoHandler aProtoHandler(aContainer);
    ClientHandler aHandler(aProtoHandler);

    for (;;)
    {
        _tprintf(TEXT("\nPipe Server: Main thread awaiting client connection on %s\n"), lpszPipename);
        hPipe = CreateNamedPipe(
            lpszPipename,             // pipe name 
            PIPE_ACCESS_DUPLEX,       // read/write access 
            PIPE_READMODE_BYTE |      // byte type pipe 
            PIPE_READMODE_BYTE |      // byte-read mode 
            PIPE_WAIT,                // blocking mode 
            PIPE_UNLIMITED_INSTANCES, // max. instances  
            BUFSIZE,                  // output buffer size 
            BUFSIZE,                  // input buffer size 
            0,                        // client time-out 
            NULL);                    // default security attribute 

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            _tprintf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
            return -1;
        }

        // Wait for the client to connect; if it succeeds, 
        // the function returns a nonzero value. If the function
        // returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

        bool fConnected = ConnectNamedPipe(hPipe, NULL) ?
            true : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (fConnected)
        {
            printf("Client connected, creating a processing thread.\n");

            aChecker.push(std::async(std::launch::async, &ClientHandler::InstanceThread, &aHandler, hPipe));
        }
        else
        {
            // The client could not connect, so close the pipe. 
            CloseHandle(hPipe);
        }
    }

    aChecker.stop();
    aCheckerThread.join();

    return 0;
}