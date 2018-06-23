#pragma once

#include <cstddef>
#include <cstdint>
#include <windows.h> 
#include <tchar.h>

#define FOURCC(a,b,c,d) ( (uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)) )

namespace NamepPipe
{
    static const LPTSTR myPipeName = TEXT("\\\\.\\pipe\\mynamedpipe");

    enum npMessageType
    {
        Request = 1,
        Response = 2
    };

    enum npStatus
    {
        NA = 0, //not applicable
        OK = 1,
        ERR = -1
    };

    enum npFunction
    {
        Reply = 0, // not applicable, the message is a reply
        Connect = 1, //actually not used
        Get = 2,
        Set = 3,
        Disconnect = 4
    };

    struct npProto
    {
        npMessageType type;
        npStatus status;
        npFunction function;
        size_t payload_size;
    };

    struct npObjectHeader
    {
        uint32_t type;
        size_t size;
    };

    struct npId
    {
        npObjectHeader header;
        uint32_t type;
        uintptr_t id;
    };
    static const npObjectHeader npIdHeader = { FOURCC('n','p','i','d'), sizeof(npId) };

    struct npPerson
    {
        npObjectHeader header;
        char firstName[32];
        char lastName[32];
        uint8_t age;
    };
    static const npObjectHeader npPersonHeader = { FOURCC('p','e','r','s'), sizeof(npPerson) };

    struct npCar
    {
        npObjectHeader header;
        char make[32];
        char model[32];
        uint16_t yearMade;
        uint32_t mileage;
    };
    static const npObjectHeader npCarHeader = { FOURCC('n','c','a','r'), sizeof(npCar) };

} //namespace NamepPipe