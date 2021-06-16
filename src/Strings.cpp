// Copyright (c) libASPL authors
// Licensed under MIT

#include "Strings.hpp"

#include <sstream>

namespace aspl {

std::string CodeToString(UInt32 value)
{
    const char* bytes = reinterpret_cast<const char*>(&value);

    std::stringstream ss;

    ss << "'" << bytes[0] << bytes[1] << bytes[2] << bytes[3] << "'";
    ss << " (0x" << std::hex << value << ")";

    return ss.str();
}

} // namespace aspl
