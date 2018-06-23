#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <string> 

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#include <unordered_map>

#include "../../include/NamedPipeTest.h"

#define BUFSIZE 512

HANDLE OpenPipe(const LPTSTR& lpszPipename)
{
    HANDLE hPipe;

    while (1)
    {
        hPipe = CreateFile(
            lpszPipename,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 

                            // Break if the pipe handle is valid. 

        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            std::cerr << "Could not open pipe. GLE=" << GetLastError() << "\n";
            return nullptr;
        }

        // All pipe instances are busy, so wait for 20 seconds. 

        if (!WaitNamedPipe(lpszPipename, 20000))
        {
            std::cerr << "Could not open pipe: 20 second wait timed out.";
            return nullptr;
        }

        // The pipe connected; change to message-read mode. 
        DWORD dwMode = PIPE_READMODE_BYTE;
        BOOL fSuccess = SetNamedPipeHandleState(
            hPipe,    // pipe handle 
            &dwMode,  // new pipe mode 
            NULL,     // don't set maximum bytes 
            NULL);    // don't set maximum time 
        if (!fSuccess)
        {
            std::cerr << "SetNamedPipeHandleState failed. GLE=" << GetLastError() << "\n";
            return nullptr;
        }
        else
        {
            break;
        }
    }

    return hPipe;
}

bool npSend(HANDLE hPipe, void* message, size_t message_sz)
{
    // Send a message to the pipe server. 
    std::cerr << "Sending << " << message_sz << " byte message: " << message << "\n";

    DWORD cbWritten = 0;
    DWORD fSuccess = WriteFile(
        hPipe,                  // pipe handle 
        message,                // message 
        message_sz,             // message length 
        &cbWritten,             // bytes written 
        NULL);                  // not overlapped 

    if (!fSuccess)
    {
        std::cerr << "WriteFile to pipe failed. GLE=" << GetLastError() << "\n";
        return false;
    }

    std::cout << "\nMessage sent to server, receiving reply as follows:\n";

    return true;
}

std::tuple<std::unique_ptr<NamepPipe::npProto>, std::unique_ptr<char[]> >
npReceive(HANDLE hPipe)
{
    using Return_t = std::tuple<std::unique_ptr<NamepPipe::npProto>, std::unique_ptr<char[]> >;

    // Read from the pipe. 
    bool read = false;
    auto WaitTill = std::chrono::system_clock::now() + std::chrono::seconds(20);
    DWORD fSuccess = FALSE;
    DWORD  cbRead = 0;
    std::unique_ptr<NamepPipe::npProto> aProto = std::make_unique<NamepPipe::npProto>();

    while (!read)
    {
        DWORD fSuccess = ReadFile( 
            hPipe,    // pipe handle 
            aProto.get(),    // buffer to receive reply 
            sizeof(NamepPipe::npProto),  // size of buffer 
            &cbRead,  // number of bytes read 
            NULL);    // not overlapped 

        if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
        {
            if (WaitTill > std::chrono::system_clock::now())
            {
                // This is kinda workaround for cases when there is a delay between header and payload coming.
                //...did not want to bother with concatenating 2 buffers into one and sending it at once
                std::this_thread::sleep_for(std::chrono::nanoseconds(10000000)); //10 ms
                continue;
            }
            else
            {
                std::cerr << "Could not read reply!\n";
                return Return_t();
            }
            break;
        }
        else
        {
            read = true;
        }
    }

    std::unique_ptr<char[]> aPayload;
    if (aProto->payload_size)
    {
        aPayload = std::make_unique<char[]>(aProto->payload_size);

        fSuccess = ReadFile(
            hPipe,    // pipe handle 
            aPayload.get(),    // buffer to receive reply 
            aProto->payload_size,  // size of buffer 
            &cbRead,  // number of bytes read 
            NULL);    // not overlapped 

        std::cout << "Received reply payload size = " << aProto->payload_size << "\n";
        std::cout << "   payload: " << aPayload.get() << "\n";
    }

    return std::make_tuple(std::move(aProto), std::move(aPayload));
}

static void show_usage()
{
    std::cerr << 
        "Usage example: \"npClient.exe 3:Set,Person(Jonh,Smith,27);Set,Car(Toyota,Yaris,2015,45234);Get,Id(Person,2645722721488)\"\n\n" <<
        "   where 3 - number of commands (not actually used, just put any number)\n" <<
        "   where \"Set,Person(Jonh,Smith,27)\"" <<
        "- first command to send, i.e. Set new object Person with provided values\n" <<
        "   where \"Get,Id(Person,2645722721488)\"" <<
        "- last command to send, i.e. Get a stored object of type Person with id \"2645722721488\"\n\n";
}

