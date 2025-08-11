// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Driver.hpp>

#include "Bridge.hpp"
#include "Variant.hpp"

#include <stddef.h>
#include <syslog.h>

namespace aspl {

namespace {

static constexpr UInt64 MagicCookie = 0xDECAFC0FFEEBAD11ull;

} // namespace

Driver::Driver(std::shared_ptr<Context> context,
    std::shared_ptr<Plugin> plugin,
    std::shared_ptr<Storage> storage)
    : context_(context ? std::move(context) : std::make_shared<Context>())
    , plugin_(plugin ? std::move(plugin) : std::make_shared<Plugin>(context_))
    , storage_(storage ? std::move(storage) : std::make_shared<Storage>(context_))
{
    GetContext()->Tracer->Message("Driver::Driver()");

    cookie_ = MagicCookie;

    driverInterfacePointer_ = &driverInterface_;
    driverInterface_ = {
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
    GetContext()->Tracer->Message("Driver::~Driver()");

    cookie_ = 0;
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
    return driverInterface_;
}

AudioServerPlugInDriverRef Driver::GetReference()
{
    return &driverInterfacePointer_;
}

Driver* Driver::GetDriver(AudioServerPlugInDriverRef driverRef)
{
    if (!driverRef) {
        return nullptr;
    }

    AudioServerPlugInDriverInterface* driverInterface = *driverRef;

    if (!driverInterface) {
        return nullptr;
    }

    Driver* driver = reinterpret_cast<Driver*>(
        reinterpret_cast<UInt8*>(driverInterface) - offsetof(Driver, driverInterface_));

    if (driver->cookie_ != MagicCookie) {
        Tracer::UnboundBug("Driver: Attempt to dereference invalid or deleted driver");
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
    const auto driver =
        Driver::GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(driverRef));

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

    const auto counter = ++driver->refCounter_;

    driver->GetContext()->Tracer->Message("Driver::QueryInterface() S_OK refCounter=%lu",
        static_cast<unsigned long>(counter));

    return S_OK;
}

ULONG Driver::AddRef(void* driverRef)
{
    const auto driver =
        Driver::GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(driverRef));

    if (!driver) {
        return 0;
    }

    const auto counter = ++driver->refCounter_;

    driver->GetContext()->Tracer->Message(
        "Driver::AddRef() refCounter=%lu", static_cast<unsigned long>(counter));

    return counter;
}

ULONG Driver::Release(void* driverRef)
{
    const auto driver =
        Driver::GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(driverRef));

    if (!driver) {
        return 0;
    }

    const auto counter = --driver->refCounter_;

    driver->GetContext()->Tracer->Message(
        "Driver::Release() refCounter=%lu", static_cast<unsigned long>(counter));

    if (static_cast<long>(counter) < 0) {
        driver->GetContext()->Tracer->Bug("Driver: Unpaired AddRef/Release");
    }

    return counter;
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

    // Some hints for troubleshooting initialization problems.
    if (status == kAudioHardwareNoError) {
        if (!newHost) {
            // Probably badly implemented InitializeImpl().
            driver->GetContext()->Tracer->Message(
                "Driver::InitializeImpl() did not set Context::Host");
        }
        if (oldHost && oldHost != newHost) {
            // Probably badly implemented plugin host.
            driver->GetContext()->Tracer->Message(
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
