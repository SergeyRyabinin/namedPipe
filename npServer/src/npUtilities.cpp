#include <future>
#include <mutex>
#include <queue>
#include <iostream>
#include <thread>

#include "../include/npUtilities.h"

uint32_t ClientHandler::InstanceThread(LPVOID lpvParam)
// Main server function to handle client connections in async mode. Each client is in a separate thread
{
    HANDLE hHeap = GetProcessHeap();

    DWORD cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;
    HANDLE hPipe = NULL;

    if (lpvParam == NULL)
    {
        std::cerr << "\nERROR - Pipe Server Failure:\n";
        std::cerr << "   InstanceThread got an unexpected NULL value in lpvParam.\n";
        std::cerr << "   InstanceThread exitting.\n";
        return (DWORD)-1;
    }

    // Print verbose messages. In production code, this should be for debugging only.
    std::cout << "InstanceThread created, receiving and processing messages.\n";

    // The thread's parameter is a handle to a pipe object instance. 
    hPipe = (HANDLE)lpvParam;

    std::unique_ptr<npProtoHandler::Proto_t> aProtoRequest = std::make_unique<npProtoHandler::Proto_t>();

    bool alive = true;
    while (alive)
    {
        NamepPipe::npStatus aStatus = NamepPipe::OK;
        std::unique_ptr<npProtoHandler::Payload_t> aRequestPayload;
        std::tie(aStatus, aProtoRequest, aRequestPayload) = Receive(hPipe, std::move(aProtoRequest));
        if (!aProtoRequest)
        {
            std::cerr << "Failed to read message!\n";
            break;
        }
        else
        {
            std::cout << "Got new message\n";
        }
        if (NamepPipe::Disconnect == aProtoRequest->function)
        {
            alive = false;
        }

        std::unique_ptr<npProtoHandler::Proto_t> aReplyProto;
        std::unique_ptr<npProtoHandler::Payload_t> aReplyPayload;
        std::tie(aStatus, aReplyProto, aReplyPayload) = protoHandler.handlePacket(std::move(aProtoRequest), std::move(aRequestPayload));

        if (!Send(hPipe, std::move(aReplyProto), std::move(aReplyPayload)))
        {
            std::cerr << "Failed to send message!\n";
            break;
        }
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    std::cout << "InstanceThread exitting.\n";
    return 0;
}

npProtoHandler::Return_t
ClientHandler::Receive(HANDLE hPipe, std::unique_ptr<npProtoHandler::Proto_t> ioProto)
{
    DWORD fSuccess = FALSE;
    DWORD cbBytesRead = 0;
    if (!ioProto)
        ioProto = std::make_unique<npProtoHandler::Proto_t>();

    bool read = false;
    auto WaitTill = std::chrono::system_clock::now() + std::chrono::seconds(20);
    while (!read)
    {
        fSuccess = ReadFile(
            hPipe,                           // handle to pipe 
            ioProto.get(),                   // buffer to receive data 
            sizeof(npProtoHandler::Proto_t), // size of buffer 
            &cbBytesRead,                    // number of bytes read 
            NULL);                           // not overlapped I/O 

        if (!fSuccess || cbBytesRead == 0)
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
            {
                std::cerr << "InstanceThread: client disconnected.\n";
            }
            else if (WaitTill > std::chrono::system_clock::now())
            {
                // This is kinda workaround for cases when there is a delay between header and payload coming.
                //...did not want to bother with concatenating 2 buffers into one and sending it at once
                std::this_thread::sleep_for(std::chrono::nanoseconds(10000000)); //10 ms
                continue;
            }
            else
            {
                std::cerr << "InstanceThread ReadFile request failed, GLE=" << GetLastError() << "\n";
                std::cerr << "Disconnecting by timeout!\n";
            }
            break;
        }
        else
        {
            read = true;
        }
    }

    if (cbBytesRead != sizeof(npProtoHandler::Proto_t))
    {
        std::cerr << "Unknown request\n";
        return npProtoHandler::Return_t();
    }

    std::unique_ptr<npProtoHandler::Payload_t> aPayload;
    if (ioProto->payload_size)
    {
        aPayload = std::make_unique<char[]>(ioProto->payload_size);

        bool read = false;
        std::chrono::nanoseconds timeToSleep(1000000000); //1s
        while (!read)
        {
            fSuccess = ReadFile(
                hPipe,        // handle to pipe 
                aPayload.get(),    // buffer to receive data 
                ioProto->payload_size, // size of buffer 
                &cbBytesRead, // number of bytes read 
                NULL);        // not overlapped I/O 

            if (!fSuccess || 0 == cbBytesRead)
            {
                if (timeToSleep > std::chrono::nanoseconds(10))
                {
                    // This is kinda workaround for cases when there is a delay between header and payload coming.
                    //...did not want to bother with concatenating 2 buffers into one and sending it at once
                    std::this_thread::sleep_for(std::chrono::nanoseconds(10000000)); //10 ms
                    timeToSleep -= std::chrono::nanoseconds(10000000);
                    continue;
                }
                std::cerr << "InstanceThread ReadFile payload failed, GLE= " << GetLastError() << "\n";
                break;
            }
            else
            {
                read = true;
            }
        }
    }

    return std::make_tuple(NamepPipe::OK, std::move(ioProto), std::move(aPayload));
}

bool ClientHandler::Send(HANDLE hPipe, std::unique_ptr<npProtoHandler::Proto_t> iReplyProto, std::unique_ptr<npProtoHandler::Payload_t> iReplyPayload)
{
    if (!hPipe)
    {
        throw std::runtime_error("Send received nullptr hPipe");
    }

    DWORD fSuccess = FALSE;
    DWORD cbWritten = 0;

    std::unique_ptr<char[]> buffToSend = 0;
    size_t buffSize = 0;

    if (iReplyProto && iReplyProto->payload_size)
    {
        if (!iReplyPayload)
        {
            std::cerr << "Protocol handler declared payload size but not provided payload!\n";
            return false;
        }

        buffSize = sizeof(npProtoHandler::Proto_t) + iReplyProto->payload_size;
        buffToSend = std::make_unique<char[]>(sizeof(npProtoHandler::Proto_t) + iReplyProto->payload_size);
        memcpy_s(buffToSend.get(), buffSize, iReplyProto.get(), sizeof(npProtoHandler::Proto_t));
        memcpy_s(buffToSend.get() + sizeof(npProtoHandler::Proto_t),
            buffSize - sizeof(npProtoHandler::Proto_t),
            iReplyPayload.get(),
            iReplyProto->payload_size);
    }
    else if (iReplyProto)
    {
        buffToSend.reset(reinterpret_cast<char*>(iReplyProto.get()));
        iReplyProto.release();
        buffSize = sizeof(npProtoHandler::Proto_t);
    }

    if (buffToSend)
    {
        fSuccess = WriteFile(
            hPipe,        // handle to pipe 
            buffToSend.get(),     // buffer to write from 
            buffSize, // number of bytes to write 
            &cbWritten,   // number of bytes written 
            NULL);        // not overlapped I/O 
    }
    if (!fSuccess || buffSize != cbWritten)
    {
        std::cerr << "InstanceThread WriteFile failed, GLE= " << GetLastError() << "\n";
        return false;
    }

    return true;
}