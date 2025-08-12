// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/DriverRequestHandler.hpp
//! @brief Handler for HAL requests to driver.

#pragma once

namespace aspl {

//! Handler for HAL requests to driver.
//!
//! You may subclass this class if you want to implement custom handling.
class DriverRequestHandler
{
public:
    DriverRequestHandler() = default;

    DriverRequestHandler(const DriverRequestHandler&) = delete;
    DriverRequestHandler& operator=(const DriverRequestHandler&) = delete;

    virtual ~DriverRequestHandler() = default;

    //! Invoked when HAL asynchronously initializes driver, at some point after
    //! driver is loaded and created.
    virtual OSStatus OnInitialize()
    {
        return kAudioHardwareNoError;
    }
};

} // namespace aspl
