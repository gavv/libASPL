// Copyright (c) libASPL authors
// Licensed under MIT

#pragma once

#include <aspl/Client.hpp>
#include <aspl/DriverLoader.hpp>
#include <aspl/Tracer.hpp>
#include <aspl/VirtualHost.hpp>

#include <CoreAudio/AudioServerPlugIn.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace aspl {

//! Convenient adapter for AudioServerPlugInDriverRef.
//!
//! AudioServerPlugInDriverRef is the top-level vtable returned by plugin entry point,
//! normally used by audio server. DriverAdapter it with a C++ interface with STL
//! types instead of CoreFoundation types.
//!
//! This may be useful for testing drivers outside of audio server.
//!
//! @note
//!  DriverAdapter is stateless. It is safe to mix calls to driver via adapter
//!  with direct calls via vtable.
//!
//! See also DriverLoader, VirtualHost.
class DriverAdapter
{
public:
    //! Initialize adapter from handle returned from DriverLoader.
    //!
    //! Same as Initialize(driverHandle.GetReference()), but also stores shared_ptr
    //! in DriverAdapter to make sure it lives long enough.
    //!
    //! Does NOT initialize driver, see Initialize().
    explicit DriverAdapter(std::shared_ptr<DriverLoader::Handle> driverHandle,
        std::shared_ptr<Tracer> tracer = {});

    //! Initialize adapter from driver's virtual table.
    //!
    //! Invokes driver's AddRef() in constructor and Release() in destructor.
    //!
    //! Does NOT initialize driver, see Initialize().
    explicit DriverAdapter(AudioServerPlugInDriverRef driverRef,
        std::shared_ptr<Tracer> tracer = {});

    DriverAdapter(const DriverAdapter&) = delete;
    DriverAdapter& operator=(const DriverAdapter&) = delete;

    //! Release driver.
    ~DriverAdapter();

    //! Get driver reference.
    //! Returns reference passed to adapter constructor.
    AudioServerPlugInDriverRef GetReference();

    //! @name Driver initialization
    //! @{

    //! Initialize driver with VirtualHost.
    //!
    //! Same as Initialize(virtHost.GetReference()), but also stores shared_ptr
    //! in DriverAdapter to make sure it lives long enough.
    //!
    //! VirtualHost is a mock implementation of AudioServerPlugInHostRef interface
    //! that can be used for testing.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Driver::InitializeImpl() and
    //!  DriverRequestHandler::OnInitialize().
    OSStatus Initialize(std::shared_ptr<VirtualHost> virtHost);

    //! Initialize driver with raw host reference.
    //!
    //! AudioServerPlugInHostRef must live at least until driver is released.
    //!
    //! AudioServerPlugInHostRef is a vtable with plugin host methods with audio
    //! server normally provides to plugin it loads. Driver requires it to
    //! operate correctly, and until this call, most driver methods will fail.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Driver::InitializeImpl(), which
    //!  calls DriverRequestHandler::OnInitialize().
    OSStatus Initialize(AudioServerPlugInHostRef host);

    //! @}

    //! @name Client management
    //! @{

    //! Register device client.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::AddClient(), which calls
    //!  ControlRequestHandler::OnAddClient().
    OSStatus AddDeviceClient(AudioObjectID deviceObjectID, const ClientInfo& clientInfo);

    //! Unregister device client.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::RemoveClient(), which calls
    //!  ControlRequestHandler::OnRemoveClient().
    OSStatus RemoveDeviceClient(AudioObjectID deviceObjectID,
        const ClientInfo& clientInfo);

    //! @}

    //! @name Configuration requests
    //! @{

    //! Perform device configuration change.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::PerformConfigurationChange().
    //!  Device expects that plugin host will call this method after device asked for it
    //!  via RequestDeviceConfigurationChange().
    OSStatus PerformDeviceConfigurationChange(AudioObjectID deviceObjectID,
        UInt64 changeAction,
        void* changeInfo);

    //! Abort device configuration change.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::AbortConfigurationChange().
    //!  Device expects that plugin host will call this method after device asked for it
    //!  via RequestDeviceConfigurationChange().
    OSStatus AbortDeviceConfigurationChange(AudioObjectID deviceObjectID,
        UInt64 changeAction,
        void* changeInfo);

