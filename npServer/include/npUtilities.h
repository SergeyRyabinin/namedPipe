#pragma once

#include "../../include/NamedPipeTest.h"
#include "../include/npStorageContainer.h"
#include "../include/npProtoHandler.h"

class ClientHandler
{
public:
    ClientHandler(npProtoHandler& iHandler)
        : protoHandler(iHandler)
    {};

    // Main server function to handle client connections in async mode. Each client is in a separate thread
    uint32_t InstanceThread(LPVOID lpvParam);

private:
    npProtoHandler::Return_t
    Receive(HANDLE hPipe, std::unique_ptr<npProtoHandler::Proto_t> ioProto);

    bool 
    Send(HANDLE hPipe, std::unique_ptr<npProtoHandler::Proto_t> iReplyProto, std::unique_ptr<npProtoHandler::Payload_t> iReplyPayload);

    npProtoHandler& protoHandler;
};

class ReplyChecker
{
public:
    virtual ~ReplyChecker()
    {
        alive = false;

        //exceptions must not leave destructor
        try
        {
            queue.clear();
        }
        catch(...)
        {
            std::cerr << "Something very bad happened in destructing async future objects!\n";
        }
    }

    void push(std::future<uint32_t>&& aThread)
    {
        std::lock_guard<std::mutex> lock(queueMtx);
        queue.push_back(std::move(aThread));
    }

    void stop()
    {
        alive = false;
    }

    void check()
    {
        while (alive)
        {
            //TODO: use proper syncronization method
            std::this_thread::sleep_for(std::chrono::nanoseconds(10000000)); //10 ms
            auto aThread = get_ready();
            if (aThread.valid())
            {
                try
                {
                    std::cout << "Thread result: " << aThread.get() << "\n";
                }
                catch (std::runtime_error& aExcept)
                {
                    std::cout << "Exception happened! " << aExcept.what() << "\n";
                }
                catch (std::exception& aExcept)
                {
                    std::cout << "Exception happened! " << aExcept.what() << "\n";
                }
                catch (...)
                {
                    std::cout << "Unknown exception happened!\n";
                }
            }
        }
    }

private:
    std::future<uint32_t> get_ready()
    {
        std::lock_guard<std::mutex> lock(queueMtx);
        if (!queue.empty())
        {
            for (auto it = queue.begin(); it != queue.end(); ++it)
            {
                if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    auto ret = std::move(*it);
                    queue.erase(it);
                    return ret;
                }
            }
        }
        return std::future<uint32_t>();
    }

    mutable std::mutex queueMtx;
    bool alive = true;
    std::list<std::future<uint32_t>> queue;
};