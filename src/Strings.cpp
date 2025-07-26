// Copyright (c) libASPL authors
// Licensed under MIT

#include "Strings.hpp"

#include <CoreFoundation/CFPlugInCOM.h>

#include <sstream>

namespace aspl {

std::string CodeToString(UInt32 value)
{
    const char* bytes = reinterpret_cast<const char*>(&value);

    std::stringstream ss;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ss << "'" << bytes[0] << bytes[1] << bytes[2] << bytes[3] << "'";
#else
    ss << "'" << bytes[3] << bytes[2] << bytes[1] << bytes[0] << "'";
#endif

    ss << " (0x" << std::hex << value << ")";

    return ss.str();
}

std::string HresultToString(HRESULT hresult)
{
    std::stringstream ss;

    switch (hresult) {
    case S_OK:
        ss << "S_OK";
        break;
    case E_NOINTERFACE:
        ss << "E_NOINTERFACE";
        break;
    case E_POINTER:
        ss << "E_POINTER";
        break;
    case E_INVALIDARG:
        ss << "E_INVALIDARG";
        break;
    case E_OUTOFMEMORY:
        ss << "E_OUTOFMEMORY";
        break;
    case E_UNEXPECTED:
        ss << "E_UNEXPECTED";
        break;
    case E_FAIL:
        ss << "E_FAIL";
        break;
    default:
        ss << "<unknown error>";
        break;
    }

    ss << " (0x" << std::hex << UInt32(hresult) << ")";

    return ss.str();
}

} // namespace aspl
