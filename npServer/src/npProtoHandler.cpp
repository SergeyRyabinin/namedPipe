#include "../include/npProtoHandler.h"

npProtoHandler::Return_t
npProtoHandler::handlePacket(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload)
{
    if (NamepPipe::Request != iHeader->type)
    {
        std::cout << "Unexpected message type!";
        return Return_t();
    }

    switch (iHeader->function)
    {
    case NamepPipe::Reply:
        std::cout << "Unexpected function type!";
        return Return_t();
        break;

        //Not implemented
    case NamepPipe::Connect:
        std::cout << "Unexpected function type!";
        return Return_t();
        break;

    case NamepPipe::Set:
        std::cout << "Set!";
        return set(std::move(iHeader), std::move(iPayload));
        break;

    case NamepPipe::Get:
        std::cout << "Get!";
        return get(std::move(iHeader), std::move(iPayload));
        break;

    case NamepPipe::Disconnect:
        std::cout << "Bye!";
        return bye(std::move(iHeader), std::move(iPayload));
        break;
    }

    std::unique_ptr<Proto_t> aReplyProto = std::make_unique<Proto_t>();
    aReplyProto->type = NamepPipe::Response;
    aReplyProto->status = NamepPipe::ERR;
    aReplyProto->function = NamepPipe::Reply;
    return std::make_tuple(NamepPipe::OK, std::move(aReplyProto), std::unique_ptr<Payload_t>());
}

npProtoHandler::Return_t npProtoHandler::set(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload)
{
    if (!iPayload)
        throw(std::runtime_error("Payload is missing!"));

    std::unique_ptr<npPayloadWrapper> aPayloadWrap = std::make_unique<npPayloadWrapper>(std::move(iPayload));
    uintptr_t id;
    uint32_t type;
    std::tie(id, type) = storage.addElement(std::move(aPayloadWrap));

    std::unique_ptr<Proto_t> aReplyProto = std::make_unique<Proto_t>();
    aReplyProto->type = NamepPipe::Response;
    aReplyProto->status = NamepPipe::OK;
    aReplyProto->function = NamepPipe::Reply;
    aReplyProto->payload_size = sizeof(NamepPipe::npId);

    std::unique_ptr<Payload_t> aReplyPayload = std::make_unique<Payload_t>(sizeof(NamepPipe::npId));
    NamepPipe::npId* aReplyId = reinterpret_cast<NamepPipe::npId*>(aReplyPayload.get());

    aReplyId->header = NamepPipe::npIdHeader;
    aReplyId->id = id;
    aReplyId->type = type;

    return std::make_tuple(NamepPipe::OK, std::move(aReplyProto), std::move(aReplyPayload));
}

npProtoHandler::Return_t npProtoHandler::bye(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload)
{
    std::unique_ptr<Proto_t> aReplyProto = std::make_unique<Proto_t>();
    aReplyProto->type = NamepPipe::Response;
    aReplyProto->status = NamepPipe::OK;
    aReplyProto->function = NamepPipe::Reply;
    aReplyProto->payload_size = 0;

    return std::make_tuple(NamepPipe::OK, std::move(aReplyProto), std::unique_ptr<Payload_t>());
}

npProtoHandler::Return_t npProtoHandler::get(std::unique_ptr<NamepPipe::npProto> iHeader, std::unique_ptr<char[]> iPayload)
{
    if (!iPayload)
        throw(std::runtime_error("Payload is missing!"));

    std::unique_ptr<npPayloadWrapper> aPayloadWrap = std::make_unique<npPayloadWrapper>(std::move(iPayload));

    auto aId = aPayloadWrap->as_T<NamepPipe::npId>();

    npPayloadWrapper const * const aFound = storage.getElement(aId->type, aId->id);
    if (aFound && aFound->as_T<void*>())
    {
        std::unique_ptr<Proto_t> aReplyProto = std::make_unique<Proto_t>();
        aReplyProto->type = NamepPipe::Response;
        aReplyProto->status = NamepPipe::OK;
        aReplyProto->function = NamepPipe::Reply;
        aReplyProto->payload_size = aFound->size;

        std::unique_ptr<Payload_t> aReplyPayload = std::make_unique<Payload_t>(aReplyProto->payload_size);

        memcpy_s(aReplyPayload.get(), aReplyProto->payload_size, aFound->as_T<void*>(), aFound->size);

        return std::make_tuple(NamepPipe::OK, std::move(aReplyProto), std::move(aReplyPayload));
    }

    if (aFound && !aFound->as_T<void*>())
    {
        std::cerr << " Internal error! Stored object is null\n";
    }

    std::cerr << "Requested object type " << aId->type << " with id " << aId->id << " was not found in the container!\n";

    std::unique_ptr<Proto_t> aReplyProto = std::make_unique<Proto_t>();
    aReplyProto->type = NamepPipe::Response;
    aReplyProto->status = NamepPipe::ERR;
    aReplyProto->function = NamepPipe::Reply;
    aReplyProto->payload_size = 0;

    return std::make_tuple(NamepPipe::OK, std::move(aReplyProto), std::unique_ptr<Payload_t>());
}