class Order
{
public:
    Order(const std::string& strOrder)
    {
        //"Set,Person(Jonh,Smith,27);"
        size_t lastPos = strOrder.find_first_of(",");
        std::string aFunc = strOrder.substr(0, lastPos);

        aProto.status = NamepPipe::OK;
        aProto.type = NamepPipe::Request;
        try {
            aProto.function = aFuncMap.at(aFunc);
        }
        catch (std::out_of_range& aExcept) {
            std::cout << "Unknown function in the arguments!\n";
            throw aExcept;
        }

        std::string aType = strOrder.substr(lastPos + 1, strOrder.find_first_of("(", lastPos) - lastPos - 1);
        lastPos = strOrder.find_first_of("(", lastPos);
        std::string aArgs = strOrder.substr(lastPos+1, strOrder.find_first_of(")", lastPos) - lastPos - 1);
        try {
            std::tie(aProto.payload_size, aPayload) = aFactoryMap.at(aType)(aArgs);
        }
        catch (std::out_of_range& aExcept) {
            std::cout << "Unknown type in the arguments!\n";
            throw aExcept;
        }
    }

    NamepPipe::npProto aProto;
    std::unique_ptr<char[]> aPayload;

private:

    static const std::unordered_map<std::string, NamepPipe::npFunction> aFuncMap;

    using FactoryFunc = std::function<std::tuple<size_t,std::unique_ptr<char[]> >(const std::string&)>;
    static const std::unordered_map<std::string, FactoryFunc> aFactoryMap;
};

const std::unordered_map<std::string, NamepPipe::npFunction> Order::aFuncMap =
{
    {"Reply"     , NamepPipe::Reply     },
    {"Connect"   , NamepPipe::Connect   },
    {"Get"       , NamepPipe::Get       },
    {"Set"       , NamepPipe::Set       },
    {"Disconnect", NamepPipe::Disconnect}
};

const std::unordered_map<std::string, Order::FactoryFunc> Order::aFactoryMap =
{
    { "Person"     , [](const std::string& aStr) 
        {
            std::unique_ptr<char[]> aPtr = std::make_unique<char[]>(sizeof(NamepPipe::npPerson));
            NamepPipe::npPerson& aPerson = *reinterpret_cast<NamepPipe::npPerson*>(aPtr.get());
            aPerson.header = NamepPipe::npPersonHeader;
            size_t lastPos = 0;
            std::string aFirstName = aStr.substr(lastPos, aStr.find_first_of(",", lastPos) - lastPos);
            memcpy_s(&aPerson.firstName, sizeof(aPerson.firstName), aFirstName.c_str(), aFirstName.size());

            lastPos = aStr.find_first_of(",", lastPos) + 1;
            std::string aLastName = aStr.substr(lastPos, aStr.find_first_of(",", lastPos) - lastPos);
            memcpy_s(&aPerson.lastName, sizeof(aPerson.lastName), aLastName.c_str(), aLastName.size());

            lastPos = aStr.find_first_of(",", lastPos) + 1;
            aPerson.age = std::stoi(aStr.substr(lastPos, aStr.find_first_of(")", lastPos) - lastPos));

            return std::make_tuple(size_t(aPerson.header.size), std::move(aPtr));
        } },

    { "Car"     , [](const std::string& aStr) 
        {
            std::unique_ptr<char[]> aPtr = std::make_unique<char[]>(sizeof(NamepPipe::npCar));
            NamepPipe::npCar& aCar = *reinterpret_cast<NamepPipe::npCar*>(aPtr.get());
            aCar.header = NamepPipe::npCarHeader;
            size_t lastPos = 0;
            std::string aMake = aStr.substr(lastPos, aStr.find_first_of(",", lastPos) - lastPos);
            memcpy_s(&aCar.make, sizeof(aCar.make), aMake.c_str(), aMake.size());

            lastPos = aStr.find_first_of(",", lastPos) + 1;
            std::string aModel = aStr.substr(lastPos, aStr.find_first_of(",", lastPos) - lastPos);
            memcpy_s(&aCar.model, sizeof(aCar.model), aModel.c_str(), aModel.size());

            lastPos = aStr.find_first_of(",", lastPos) + 1;
            aCar.yearMade = std::stoi(aStr.substr(lastPos, aStr.find_first_of(",", lastPos) - lastPos));

            lastPos = aStr.find_first_of(",", lastPos) + 1;
            aCar.mileage = std::stoi(aStr.substr(lastPos, aStr.find_first_of(")", lastPos) - lastPos));

            return std::make_tuple(size_t(aCar.header.size), std::move(aPtr));
        } },

    { "Id"     , [](const std::string& aStr) 
        {
            //"Person,12342"
            std::unique_ptr<char[]> aPtr = std::make_unique<char[]>(sizeof(NamepPipe::npId));
            NamepPipe::npId& aId = *reinterpret_cast<NamepPipe::npId*>(aPtr.get());
            aId.header = NamepPipe::npIdHeader;
            size_t lastPos = 0;
            std::string aType = aStr.substr(lastPos, aStr.find_first_of(",", lastPos) - lastPos);

            const std::unordered_map<std::string, NamepPipe::npObjectHeader> aTypeNameFourcc =
            {
                {"Person", NamepPipe::npPersonHeader},
                {"Car", NamepPipe::npCarHeader},
                {"Id", NamepPipe::npIdHeader},
            };
            aId.type = aTypeNameFourcc.at(aType).type;

            lastPos = aStr.find_first_of(",", lastPos) + 1;
            aId.id = std::stoll(aStr.substr(lastPos, aStr.find_first_of(")", lastPos) - lastPos));

            return std::make_tuple(size_t(aId.header.size), std::move(aPtr));
        } },
};

