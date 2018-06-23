#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "../../include/NamedPipeTest.h"

class npPayloadWrapper : public NamepPipe::npObjectHeader
{
public:
    npPayloadWrapper(const NamepPipe::npObjectHeader& iHeader, const char* iPayload);

    npPayloadWrapper(const NamepPipe::npObjectHeader& iHeader, std::unique_ptr<char[]> iPayload);

    npPayloadWrapper(std::unique_ptr<char[]> iPayload);

    uintptr_t getId() const
    {
        return id;
    }

    template<typename T>
    T const * const as_T() const
    {
        return reinterpret_cast<T*>(payload.get());
    }

    npPayloadWrapper(const npPayloadWrapper&) = default;
    npPayloadWrapper(npPayloadWrapper&&) = default;
    npPayloadWrapper& operator=(const npPayloadWrapper&) = delete;
    npPayloadWrapper& operator=(npPayloadWrapper&&) = delete;

private:
    std::unique_ptr<char[]> payload;
    uintptr_t id;
};

class StorageContainer
{
public:
    std::tuple<uintptr_t, uint32_t>
    addElement(std::unique_ptr<npPayloadWrapper> iPayload)
    {
        std::lock_guard<std::mutex> lock(storageMtx);

        const NamepPipe::npObjectHeader& aHeader = *iPayload;

        auto it = storage[aHeader.type].insert(std::move(iPayload));

        return std::make_tuple(reinterpret_cast<uintptr_t>(it.first->get()), aHeader.type);
    }

    npPayloadWrapper const * const getElement(uint32_t type, uintptr_t iKey) const;

private:
    mutable std::mutex storageMtx;

    // Todo change raw PTR to a more safe type
    std::unordered_map <uint32_t,
        std::unordered_set<std::unique_ptr<npPayloadWrapper> >
    > storage;
};