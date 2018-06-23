#pragma once

#include <iostream>
#include <memory>
#include <mutex>

#include "../../include/NamedPipeTest.h"
#include "../include/npStorageContainer.h"

class npProtoHandler
{
public:
    using Proto_t = NamepPipe::npProto;
    using Payload_t = char[];
    using Return_t = std::tuple<NamepPipe::npStatus, std::unique_ptr<Proto_t>, std::unique_ptr<Payload_t>>;

    npProtoHandler() = delete;
    npProtoHandler(const npProtoHandler&) = delete;
    npProtoHandler(npProtoHandler&&) = delete;
    npProtoHandler& operator=(npProtoHandler&&) = delete;
    npProtoHandler& operator=(const npProtoHandler&) = delete;

    npProtoHandler(StorageContainer& iContainer)
        : storage(iContainer)
    {};
    virtual ~npProtoHandler() {};

    Return_t handlePacket(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload);

private:
    Return_t set(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload);
    Return_t bye(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload);
    Return_t get(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload);

    StorageContainer& storage;
};