    //! @}

    //! @name Property operations
    //! @{

    //! Check whether given property is present.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::HasProperty().
    bool HasProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Check whether given property can be set.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::IsPropertySettable().
    std::pair<OSStatus, bool> IsPropertySettable(AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Get scalar property value.
    //! Use for properties that are integers, floats, structs, or fixed-size arrays.
    //!
    //! @pre
    //!  Requested property must be of type T, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::GetPropertyData().
    template <class T>
    std::pair<OSStatus, T> GetScalarProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Get string property value.
    //! Use for CFString properties.
    //!
    //! @pre
    //!  Requested property must be CFStringRef, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::GetPropertyData().
    std::pair<OSStatus, std::string> GetStringProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Get vector property value.
    //! Use for properties that are variable-size arrays of scalars.
    //!
    //! @pre
    //!  Requested property must be vector of T, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::GetPropertyData().
    template <class T>
    std::pair<OSStatus, std::vector<T>> GetVectorProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Get CoreFoundation property value.
    //! Use for CFTypeRef properties like CFStringRef, CFURLRef, CFPropertyListRef.
    //! T must be CFXxxRef type - a pointer to some CFXxx.
    //! Returns shared_ptr<CFXxx>, which will call CFRelease() in deleter.
    //!
    //! @pre
    //!  Requested property must be CFXxxRef, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::GetPropertyData().
    template <class T>
    std::pair<OSStatus, std::shared_ptr<std::remove_pointer_t<T>>> GetCFProperty(
        AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Get property data size.
    //! This size can be used to allocate output buffer when calling GetPropertyData().
    //!
    //! @note
    //!  For scalar properties, returns scalar byte size.
    //!  For vector properties, returns total byte size of the vector.
    //!  For CF properties, including strings, returns size of CFTypeRef,
    //!  i.e. pointer size.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::GetPropertyDataSize().
    std::pair<OSStatus, size_t> GetPropertySize(AudioObjectID objectID,
        pid_t clientProcessID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain,
        const void* qualifier = nullptr,
        size_t qualifierSize = 0) const;

    //! Get property data.
    //! On success, writes property to user-allocated output buffer @p data of size
    //! @p dataSize and updates actual size in @p dataSize.
    //!
    //! @note
    //!  It is recommended to use type-safe wrappers, when possible: GetScalarProperty(),
    //!  GetStringProperty(), GetVectorProperty(), GetCFProperty(). However, if property
    //!  has unusual format or needs qualifier, you may need this method.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::GetPropertyData().
    OSStatus GetPropertyData(AudioObjectID objectID,
        pid_t clientProcessID,
        void* data,
        size_t* dataSize,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain,
        const void* qualifier = nullptr,
        size_t qualifierSize = 0) const;

    //! Set scalar property value.
    //! Use for properties that are integers, floats, structs, or fixed-size arrays.
    //!
    //! @pre
    //!  Property must be of type T, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::SetPropertyData().
    template <class T>
    OSStatus SetScalarProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        const T& value,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Set string property value.
    //! Use for CFString properties.
    //!
    //! @pre
    //!  Property must be CFStringRef, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::SetPropertyData().
    OSStatus SetStringProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        const std::string& value,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Set vector property value.
    //! Use for properties that are variable-size arrays of scalars.
    //!
    //! @pre
    //!  Property must be vector of T, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::SetPropertyData().
    template <class T>
    OSStatus SetVectorProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        const std::vector<T>& value,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Set CoreFoundation property value.
    //! Use for CFTypeRef properties like CFStringRef, CFURLRef, CFPropertyListRef.
    //!
    //! @pre
    //!  Property must be CFXxxRef, otherwise things happen.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::SetPropertyData().
    template <class T>
    OSStatus SetCFProperty(AudioObjectID objectID,
        pid_t clientProcessID,
        T value,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) const;

    //! Set property data.
    //! Writes property data from user-provided input buffer @p data of size @p dataSize.
    //!
    //! @note
    //!  It is recommended to use type-safe wrappers, when possible: SetScalarProperty(),
    //!  SetStringProperty(), SetVectorProperty(), SetCFProperty(). However, if property
    //!  has unusual format or needs qualifier, you'll need this method.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls Object::SetPropertyData().
    OSStatus SetPropertyData(AudioObjectID objectID,
        pid_t clientProcessID,
        const void* data,
        size_t dataSize,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain,
        const void* qualifier = nullptr,
        size_t qualifierSize = 0) const;

    //! @}

    //! @name I/O operations
    //! @{

    //! Start I/O for device.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::StartIO(), which calls
    //!  ControlRequestHandler::OnStartIO().
    OSStatus StartIO(AudioObjectID deviceObjectID, UInt32 clientID);

    //! Stop I/O for device.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::StopIO(), which calls
    //!  ControlRequestHandler::OnStopIO().
    OSStatus StopIO(AudioObjectID deviceObjectID, UInt32 clientID);

    //! Information about most recent zero timestamp.
    struct ZeroTimestamp
    {
        //! Timestamp in sample stream domain.
        Float64 SampleTime = 0;
        //! Timestamp in host time domain.
        UInt64 HostTime = 0;
        //! Changes when zero timestamp moves.
        UInt64 Seed = 0;
    };

    //! Get zero time stamp for device.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::GetZeroTimeStamp().
    std::pair<OSStatus, ZeroTimestamp> GetZeroTimeStamp(AudioObjectID deviceObjectID,
        UInt32 clientID);

    //! I/O cycle information.
    //! Should be provided for every I/O operation.
    //!
    //! @remarks
    //!  This struct is mapped to AudioServerPlugInIOCycleInfo.
    struct IOCycle
    {
        //! Cycle ordinal number.
        //! Starts at 1 and increments for each subsequent cycle.
        UInt64 CycleCounter = 0;

        //! Number of frames in input/output buffer.
        //! E.g. for 2-channel 16-bit audio, 100 frame count corresponds to 400 bytes.
        UInt32 BatchFrameCount = 0;
        //! Nominal number of frames in input/output buffer before drift compensation.
        //! If zero, assumed to be equal to BatchFrameCount.
        UInt32 BatchNominalFrameCount = 0;

        //! The sample time from where in the device's timeline the data starts.
        Float64 BatchSampleTime = 0;
        //! The host time from where in the device's timeline the data starts.
        UInt64 BatchHostTime = 0;

        //! The current sample time in the device's timeline.
        Float64 CurrentSampleTime = 0;
        //! The current host time in the device's timeline.
        UInt64 CurrentHostTime = 0;

        //! Number of host ticks per frame that the host is measuring.
        Float64 HostTicksPerFrame = 0;
    };

    //! Check if device will read client input.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::WillDoIOOperation()
    //!  with kAudioServerPlugInIOOperationReadInput.
    std::tuple<OSStatus, bool, bool> WillReadClientInput(AudioObjectID deviceObjectID,
        UInt32 clientID);

    //! Read client input from device.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::BeginIOOperation(),
    //!  aspl::Device::DoIOOperation(), and aspl::Device::EndIOOperation()
    //!  with kAudioServerPlugInIOOperationReadInput.
    OSStatus ReadClientInput(AudioObjectID deviceObjectID,
        AudioObjectID streamObjectID,
        UInt32 clientID,
        const IOCycle& ioCycle,
        void* bytes,
        UInt32 bytesCount);

    //! Check if device will write client output.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::WillDoIOOperation()
    //!  with kAudioServerPlugInIOOperationMixOutput.
    std::tuple<OSStatus, bool, bool> WillWriteClientOutput(AudioObjectID deviceObjectID,
        UInt32 clientID);

    //! Write client output to device.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::BeginIOOperation(),
    //!  aspl::Device::DoIOOperation(), and aspl::Device::EndIOOperation()
    //!  with kAudioServerPlugInIOOperationMixOutput.
    OSStatus WriteClientOutput(AudioObjectID deviceObjectID,
        AudioObjectID streamObjectID,
        UInt32 clientID,
        const IOCycle& ioCycle,
        const void* bytes,
        UInt32 bytesCount);

    //! Check if device will write mixed output.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::WillDoIOOperation()
    //!  with kAudioServerPlugInIOOperationWriteMix.
    std::tuple<OSStatus, bool, bool> WillWriteMixedOutput(AudioObjectID deviceObjectID,
        UInt32 clientID);

    //! Write mixed output to device.
    //!
    //! @remarks
    //!  In libASPL drivers, this method calls aspl::Device::BeginIOOperation(),
    //!  aspl::Device::DoIOOperation(), and aspl::Device::EndIOOperation()
    //!  with kAudioServerPlugInIOOperationWriteMix.
    OSStatus WriteMixedOutput(AudioObjectID deviceObjectID,
        AudioObjectID streamObjectID,
        UInt32 clientID,
        const IOCycle& ioCycle,
        const void* bytes,
        UInt32 bytesCount);

    //! @}

private:
    static AudioServerPlugInIOCycleInfo MakeCycleInfo(const IOCycle& ioCycle);

    template <typename Func, typename... Args>
    OSStatus InvokeMethod(Func func, Args&&... args) const;

    template <typename Type, typename Func, typename... Args>
    Type InvokeMethodT(Func func, Args&&... args) const;

    std::shared_ptr<Tracer> tracer_;

    std::shared_ptr<DriverLoader::Handle> driverHandle_;
    std::shared_ptr<VirtualHost> virtHost_;

    AudioServerPlugInDriverRef driverRef_ = nullptr;
};

// Template getters

template <class T>
std::pair<OSStatus, T> DriverAdapter::GetScalarProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(!std::is_pointer_v<T>, "T must not be a pointer");

    T data;
    size_t dataSize = sizeof(data);

    OSStatus status = GetPropertyData(
        objectID, clientProcessID, &data, &dataSize, selector, scope, element);

    return std::make_pair(status, data);
}

template <class T>
std::pair<OSStatus, std::vector<T>> DriverAdapter::GetVectorProperty(
    AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(!std::is_pointer_v<T>, "T must not be a pointer");

    auto [status, dataSize] =
        GetPropertySize(objectID, clientProcessID, selector, scope, element);
    if (status != kAudioHardwareNoError || dataSize == 0) {
        return std::make_pair(status, std::vector<T>{});
    }

    std::vector<T> data;
    data.resize(dataSize / sizeof(T));

    status = GetPropertyData(
        objectID, clientProcessID, &data[0], &dataSize, selector, scope, element);
    if (status != kAudioHardwareNoError) {
        return std::make_pair(status, std::vector<T>{});
    }

    data.resize(dataSize / sizeof(T));
    return std::make_pair(status, std::move(data));
}

template <class T>
std::pair<OSStatus, std::shared_ptr<std::remove_pointer_t<T>>>
DriverAdapter::GetCFProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    static_assert(std::is_pointer_v<T>, "T must be some kind of CFTypeRef");

