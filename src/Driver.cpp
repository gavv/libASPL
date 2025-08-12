// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Driver.hpp>

#include "Bridge.hpp"
#include "Variant.hpp"

#include <stddef.h>
#include <syslog.h>

namespace aspl {

namespace {

static constexpr UInt64 LiveCookie = 0xFEEDDECAFC0FFEEuLL;
static constexpr UInt64 DeadCookie = 0xDEFACEDDEADF00DuLL;

} // namespace

struct Driver::ControlBlock
{
    // First field is pointer to vtbl.
    // This makes ControlBlock a valid COM object.
    // Pointer to ControlBlock can be casted to AudioServerPlugInDriverRef, which is
    // pointer to pointer to vtbl.
    AudioServerPlugInDriverInterface* vtblPtr = nullptr;

    // We allow user to mutate vtable, so it's not global and not constant.
    // Instead, every instance embeds a copy of vtable.
    AudioServerPlugInDriverInterface vtbl;

    std::atomic<ULONG> refCount = 0;
    std::atomic<UInt64> cookie = 0;

    std::weak_ptr<Driver> driver;
};

Driver::Driver(std::shared_ptr<Context> context,
    std::shared_ptr<Plugin> plugin,
    std::shared_ptr<Storage> storage)
    : context_(context ? std::move(context) : std::make_shared<Context>())
    , plugin_(plugin ? std::move(plugin) : std::make_shared<Plugin>(context_))
    , storage_(storage ? std::move(storage) : std::make_shared<Storage>(context_))
    , dcb_(new ControlBlock)
{
    GetContext()->Tracer->Message("Driver::Driver() dcb=%p", dcb_);

    dcb_->refCount = 1;
    dcb_->cookie = LiveCookie;

    dcb_->vtblPtr = &dcb_->vtbl;
    dcb_->vtbl = {
        // Reserved
        nullptr,

        // Service methods, handled by driver itself
        Driver::QueryInterface,
        Driver::AddRef,
        Driver::Release,
        Driver::InitializeJumper,
        Driver::CreateDeviceJumper,
        Driver::DestroyDeviceJumper,

        // Methods redirected to objects found in Context::Dispatcher
        Bridge::AddClient,
        Bridge::RemoveClient,
        Bridge::PerformConfigurationChange,
        Bridge::AbortConfigurationChange,
        Bridge::HasProperty,
        Bridge::IsPropertySettable,
        Bridge::GetPropertyDataSize,
        Bridge::GetPropertyData,
        Bridge::SetPropertyData,
        Bridge::StartIO,
        Bridge::StopIO,
        Bridge::GetZeroTimeStamp,
        Bridge::WillDoIOOperation,
        Bridge::BeginIOOperation,
        Bridge::DoIOOperation,
        Bridge::EndIOOperation,
    };
}

Driver::~Driver()
{
    GetContext()->Tracer->Message(
        "Driver::~Driver() dcb=%p refCount=%ld", dcb_, static_cast<long>(dcb_->refCount));

    // Normally we delete control block here.
    // Control block may outlive driver only if user deletes aspl::Driver too early, while
    // the HAL still retains a reference to the control block via
    // AudioServerPlugInDriverRef. We could blindly delete control block here, but we want
    // to handle this bug a bit more gracefully and properly report errors without UB,
    // so we allow a dangling AudioServerPlugInDriverRef to extend control block lifetime.
    if (--dcb_->refCount == 0) {
        GetContext()->Tracer->Message("Driver::~Driver() deleting dcb");
        dcb_->cookie = DeadCookie;
        delete dcb_;
    } else {
        GetContext()->Tracer->Bug(
            "Driver::~Driver() called too early:"
            " HAL still uses AudioServerPlugInDriverRef");
    }
}

std::shared_ptr<const Context> Driver::GetContext() const
{
    return context_;
}

std::shared_ptr<Context> Driver::GetMutableContext()
{
    return context_;
}

std::shared_ptr<Plugin> Driver::GetPlugin() const
{
    return plugin_;
}

std::shared_ptr<Storage> Driver::GetStorage() const
{
    return storage_;
}

const AudioServerPlugInDriverInterface& Driver::GetPluginInterface() const
{
    return **const_cast<Driver*>(this)->GetReference();
}

AudioServerPlugInDriverInterface& Driver::GetMutablePluginInterface()
{
    return **GetReference();
}

AudioServerPlugInDriverRef Driver::GetReference()
{
    // Lazy-initialize back-reference DCB -> Driver when DCB is accessed first time.
    // We can't do it in Driver ctor because shared_from_this() is not allowed there.
    if (dcb_->driver.expired()) {
        try {
            dcb_->driver = shared_from_this();
        }
        catch (const std::bad_weak_ptr&) {
            GetContext()->Tracer->Bug("Driver: Instance not managed by shared_ptr");
        }
    }

    // AudioServerPlugInDriverRef is pointer to pointer to vtbl.
    // dcb_ is pointer to struct which first field is pointer to vtbl.
    return reinterpret_cast<AudioServerPlugInDriverRef>(dcb_);
}

Driver::ControlBlock* Driver::GetControlBlock(AudioServerPlugInDriverRef driverRef)
{
    ControlBlock* dcb = reinterpret_cast<ControlBlock*>(driverRef);
    if (!dcb) {
        return nullptr;
    }

    if (const auto cookie = dcb->cookie.load(); cookie != LiveCookie) {
        // This is not a valid control block pointer, or control block was
        // already deleted after its ref counter became zero.
        //
        // (In the 1st case cookie is garbage; in the 2nd case it's either DeadCookie
        // or garbage, depending on whether the memory was already reused).
        //
        // Normally this can't happen unless the user had called AddRef/Release directly
        // via AudioServerPlugInDriverRef and messed things up.
        //
        // This might cause a crash, but since it hasn't happened yet, let's continue
        // to pretend that we can handle it gracefully.
        Tracer::UnboundBug(
            "Driver: Attempt to dereference corrupted or Release()ed "
            " AudioServerPlugInDriverRef: dcb=%p cookie=0x%016llX",
            dcb,
            static_cast<unsigned long long>(cookie));
        return nullptr;
    }

    return dcb;
}

std::shared_ptr<Driver> Driver::GetDriver(AudioServerPlugInDriverRef driverRef)
{
    ControlBlock* dcb = GetControlBlock(driverRef);
    if (!dcb) {
        return nullptr;
    }

    auto driver = dcb->driver.lock();
    if (!driver) {
        // Back-reference DCB -> Driver expired.
        //
        // This happens if the user deletes aspl::Driver while HAL still retains a
        // reference to AudioServerPlugInDriverRef and tries to use it.
        //
        // Typically this indicates a bug in user code - the user is responsible for
        // keeping driver alive until driver bundle is unloaded.
        //
        // This bug will not cause a crash. All driver operations invoked from HAL
        // use GetDriver(), so they will report errors to HAL until it calls Release()
        // to delete dangling control block.
        Tracer::UnboundBug(
            "Driver: Attempt to access deleted driver via dangling"
            " AudioServerPlugInDriverRef: dcb=%p refCount=%ld",
            dcb,
            static_cast<long>(dcb->refCount));
        return nullptr;
    }

    return driver;
}

void Driver::SetDriverHandler(std::shared_ptr<DriverRequestHandler> handler)
{
    driverHandler_.WriteValue(handler);
}

void Driver::SetDriverHandler(DriverRequestHandler* handler)
{
    driverHandler_.WriteValue(handler);
}

OSStatus Driver::InitializeImpl(AudioServerPlugInHostRef hostRef)
{
    GetContext()->Tracer->Message("Driver::InitializeImpl() hostRef=%p", hostRef);

    GetMutableContext()->Host = hostRef;

    const auto handlerVariant = driverHandler_.ReadValue();
    const auto handler = GetVariantPtr(handlerVariant);

    if (handler) {
        GetContext()->Tracer->Message("DriverRequestHandler::OnInitialize()");
        return handler->OnInitialize();
    }

    return kAudioHardwareNoError;
}

OSStatus Driver::CreateDeviceImpl(CFDictionaryRef description,
    const AudioServerPlugInClientInfo* clientInfo,
    AudioObjectID* outDeviceObjectID)
{
    GetContext()->Tracer->Message(
        "Driver::CreateDeviceImpl() kAudioHardwareUnsupportedOperationError");

    return kAudioHardwareUnsupportedOperationError;
}

OSStatus Driver::DestroyDeviceImpl(AudioObjectID objectID)
{
    GetContext()->Tracer->Message(
        "Driver::DestroyDeviceImpl() kAudioHardwareUnsupportedOperationError");

    return kAudioHardwareUnsupportedOperationError;
}

HRESULT Driver::QueryInterface(void* driverRef, REFIID iid, LPVOID* outInterface)
{
    ControlBlock* dcb =
        Driver::GetControlBlock(reinterpret_cast<AudioServerPlugInDriverRef>(driverRef));
    if (!dcb) {
        // Bad pointer or use-after-free.
        return E_POINTER;
    }

    const auto driver = dcb->driver.lock();
    if (!driver) {
        Tracer::UnboundBug(
            "Driver: Attempt to call QueryInterface() on deleted driver via dangling"
            " AudioServerPlugInDriverRef: dcb=%p refCount=%ld",
            dcb,
            static_cast<long>(dcb->refCount));
        return E_POINTER;
    }

    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid);

