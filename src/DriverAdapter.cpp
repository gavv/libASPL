// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/DriverAdapter.hpp"

#include "Convert.hpp"
#include "Strings.hpp"

namespace aspl {

DriverAdapter::DriverAdapter(std::shared_ptr<DriverLoader::Handle> driverHandle,
    std::shared_ptr<Tracer> tracer)
    : DriverAdapter(driverHandle ? driverHandle->GetReference() : nullptr, tracer)
{
    driverHandle_ = std::move(driverHandle);
}

DriverAdapter::DriverAdapter(AudioServerPlugInDriverRef driverRef,
    std::shared_ptr<Tracer> tracer)
    : tracer_(
          tracer ? std::move(tracer) : std::make_shared<Tracer>(Tracer::Output::Stderr))
    , driverRef_(driverRef)
{
    if (!driverRef_) {
        tracer_->Message("DriverAdapter::DriverAdapter() driverRef is null");
        return;
    }

    if ((*driverRef_)->AddRef == nullptr) {
        tracer_->Message("DriverAdapter::DriverAdapter() AddRef is null");
        return;
    }

    ULONG refCount = (*driverRef_)->AddRef(driverRef_);
    tracer_->Message("DriverAdapter::DriverAdapter() AddRef: refCount=%lu",
        static_cast<unsigned long>(refCount));
}

DriverAdapter::~DriverAdapter()
{
    if (!driverRef_) {
        return;
    }

    if ((*driverRef_)->Release == nullptr) {
        tracer_->Message("DriverAdapter::~DriverAdapter() Release is null");
        return;
    }

    ULONG refCount = (*driverRef_)->Release(driverRef_);
    tracer_->Message("DriverAdapter::~DriverAdapter() Release: refCount=%lu",
        static_cast<unsigned long>(refCount));
}

AudioServerPlugInDriverRef DriverAdapter::GetReference()
{
    return driverRef_;
}

OSStatus DriverAdapter::Initialize(std::shared_ptr<VirtualHost> virtHost)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::Initialize()";

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    if (!virtHost) {
        tracer_->Message("VirtualHost is null");
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    status = InvokeMethod(
        &AudioServerPlugInDriverInterface::Initialize, virtHost->GetReference());

end:
    if (status == kAudioHardwareNoError) {
        virtHost_ = virtHost;
    }

    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::Initialize(AudioServerPlugInHostRef host)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::Initialize()";

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::Initialize, host);

end:
    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::AddDeviceClient(AudioObjectID deviceObjectID,
    const ClientInfo& clientInfo)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::AddDeviceClient()";
    op.ObjectID = deviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    AudioServerPlugInClientInfo rawClientInfo = {};
    CFStringRef bundleID = nullptr;

    rawClientInfo.mClientID = clientInfo.ClientID;
    rawClientInfo.mProcessID = clientInfo.ProcessID;
    rawClientInfo.mIsNativeEndian = clientInfo.IsNativeEndian;

    Convert::ToFoundation(clientInfo.BundleID, bundleID);
    rawClientInfo.mBundleID = bundleID;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::AddDeviceClient,
        deviceObjectID,
        &rawClientInfo);

end:
    if (bundleID) {
        CFRelease(bundleID);
    }

    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::RemoveDeviceClient(AudioObjectID deviceObjectID,
    const ClientInfo& clientInfo)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::RemoveDeviceClient()";
    op.ObjectID = deviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    AudioServerPlugInClientInfo rawClientInfo = {};
    CFStringRef bundleID = nullptr;

    rawClientInfo.mClientID = clientInfo.ClientID;
    rawClientInfo.mProcessID = clientInfo.ProcessID;
    rawClientInfo.mIsNativeEndian = clientInfo.IsNativeEndian;

    Convert::ToFoundation(clientInfo.BundleID, bundleID);
    rawClientInfo.mBundleID = bundleID;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::RemoveDeviceClient,
        deviceObjectID,
        &rawClientInfo);

