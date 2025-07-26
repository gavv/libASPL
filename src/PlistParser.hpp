// Copyright (c) libASPL authors
// Licensed under MIT

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aspl {

class PlistValue;

// Plist file parser.
// Parses XML or binary plist files and provides access to parsed fields as STL types.
// Supports a subset of possible plist types: strings, numbers, booleans, arrays, dicts.
class PlistParser
{
public:
    PlistParser();
    ~PlistParser();

    PlistParser(const PlistParser&) = delete;
    PlistParser& operator=(const PlistParser&) = delete;

    // Open plist file.
    bool Open(const std::string& path);

    // Close plist file.
    void Close();

    // Get value by key.
    std::optional<PlistValue> GetValue(const std::string& key) const;

private:
    std::optional<std::map<std::string, PlistValue>> rootDict_;
};

// Plist value.
// Forms hierarchy that mirrors original XML.
class PlistValue
{
public:
    ~PlistValue();

    PlistValue(const PlistValue& other);
    PlistValue& operator=(const PlistValue& other);

    PlistValue(PlistValue&& other) noexcept;
    PlistValue& operator=(PlistValue&& other) noexcept;

    // Get string value.
    std::optional<std::string> AsString() const;

    // Get integer value.
    std::optional<SInt64> AsInteger() const;

    // Get real value.
    std::optional<Float64> AsReal() const;

    // Get boolean value.
    std::optional<Boolean> AsBoolean() const;

    // Get array value.
    std::optional<std::vector<PlistValue>> AsArray() const;

    // Get dictionary value.
    std::optional<std::map<std::string, PlistValue>> AsDict() const;

private:
    friend class PlistParser;

    // Constructor calls CFRetain on value.
    explicit PlistValue(CFTypeRef value);

    CFTypeRef cfValue_ = nullptr;
};

} // namespace aspl