    const bool isSupportedInterface =
        CFEqual(interfaceID, IUnknownUUID) ||
        CFEqual(interfaceID, kAudioServerPlugInDriverInterfaceUUID);

    CFRelease(interfaceID);

    if (!isSupportedInterface) {
        driver->GetContext()->Tracer->Message("Driver::QueryInterface() E_NOINTERFACE");
        return E_NOINTERFACE;
    }

    *outInterface = driver->GetReference();

    const auto counter = ++driver->dcb_->refCount;

    driver->GetContext()->Tracer->Message(
        "Driver::QueryInterface() S_OK dcb=%p refCount=%ld",
        driver->dcb_,
        static_cast<long>(counter));

    return S_OK;
}

ULONG Driver::AddRef(void* driverRef)
{
    ControlBlock* dcb =
        Driver::GetControlBlock(reinterpret_cast<AudioServerPlugInDriverRef>(driverRef));
    if (!dcb) {
        // Bad pointer or use-after-free.
        return 0;
    }

    const auto newCounter = ++dcb->refCount;
    const auto driver = dcb->driver.lock();

    if (static_cast<long>(newCounter) > 1) {
        if (driver) {
            // Normal scenario.
            driver->GetContext()->Tracer->Message("Driver::AddRef() dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        } else {
            // Driver deleted, control block is dangling.
            // Ref counting silently works, other operations will loudly fail.
        }
    } else {
        // If counter was not at least 1 before it was incremented, someone is
        // using AudioServerPlugInDriverRef without properly owning it, or
        // there were unpaired calls.
        if (driver) {
            driver->GetContext()->Tracer->Bug(
                "Driver: Unpaired or unowned call(s) to AddRef/Release:"
                " dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        } else {
            Tracer::UnboundBug(
                "Driver: Unpaired or unowned call(s) to AddRef/Release:"
                " dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        }
    }

    return newCounter;
}

ULONG Driver::Release(void* driverRef)
{
    ControlBlock* dcb =
        Driver::GetControlBlock(reinterpret_cast<AudioServerPlugInDriverRef>(driverRef));
    if (!dcb) {
        // Bad pointer or use-after-free.
        return 0;
    }

    const auto newCounter = --dcb->refCount;
    const auto driver = dcb->driver.lock();

    if (static_cast<long>(newCounter) > 0) {
        if (driver) {
            // Normal scenario.
            driver->GetContext()->Tracer->Message("Driver::Release() dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        } else {
            // Driver deleted, control block is dangling.
            // Ref counting silently works, other operations will loudly fail.
        }
    } else if (newCounter == 0) {
        if (driver) {
            // This can't happen normally because driver holds a reference to
            // the control block. If driver still exists but ref count is zero,
            // calls to AddRef/Release were messed up.
            driver->GetContext()->Tracer->Bug(
                "Driver: Unpaired or unowned call(s) to AddRef/Release:"
                " dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        } else {
            // Driver deleted, control block is dangling.
            // Ref counting silently works, other operations will loudly fail.
            dcb->cookie = DeadCookie;
            delete dcb;
        }
    } else {
        // Negative / wrapped counter.
        if (driver) {
            driver->GetContext()->Tracer->Bug(
                "Driver: Unpaired or unowned call(s) to AddRef/Release:"
                " dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        } else {
            Tracer::UnboundBug(
                "Driver: Unpaired or unowned call(s) to AddRef/Release:"
                " dcb=%p refCount=%ld",
                dcb,
                static_cast<long>(newCounter));
        }
    }

    return newCounter;
}

OSStatus Driver::InitializeJumper(AudioServerPlugInDriverRef driverRef,
    AudioServerPlugInHostRef hostRef)
{
    const auto driver = Driver::GetDriver(driverRef);
    if (!driver) {
        return kAudioHardwareUnspecifiedError;
    }

    auto oldHost = driver->GetContext()->Host.load();

    const OSStatus status = driver->InitializeImpl(hostRef);

    auto newHost = driver->GetContext()->Host.load();

    // Give user some hints for troubleshooting initialization problems.
    if (status == kAudioHardwareNoError) {
        if (!newHost) {
            // Probably badly implemented InitializeImpl().
            driver->GetContext()->Tracer->Bug(
                "Driver::InitializeImpl() did not set Context::Host");
        }
        if (oldHost && oldHost != newHost) {
            // Probably badly implemented plugin host.
            // Normally Context.Host should be set exactly once.
            driver->GetContext()->Tracer->Bug(
                "Driver::InitializeImpl() overwrote Context::Host from %p to %p",
                oldHost,
                newHost);
        }
    }

    return status;
}

OSStatus Driver::CreateDeviceJumper(AudioServerPlugInDriverRef driverRef,
    CFDictionaryRef description,
    const AudioServerPlugInClientInfo* clientInfo,
    AudioObjectID* outDeviceObjectID)
{
    const auto driver = Driver::GetDriver(driverRef);
    if (!driver) {
        return kAudioHardwareUnspecifiedError;
    }

    return driver->CreateDeviceImpl(description, clientInfo, outDeviceObjectID);
}

OSStatus Driver::DestroyDeviceJumper(AudioServerPlugInDriverRef driverRef,
    AudioObjectID objectID)
{
    const auto driver = Driver::GetDriver(driverRef);
    if (!driver) {
        return kAudioHardwareUnspecifiedError;
    }

    return driver->DestroyDeviceImpl(objectID);
}

} // namespace aspl