end:
    if (bundleID) {
        CFRelease(bundleID);
    }

    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::PerformDeviceConfigurationChange(AudioObjectID deviceObjectID,
    UInt64 changeAction,
    void* changeInfo)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::PerformDeviceConfigurationChange()";
    op.ObjectID = deviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    status =
        InvokeMethod(&AudioServerPlugInDriverInterface::PerformDeviceConfigurationChange,
            deviceObjectID,
            changeAction,
            changeInfo);

end:
    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::AbortDeviceConfigurationChange(AudioObjectID deviceObjectID,
    UInt64 changeAction,
    void* changeInfo)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::AbortDeviceConfigurationChange()";
    op.ObjectID = deviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    status =
        InvokeMethod(&AudioServerPlugInDriverInterface::AbortDeviceConfigurationChange,
            deviceObjectID,
            changeAction,
            changeInfo);

end:
    tracer_->OperationEnd(op, status);
    return status;
}

bool DriverAdapter::HasProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;
    address.mElement = element;

    Tracer::Operation op;
    op.Name = "DriverAdapter::HasProperty()";
    op.Flags = Tracer::Flags::Readonly;
    op.ObjectID = objectID;
    op.ClientPID = clientProcessID;
    op.PropertyAddress = &address;

    tracer_->OperationBegin(op);

    Boolean result =
        InvokeMethodT<Boolean>(&AudioServerPlugInDriverInterface::HasProperty,
            objectID,
            clientProcessID,
            &address);

end:
    tracer_->OperationEnd(op, kAudioHardwareNoError);
    return result;
}

std::pair<OSStatus, bool> DriverAdapter::IsPropertySettable(AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;
    address.mElement = element;

    Tracer::Operation op;
    op.Name = "DriverAdapter::IsPropertySettable()";
    op.Flags = Tracer::Flags::Readonly;
    op.ObjectID = objectID;
    op.ClientPID = clientProcessID;
    op.PropertyAddress = &address;

    tracer_->OperationBegin(op);

    Boolean outIsSettable = false;
    OSStatus status = InvokeMethod(&AudioServerPlugInDriverInterface::IsPropertySettable,
        objectID,
        clientProcessID,
        &address,
        &outIsSettable);

end:
    tracer_->OperationEnd(op, status);
    return std::make_pair(status, outIsSettable);
}

std::pair<OSStatus, std::string> DriverAdapter::GetStringProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    CFStringRef data = nullptr;
    size_t dataSize = sizeof(data);

    std::string result;
    OSStatus status = GetPropertyData(
        objectID, clientProcessID, &data, &dataSize, selector, scope, element);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    Convert::FromFoundation(data, result);

end:
    if (data) {
        CFRelease(data);
    }

    return std::make_pair(status, std::move(result));
}

std::pair<OSStatus, size_t> DriverAdapter::GetPropertySize(AudioObjectID objectID,
    pid_t clientProcessID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element,
    const void* qualifier,
    size_t qualifierSize) const
{
    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;
    address.mElement = element;

    Tracer::Operation op;
    op.Name = "DriverAdapter::GetPropertySize()";
    op.Flags = Tracer::Flags::Readonly;
    op.ObjectID = objectID;
    op.ClientPID = clientProcessID;
    op.PropertyAddress = &address;
    op.QualifierDataSize = static_cast<UInt32>(qualifierSize);
    op.QualifierData = qualifier;

    tracer_->OperationBegin(op);

    UInt32 outDataSize = 0;
    OSStatus status = InvokeMethod(&AudioServerPlugInDriverInterface::GetPropertyDataSize,
        objectID,
        clientProcessID,
        &address,
        static_cast<UInt32>(qualifierSize),
        qualifier,
        &outDataSize);

end:
    tracer_->OperationEnd(op, status);
    return std::make_pair(status, static_cast<size_t>(outDataSize));
}

