// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/Tracer.hpp
//! @brief Operation tracer.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

#include <pthread.h>
#include <unistd.h>

namespace aspl {

//! Operation tracer.
//!
//! All objects use tracer to report operations issues by HAL. By default,
//! tracers sends messages to syslog with "[aspl]" prefix.
//!
//! Tracer maintains per-thread operations depth counter. Nested operations
//! are indented in output messages.
//!
//! If you want to change tracer output, you can use one of the predefined
//! modes or override Print() method. All other methods use it.
//!
//! If you want to change formatting of the operations, you can override
//! other public methods.
class Tracer
{
public:
    //! Tracing mode.
    enum class Mode
    {
        //! No operation.
        //! Don't perform any tracing.
        Noop,

        //! Standard error stream.
        //! Send all messages to stderr.
        Stderr,

        //! System log.
        //! Send all messages to syslog().
        Syslog,
    };

    //! Operation info.
    struct Operation
    {
        //! Operation name.
        const char* Name = nullptr;

        //! ID of object for which operation was initiated.
        AudioObjectID ObjectID = 0;

        //! PID of client which initiated operation.
        pid_t ClientPID = 0;

        //! Address of property.
        //! May be null.
        const AudioObjectPropertyAddress* PropertyAddress = nullptr;

        //! Qualifier size.
        //! May be zero.
        UInt32 QualifierDataSize = 0;

        //! Qualifier data.
        //! May be null.
        const void* QualifierData = nullptr;

        //! Input size.
        //! Non-zero for operations that have input or output data.
        UInt32 InDataSize = 0;

        //! Input data.
        //! Non-null for operations that have input data.
        const void* InData = nullptr;

        //! Output size.
        //! Non-null for operations that have output data.
        const UInt32* OutDataSize = nullptr;

        //! Output data.
        //! Non-null for operations that have output data.
        const void* OutData = nullptr;
    };

    //! Initialize tracer.
    //! Mode defines where to send messages.
    explicit Tracer(Mode mode = Mode::Syslog);

    Tracer(const Tracer&) = delete;
    Tracer& operator=(const Tracer&) = delete;

    virtual ~Tracer() = default;

    //! Called when an operations starts.
    //! Default implementation formats arguments and calls Print().
    virtual void OperationBegin(const Operation& operation);

    //! Called to print message to log.
    //! Default implementation formats arguments and calls Print().
    virtual void Message(const char* format, ...) __attribute__((format(printf, 2, 3)));

    //! Called when an operations completes.
    //! Zero status indicates operation success.
    //! Default implementation formats arguments and calls Print().
    virtual void OperationEnd(const Operation& operation, OSStatus status);

protected:
    //! Print message somewhere.
    //! Default implementation sends message to syslog if mode is Mode::Syslog,
    //! or does nothing if mode is Mode::Noop.
    virtual void Print(const char* message);

private:
    // Returns thread-local operation depth counter
    int& GetDepthCounter();

    const Mode mode_;
    pthread_key_t depthCounter_;
};

} // namespace aspl
