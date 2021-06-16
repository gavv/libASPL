// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Plugin.hpp>

#include <algorithm>

namespace aspl {

Plugin::Plugin(const std::shared_ptr<const Context>& context,
    const PluginParameters& params)
    : Object(context, "Plugin", kAudioObjectPlugInObject)
    , params_(params)
{
}

std::string Plugin::GetManufacturer() const
{
    return params_.Manufacturer;
}

std::string Plugin::GetResourceBundlePath() const
{
    return params_.ResourceBundlePath;
}

std::vector<AudioObjectID> Plugin::GetDeviceIDs() const
{
    return GetOwnedObjectIDs(kAudioObjectPropertyScopeGlobal, kAudioDeviceClassID);
}

AudioObjectID Plugin::GetDeviceIDByUID(const std::string& uid) const
{
    auto readLock = deviceByUID_.GetReadLock();

    const auto& deviceByUID = readLock.GetReference();

    if (!deviceByUID.count(uid)) {
        return {};
    }

    const auto device = deviceByUID.at(uid);

    return device->GetID();
}

UInt32 Plugin::GetDeviceCount() const
{
    auto readLock = devices_.GetReadLock();

    const auto& devices = readLock.GetReference();

    return UInt32(devices.size());
}

std::shared_ptr<Device> Plugin::GetDeviceByIndex(UInt32 idx) const
{
    auto readLock = devices_.GetReadLock();

    const auto& devices = readLock.GetReference();

    if (devices.size() <= idx) {
        return {};
    }

    return devices[idx];
}

std::shared_ptr<Device> Plugin::GetDeviceByID(AudioObjectID deviceID) const
{
    auto readLock = deviceByID_.GetReadLock();

    const auto& deviceByID = readLock.GetReference();

    if (!deviceByID.count(deviceID)) {
        return {};
    }

    return deviceByID.at(deviceID);
}

void Plugin::AddDevice(const std::shared_ptr<Device>& device)
{
    std::lock_guard<decltype(writeMutex_)> writeLock(writeMutex_);

    Tracer::Operation op;
    op.Name = "Plugin::AddDevice()";
    op.ObjectID = GetID();

    GetContext()->Tracer->OperationBegin(op);

    AddOwnedObject(device);

    {
        auto devices = devices_.Get();

        devices.push_back(device);
        devices_.Set(std::move(devices));
    }

    {
        auto deviceByID = deviceByID_.Get();

        deviceByID[device->GetID()] = device;
        deviceByID_.Set(std::move(deviceByID));
    }

    {
        auto deviceByUID = deviceByUID_.Get();

        if (auto uid = device->GetDeviceUID(); !uid.empty()) {
            deviceByUID[uid] = device;
        }

        deviceByUID_.Set(std::move(deviceByUID));
    }

    NotifyPropertiesChanged(
        {kAudioObjectPropertyOwnedObjects, kAudioPlugInPropertyDeviceList});

    GetContext()->Tracer->OperationEnd(op, kAudioHardwareNoError);
}

void Plugin::RemoveDevice(const std::shared_ptr<Device>& device)
{
    std::lock_guard<decltype(writeMutex_)> writeLock(writeMutex_);

    Tracer::Operation op;
    op.Name = "Plugin::RemoveDevice()";
    op.ObjectID = GetID();

    GetContext()->Tracer->OperationBegin(op);

    {
        auto devices = devices_.Get();

        if (auto pos = std::find(devices.begin(), devices.end(), device);
            pos != devices.end()) {
            devices.erase(pos);
        }

        devices_.Set(std::move(devices));
    }

    {
        auto deviceByID = deviceByID_.Get();

        deviceByID.erase(device->GetID());
        deviceByID_.Set(std::move(deviceByID));
    }

    {
        auto deviceByUID = deviceByUID_.Get();

        if (auto uid = device->GetDeviceUID(); !uid.empty()) {
            deviceByUID.erase(uid);
        }

        deviceByUID_.Set(std::move(deviceByUID));
    }

    RemoveOwnedObject(device->GetID());

    NotifyPropertiesChanged(
        {kAudioObjectPropertyOwnedObjects, kAudioPlugInPropertyDeviceList});

    GetContext()->Tracer->OperationEnd(op, kAudioHardwareNoError);
}

} // namespace aspl