OSStatus DriverAdapter::GetPropertyData(AudioObjectID objectID,
    pid_t clientProcessID,
    void* data,
    size_t* dataSize,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element,
    const void* qualifier,
    size_t qualifierSize) const
{
    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;
    address.mElement = element;

    UInt32 inDataSize = dataSize ? static_cast<UInt32>(*dataSize) : 0;
    UInt32 outDataSize = inDataSize;

    Tracer::Operation op;
    op.Name = "DriverAdapter::GetPropertyData()";
    op.Flags = Tracer::Flags::Readonly;
    op.ObjectID = objectID;
    op.ClientPID = clientProcessID;
    op.PropertyAddress = &address;
    op.QualifierDataSize = static_cast<UInt32>(qualifierSize);
    op.QualifierData = qualifier;
    op.OutDataSize = &outDataSize;
    op.OutData = data;

    tracer_->OperationBegin(op);

    OSStatus status = InvokeMethod(&AudioServerPlugInDriverInterface::GetPropertyData,
        objectID,
        clientProcessID,
        &address,
        static_cast<UInt32>(qualifierSize),
        qualifier,
        inDataSize,
        &outDataSize,
        data);

    if (dataSize) {
        *dataSize = static_cast<size_t>(outDataSize);
    }

end:
    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::SetStringProperty(AudioObjectID objectID,
    pid_t clientProcessID,
    const std::string& value,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) const
{
    CFStringRef data = nullptr;
    Convert::ToFoundation(value, data);

    OSStatus status = SetPropertyData(
        objectID, clientProcessID, &data, sizeof(data), selector, scope, element);

end:
    if (data) {
        CFRelease(data);
    }

    return status;
}

OSStatus DriverAdapter::SetPropertyData(AudioObjectID objectID,
    pid_t clientProcessID,
    const void* data,
    size_t dataSize,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element,
    const void* qualifier,
    size_t qualifierSize) const
{
    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;
    address.mElement = element;

    Tracer::Operation op;
    op.Name = "DriverAdapter::SetPropertyData()";
    op.ObjectID = objectID;
    op.ClientPID = clientProcessID;
    op.PropertyAddress = &address;
    op.QualifierDataSize = static_cast<UInt32>(qualifierSize);
    op.QualifierData = qualifier;
    op.InDataSize = static_cast<UInt32>(dataSize);
    op.InData = data;

    tracer_->OperationBegin(op);

    OSStatus status = InvokeMethod(&AudioServerPlugInDriverInterface::SetPropertyData,
        objectID,
        clientProcessID,
        &address,
        static_cast<UInt32>(qualifierSize),
        qualifier,
        static_cast<UInt32>(dataSize),
        data);

end:
    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::StartIO(AudioObjectID deviceObjectID, UInt32 clientID)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::StartIO()";
    op.ObjectID = deviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    status = InvokeMethod(
        &AudioServerPlugInDriverInterface::StartIO, deviceObjectID, clientID);

end:
    tracer_->OperationEnd(op, status);
    return status;
}

OSStatus DriverAdapter::StopIO(AudioObjectID deviceObjectID, UInt32 clientID)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::StopIO()";
    op.ObjectID = deviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    status =
        InvokeMethod(&AudioServerPlugInDriverInterface::StopIO, deviceObjectID, clientID);

end:
    tracer_->OperationEnd(op, status);
    return status;
}

std::pair<OSStatus, DriverAdapter::ZeroTimestamp> DriverAdapter::GetZeroTimeStamp(
    AudioObjectID deviceObjectID,
    UInt32 clientID)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::GetZeroTimeStamp()";
    op.ObjectID = deviceObjectID;
    op.Flags = (Tracer::Flags::Realtime | Tracer::Flags::Readonly);

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    ZeroTimestamp timestamp;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::GetZeroTimeStamp,
        deviceObjectID,
        clientID,
        &timestamp.SampleTime,
        &timestamp.HostTime,
        &timestamp.Seed);

end:
    tracer_->OperationEnd(op, status);
    return std::make_pair(status, timestamp);
}

std::tuple<OSStatus, bool, bool> DriverAdapter::WillReadClientInput(
    AudioObjectID deviceObjectID,
    UInt32 clientID)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::WillReadClientInput()";
    op.ObjectID = deviceObjectID;
    op.Flags = (Tracer::Flags::Realtime | Tracer::Flags::Readonly);

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    Boolean willDo = false;
    Boolean willDoInPlace = false;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::WillDoIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationReadInput,
        &willDo,
        &willDoInPlace);