    CFTypeRef data = nullptr;
    size_t dataSize = sizeof(data);

    OSStatus status = GetPropertyData(
        objectID, clientProcessID, &data, &dataSize, selector, scope, element);
    if (status != kAudioHardwareNoError) {
        return std::make_pair(status, std::shared_ptr<std::remove_pointer_t<T>>{});
    }

    auto dataPtr =
        std::shared_ptr<std::remove_pointer_t<T>>(static_cast<T>(data), [](auto ref) {
            if (ref) {
                CFRelease(ref);
            }
        });
    return std::make_pair(status, std::move(dataPtr));
}

// Template setters

template <class T>
OSStatus DriverAdapter::SetScalarProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    const T& value,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(!std::is_pointer_v<T>, "T must not be a pointer");

    return SetPropertyData(
        objectID, clientProcessID, &value, sizeof(value), selector, scope, element);
}

template <class T>
OSStatus DriverAdapter::SetVectorProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    const std::vector<T>& value,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(!std::is_pointer_v<T>, "T must not be a pointer");

    return SetPropertyData(objectID,
        clientProcessID,
        &value[0],
        value.size() * sizeof(T),
        selector,
        scope,
        element);
}

template <class T>
OSStatus DriverAdapter::SetCFProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    T value,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    static_assert(std::is_pointer_v<T>, "T must be some kind of CFTypeRef");

    return SetPropertyData(
        objectID, clientProcessID, &value, sizeof(value), selector, scope, element);
}

} // namespace aspl
