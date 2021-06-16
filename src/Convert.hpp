// Copyright (c) libASPL authors
// Licensed under MIT

#pragma once

#include "Traits.hpp"

#include <CoreAudio/AudioServerPlugIn.h>

#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace aspl {

class Convert
{
public:
    template <typename T>
    static std::string ToString(const T& value)
    {
        if constexpr (std::is_convertible<T, std::string>::value) {
            return value;
        }

        if constexpr (std::is_arithmetic<T>::value) {
            return std::to_string(value);
        }

        if constexpr (std::is_enum<T>::value) {
            return std::to_string(static_cast<typename std::underlying_type_t<T>>(value));
        }

        if constexpr (std::is_array<T>::value || is_vector<T>::value) {
            return ArrayToString(value);
        }

        if constexpr (std::is_same<T, AudioStreamBasicDescription>::value) {
            return FormatToString(value);
        }

        return "...";
    }

    template <typename T,
        typename = typename std::enable_if_t<std::is_trivially_copyable<T>::value, void>>
    static void ToFoundation(const T& value, T& result)
    {
        result = value;
    }

    template <typename T,
        typename = typename std::enable_if_t<std::is_trivially_copyable<T>::value, void>>
    static void FromFoundation(const T& value, T& result)
    {
        result = value;
    }

    template <typename T,
        typename = typename std::enable_if_t<std::is_enum<T>::value, void>>
    static void ToFoundation(T value, typename std::underlying_type_t<T>& result)
    {
        result = static_cast<typename std::underlying_type_t<T>>(value);
    }

    template <typename T,
        typename = typename std::enable_if_t<std::is_enum<T>::value, void>>
    static void FromFoundation(typename std::underlying_type_t<T> value, T& result)
    {
        result = static_cast<T>(value);
    }

    static void ToFoundation(bool value, UInt32& result)
    {
        result = (value ? 1 : 0);
    }

    static void FromFoundation(UInt32 value, bool& result)
    {
        result = (value != 0);
    }

    static void ToFoundation(const std::string& value, CFStringRef& result);
    static void FromFoundation(CFStringRef value, std::string& result);

    static void ToFoundation(const std::string& value, CFURLRef& result);
    static void FromFoundation(CFURLRef value, std::string& result);

private:
    static std::string FormatToString(const AudioStreamBasicDescription& format);

    template <typename T>
    static std::string ArrayToString(const T& array)
    {
        std::ostringstream ss;

        std::string sep = "[";
        for (const auto& elem : array) {
            ss << sep << ToString(elem);
            sep = ", ";
        }
        ss << "]";

        return ss.str();
    }
};

} // namespace aspl