end:
    tracer_->OperationEnd(op, status);
    return std::make_tuple(status, willDo, willDoInPlace);
}

OSStatus DriverAdapter::ReadClientInput(AudioObjectID deviceObjectID,
    AudioObjectID streamObjectID,
    UInt32 clientID,
    const IOCycle& ioCycle,
    void* bytes,
    UInt32 bytesCount)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::ReadClientInput()";
    op.ObjectID = deviceObjectID;
    op.Flags = Tracer::Flags::Realtime;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    AudioServerPlugInIOCycleInfo ioCycleInfo = MakeCycleInfo(ioCycle);

    status = InvokeMethod(&AudioServerPlugInDriverInterface::BeginIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationReadInput,
        ioCycle.BatchFrameCount,
        &ioCycleInfo);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    status = InvokeMethod(&AudioServerPlugInDriverInterface::DoIOOperation,
        deviceObjectID,
        streamObjectID,
        clientID,
        kAudioServerPlugInIOOperationReadInput,
        ioCycle.BatchFrameCount,
        &ioCycleInfo,
        bytes,
        nullptr);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    status = InvokeMethod(&AudioServerPlugInDriverInterface::EndIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationReadInput,
        ioCycle.BatchFrameCount,
        &ioCycleInfo);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

end:
    tracer_->OperationEnd(op, status);
    return status;
}

std::tuple<OSStatus, bool, bool> DriverAdapter::WillWriteClientOutput(
    AudioObjectID deviceObjectID,
    UInt32 clientID)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::WillWriteClientOutput()";
    op.ObjectID = deviceObjectID;
    op.Flags = (Tracer::Flags::Realtime | Tracer::Flags::Readonly);

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    Boolean willDo = false;
    Boolean willDoInPlace = false;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::WillDoIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationMixOutput,
        &willDo,
        &willDoInPlace);

end:
    tracer_->OperationEnd(op, status);
    return std::make_tuple(status, willDo, willDoInPlace);
}

OSStatus DriverAdapter::WriteClientOutput(AudioObjectID deviceObjectID,
    AudioObjectID streamObjectID,
    UInt32 clientID,
    const IOCycle& ioCycle,
    const void* bytes,
    UInt32 bytesCount)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::WriteClientOutput()";
    op.ObjectID = deviceObjectID;
    op.Flags = Tracer::Flags::Realtime;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    AudioServerPlugInIOCycleInfo ioCycleInfo = MakeCycleInfo(ioCycle);

    status = InvokeMethod(&AudioServerPlugInDriverInterface::BeginIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationMixOutput,
        ioCycle.BatchFrameCount,
        &ioCycleInfo);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    status = InvokeMethod(&AudioServerPlugInDriverInterface::DoIOOperation,
        deviceObjectID,
        streamObjectID,
        clientID,
        kAudioServerPlugInIOOperationMixOutput,
        ioCycle.BatchFrameCount,
        &ioCycleInfo,
        const_cast<void*>(bytes),
        nullptr);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    status = InvokeMethod(&AudioServerPlugInDriverInterface::EndIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationMixOutput,
        ioCycle.BatchFrameCount,
        &ioCycleInfo);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

end:
    tracer_->OperationEnd(op, status);
    return status;
}

std::tuple<OSStatus, bool, bool> DriverAdapter::WillWriteMixedOutput(
    AudioObjectID deviceObjectID,
    UInt32 clientID)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::WillWriteMixedOutput()";
    op.ObjectID = deviceObjectID;
    op.Flags = (Tracer::Flags::Realtime | Tracer::Flags::Readonly);

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    Boolean willDo = false;
    Boolean willDoInPlace = false;

    status = InvokeMethod(&AudioServerPlugInDriverInterface::WillDoIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationWriteMix,
        &willDo,
        &willDoInPlace);

end:
    tracer_->OperationEnd(op, status);
    return std::make_tuple(status, willDo, willDoInPlace);
}