std::vector<Order> ParseInputCmd(const std::string& iCmd)
{
    // TODO: use boost::tokenizer
    std::string nStr = iCmd.substr(0, iCmd.find_first_of(":"));
    size_t nInt = std::stoi(nStr);
    size_t lastPos = iCmd.find_first_of(":");

    std::vector<std::string> aCmdStr;
    while (std::string::npos != lastPos)
    {
        if (std::string::npos != iCmd.find(";", lastPos))
        {
            aCmdStr.push_back(iCmd.substr(lastPos + 1, iCmd.find_first_of(";", lastPos+1) - lastPos-1));
            lastPos = iCmd.find_first_of(";", lastPos+1);
        }
        else
        {
            aCmdStr.push_back(iCmd.substr(lastPos + 1, iCmd.size()));
            break;
        }
    }

    std::vector<Order> aOrders;
    for (auto& aCmd : aCmdStr)
    {
        aOrders.emplace_back(Order(std::move(aCmd)));
    }

    return aOrders;
}

bool processOrder(HANDLE hPipe, Order& iOrder)
{
    if (!npSend(hPipe, &iOrder.aProto, sizeof(iOrder.aProto)))
    {
        {
            std::cerr << "Could not send proto message!\n";
            return false;
        }
    }
    if (iOrder.aPayload)
    {
        if (!npSend(hPipe, iOrder.aPayload.get(), iOrder.aProto.payload_size))
        {
            {
                std::cerr << "Could not send payload message!\n";
                return false;
            }
        }
    }

    std::unique_ptr<NamepPipe::npProto> aProto;
    std::unique_ptr<char[]> aPayload;
    std::tie(aProto, aPayload) = npReceive(hPipe);

    if (NamepPipe::Set == iOrder.aProto.function)
    {
        if (!aPayload)
        {
            std::cerr << "Expected a payload in a reply!\n";
        }
        else
        {
            NamepPipe::npId* pId = reinterpret_cast<NamepPipe::npId*>(aPayload.get());
            std::cout << "Ordered objected with type = " << pId->type << " inserted at index = " << pId->id << "\n";
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
    std::vector<Order> aOrders;
    if (argc != 2)
    {
        show_usage();
        return -1;
    }
    else
    {
        aOrders = ParseInputCmd(argv[1]);
    }

    BOOL   fSuccess = FALSE;

    HANDLE hPipe = OpenPipe(NamepPipe::myPipeName);
    if (!hPipe)
    {
        std::cerr << "Could not open pipe!\n";
        return -1;
    }

    for (auto& aOrder : aOrders)
    {
        if (!processOrder(hPipe, aOrder))
        {
            std::cerr << "Failed to process order\n";
        }
    }

   std::cout << "\n<No more orders to process, exiting>";

   NamepPipe::npProto aProtoBye = { NamepPipe::Request, NamepPipe::OK, NamepPipe::Disconnect, 0 };
   if (!npSend(hPipe, &aProtoBye, sizeof(aProtoBye)))
   {
       {
           std::cerr << "Could not send message!\n";
           return -1;
       }
   }
   npReceive(hPipe);

    //_getch();

    CloseHandle(hPipe);

    return 0;
}
