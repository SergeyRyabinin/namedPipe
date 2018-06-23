#include "../include/npStorageContainer.h"

#include <algorithm>

npPayloadWrapper::npPayloadWrapper(const NamepPipe::npObjectHeader& iHeader, const char* iPayload)
    : NamepPipe::npObjectHeader(iHeader)
{
    payload = std::make_unique<char[]>(iHeader.size);

    if (iPayload)
    {
        memcpy_s(payload.get(), this->size, iPayload, iHeader.size);
    }
    else
    {
        memcpy_s(payload.get(), this->size, &iHeader, sizeof(iHeader));
        memset(payload.get() + sizeof(iHeader), 0, this->size - sizeof(iHeader));
    }
    id = reinterpret_cast<uintptr_t>(payload.get());
}

npPayloadWrapper::npPayloadWrapper(const NamepPipe::npObjectHeader& iHeader, std::unique_ptr<char[]> iPayload)
    : NamepPipe::npObjectHeader(iHeader),
    payload(std::move(iPayload)),
    id(reinterpret_cast<uintptr_t>(payload.get()))
{}

npPayloadWrapper::npPayloadWrapper(std::unique_ptr<char[]> iPayload)
    : payload(std::move(iPayload)),
    id(reinterpret_cast<uintptr_t>(payload.get()))
{
    if (payload)
    {
        NamepPipe::npObjectHeader const * const iHeader = reinterpret_cast<const NamepPipe::npObjectHeader*>(payload.get());
        *(reinterpret_cast<npObjectHeader*>(this)) = *iHeader;
    }
}

npPayloadWrapper const * const StorageContainer::getElement(uint32_t type, uintptr_t iKey) const
{
    std::lock_guard<std::mutex> lock(storageMtx);

    if (storage.find(type) != storage.end())
    {
        /*
        auto it = std::find_if(storage.at(type).cbegin(), storage.at(type).cend(), [&iKey](const npPayloadWrapper& aEl)
        {
            return aEl.getId() == iKey;
        });
        */

        auto it = std::find_if(storage.at(type).cbegin(), storage.at(type).cend(),
            [&iKey](const std::unique_ptr<npPayloadWrapper>& aEl)
            {
                return reinterpret_cast<uintptr_t>(aEl.get()) == iKey;
            }
        );

        if (it != storage.at(type).end())
        {
            return it->get();
        }
    }

    return nullptr;
}