OSStatus DriverAdapter::WriteMixedOutput(AudioObjectID deviceObjectID,
    AudioObjectID streamObjectID,
    UInt32 clientID,
    const IOCycle& ioCycle,
    const void* bytes,
    UInt32 bytesCount)
{
    Tracer::Operation op;
    op.Name = "DriverAdapter::WriteMixedOutput()";
    op.ObjectID = deviceObjectID;
    op.Flags = Tracer::Flags::Realtime;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    AudioServerPlugInIOCycleInfo ioCycleInfo = MakeCycleInfo(ioCycle);

    status = InvokeMethod(&AudioServerPlugInDriverInterface::BeginIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationWriteMix,
        ioCycle.BatchFrameCount,
        &ioCycleInfo);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    status = InvokeMethod(&AudioServerPlugInDriverInterface::DoIOOperation,
        deviceObjectID,
        streamObjectID,
        clientID,
        kAudioServerPlugInIOOperationWriteMix,
        ioCycle.BatchFrameCount,
        &ioCycleInfo,
        const_cast<void*>(bytes),
        nullptr);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

    status = InvokeMethod(&AudioServerPlugInDriverInterface::EndIOOperation,
        deviceObjectID,
        clientID,
        kAudioServerPlugInIOOperationWriteMix,
        ioCycle.BatchFrameCount,
        &ioCycleInfo);
    if (status != kAudioHardwareNoError) {
        goto end;
    }

end:
    tracer_->OperationEnd(op, status);
    return status;
}

AudioServerPlugInIOCycleInfo DriverAdapter::MakeCycleInfo(const IOCycle& ioCycle)
{
    AudioServerPlugInIOCycleInfo ioCycleInfo = {};

    ioCycleInfo.mIOCycleCounter = ioCycle.CycleCounter;

    if (ioCycle.BatchNominalFrameCount != 0) {
        ioCycleInfo.mNominalIOBufferFrameSize = ioCycle.BatchNominalFrameCount;
    } else {
        ioCycleInfo.mNominalIOBufferFrameSize = ioCycle.BatchFrameCount;
    }

    ioCycleInfo.mInputTime.mSampleTime = ioCycle.BatchSampleTime;
    ioCycleInfo.mInputTime.mHostTime = ioCycle.BatchHostTime;
    ioCycleInfo.mInputTime.mFlags =
        kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid;

    ioCycleInfo.mOutputTime.mSampleTime = ioCycle.BatchSampleTime;
    ioCycleInfo.mOutputTime.mHostTime = ioCycle.BatchHostTime;
    ioCycleInfo.mOutputTime.mFlags =
        kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid;

    ioCycleInfo.mCurrentTime.mSampleTime = ioCycle.CurrentSampleTime;
    ioCycleInfo.mCurrentTime.mHostTime = ioCycle.CurrentHostTime;
    ioCycleInfo.mCurrentTime.mFlags =
        kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid;

    ioCycleInfo.mDeviceHostTicksPerFrame = ioCycle.HostTicksPerFrame;

    return ioCycleInfo;
}

template <typename Func, typename... Args>
OSStatus DriverAdapter::InvokeMethod(Func func, Args&&... args) const
{
    if (!driverRef_) {
        tracer_->Message("driverRef is null");
        return kAudioHardwareIllegalOperationError;
    }

    if (!*driverRef_) {
        tracer_->Message("driver vtbl is null");
        return kAudioHardwareIllegalOperationError;
    }

    auto funcPtr = (*driverRef_)->*func;

    if (funcPtr == nullptr) {
        tracer_->Message("method is null in driver vtbl");
        return kAudioHardwareUnsupportedOperationError;
    }

    return funcPtr(driverRef_, std::forward<Args>(args)...);
}

template <typename Type, typename Func, typename... Args>
Type DriverAdapter::InvokeMethodT(Func func, Args&&... args) const
{
    if (!driverRef_) {
        tracer_->Message("driverRef is null");
        return Type{};
    }

    if (!*driverRef_) {
        tracer_->Message("driver vtbl is null");
        return Type{};
    }

    auto funcPtr = (*driverRef_)->*func;

    if (funcPtr == nullptr) {
        tracer_->Message("method is null in driver vtbl");
        return Type{};
    }

    return funcPtr(driverRef_, std::forward<Args>(args)...);
}

} // namespace aspl
