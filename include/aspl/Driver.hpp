// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/Driver.hpp
//! @brief Plugin driver.

#pragma once

#include <aspl/Context.hpp>
#include <aspl/Plugin.hpp>

#include <CoreAudio/AudioServerPlugIn.h>

#include <atomic>
#include <memory>

namespace aspl {

//! Plugin driver.
//!
//! Driver is the top-level object of AudioServer plugin. It contains Plugin
//! object, which is the root of the whole object hierarchy. The driver's job
//! is to receive requests from HAL and redirect them to appropriate objects.
//!
//! Driver is exposed to HAL as "driver reference" (@c AudioServerPlugInDriverRef).
//! The driver reference is actually a pointer to pointer to "plugin driver interface"
//! (@c AudioServerPlugInDriverInterface), which is a struct with a bunch of function
//! pointers for various operations (mostly for property dispatch and I/O).
//!
//! Driver fills this struct with its private functions. Each of them just finds
//! the registered object by ID and redirects operation to it. Objects are searched
//! in Context::Dispatcher. Every object requires Context as constructor argument
//! and automatically registers and unregisters itself in Dispatcher.
//!
//! By default driver creates its own Context and Plugin objects, but you can
//! provide your own if desired.
//!
//! After constructing driver, you need to use GetReference() method to obtain
//! the "driver reference" and return it from the plugin entry point, like:
//!
//! @code
//!   extern "C" void* MyEntryPoint(CFAllocatorRef, CFUUIDRef typeUUID)
//!   {
//!     if (!CFEqual(typeUUID, kAudioServerPlugInTypeUUID)) {
//!       return nullptr;
//!     }
//!     static auto driver = std::make_shared<aspl::Driver>();
//!     return driver->GetReference();
//!   }
//! @endcode
//!
//! Don't forget do declare your entry point in Info.plist of the plugin.
class Driver
{
public:
    //! Construct driver.
    //! If context or plugin are not set, they are created automatically.
    explicit Driver(const std::shared_ptr<Context>& context = {},
        const std::shared_ptr<Plugin>& plugin = {});

    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;

    ~Driver();

    //! Get context.
    std::shared_ptr<const Context> GetContext() const;

    //! Get plugin.
    std::shared_ptr<Plugin> GetPlugin() const;

    //! Get plugin interface.
    //! Plugin interface is a table with function pointers which implement
    //! various plugin operations.
    const AudioServerPlugInDriverInterface& GetPluginInterface() const;

    //! Get driver reference.
    //! This is what should be returned from plugin entry point.
    AudioServerPlugInDriverRef GetReference();

    //! Cast driver reference back to driver.
    static Driver* GetDriver(AudioServerPlugInDriverRef driverRef);

private:
    // COM methods
    static HRESULT QueryInterface(void* driverRef, REFIID iid, LPVOID* outInterface);

    static ULONG AddRef(void* driverRef);
    static ULONG Release(void* driverRef);

    // Driver initialization
    static OSStatus Initialize(AudioServerPlugInDriverRef driverRef,
        AudioServerPlugInHostRef hostRef);

    // Device creation
    static OSStatus CreateDevice(AudioServerPlugInDriverRef driverRef,
        CFDictionaryRef description,
        const AudioServerPlugInClientInfo* clientInfo,
        AudioObjectID* outDeviceObjectID);

    static OSStatus DestroyDevice(AudioServerPlugInDriverRef driverRef,
        AudioObjectID objectID);

    const std::shared_ptr<Context> context_;
    const std::shared_ptr<Plugin> plugin_;

    // Driver method table
    AudioServerPlugInDriverInterface driverInterface_;
    AudioServerPlugInDriverInterface* driverInterfacePointer_;

    // Driver reference counter
    std::atomic<ULONG> refCounter_ = 0;
};

} // namespace aspl
