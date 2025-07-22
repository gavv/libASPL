// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/VirtualHost.hpp>

#include "Convert.hpp"
#include "Strings.hpp"

#include <cmath>

namespace chrono = std::chrono;

namespace aspl {

VirtualHost::VirtualHost(std::shared_ptr<Tracer> tracer,
    const VirtualHostParameters& params)
    : params_(params)
    , tracer_(tracer ? std::move(tracer) : std::make_shared<Tracer>(Tracer::Mode::Stderr))
{
    hostInterface_.PropertiesChanged = PropertiesChangedJumper;
    hostInterface_.RequestDeviceConfigurationChange =
        RequestDeviceConfigurationChangeJumper;
    hostInterface_.CopyFromStorage = CopyFromStorageJumper;
    hostInterface_.WriteToStorage = WriteToStorageJumper;
    hostInterface_.DeleteFromStorage = DeleteFromStorageJumper;
}

VirtualHost::~VirtualHost()
{
}

AudioServerPlugInHostRef VirtualHost::GetReference()
{
    return &hostInterface_;
}

VirtualHost* VirtualHost::GetHost(AudioServerPlugInHostRef inHost)
{
    if (!inHost) {
        return nullptr;
    }

    return const_cast<VirtualHost*>(reinterpret_cast<const VirtualHost*>(
        reinterpret_cast<const char*>(inHost) - offsetof(VirtualHost, hostInterface_)));
}

bool VirtualHost::HasAvailEvents() const
{
    UInt64 eventCount = 0;

    auto now = GetCurrentTimeImpl();
    auto tick = GetNextTickImpl(now);
    auto safetyZone = IsSafetyZoneImpl(now);

    if (abortFlag_) {
        goto end;
    }

    if (tick && tick->Time <= now) {
        // Next call to ReadEvent() will generate tick event.
        return true;
    }

    if (!tick || !safetyZone) {
        if (PendingEventsImpl() > 0) {
            // Next call to ReadEvent() will return queued event.
            return true;
        }
    }

end:
    return false;
}

std::shared_ptr<VirtualEventInfo> VirtualHost::WaitEvent(
    std::optional<chrono::steady_clock::time_point> deadline)
{
    std::shared_ptr<VirtualEventInfo> resultEvent;
    bool waitedEvent = false;

    if (abortFlag_) {
        goto end;
    }

    for (;;) {
        auto now = GetCurrentTimeImpl();
        auto tick = GetNextTickImpl(now);
        auto safetyZone = IsSafetyZoneImpl(now);

        if (tick && now >= tick->Time) {
            // Time to generate IO tick event. These events take priority
            // over any other events. DoNextTickImpl() may return false e.g.
            // if concurrent call to WaitEvent() already advanced tick.
            if (DoNextTickImpl(now, tick->Number)) {
                resultEvent = GenerateTickEvent(*tick);
                resultEvent->EnqueueTime = now;
                resultEvent->DequeueTime = now;
                goto end;
            }
        }

        if (waitedEvent && deadline && now >= *deadline) {
            // We already waited and deadline expired.
            goto end;
        }

        // Select nearest wake up point: user-provided deadline or IO tick.
        std::optional<chrono::steady_clock::time_point> nearestDeadline =
            deadline && tick ? std::make_optional(std::min(*deadline, tick->Time))
            : deadline       ? deadline
            : tick           ? std::make_optional(tick->Time)
                             : std::nullopt;

        if (tick) {
            TraceTickImpl(*tick, safetyZone, now, *nearestDeadline);
        }

        if (tick && safetyZone) {
            // Safety zone, ignore events and wait next tick.
            sleepSem_.TimedWait(*nearestDeadline);
            if (abortFlag_) {
                goto end;
            }
        } else {
            // Wait for event or tick / deadline.
            auto event = WaitEventImpl(nearestDeadline);
            if (abortFlag_) {
                goto end;
            }
            if (event) {
                resultEvent = event;
                resultEvent->DequeueTime = now;
                goto end;
            }
            // Timed out. Repeat loop to check if it's time for a tick,
            // or maybe deadline expired.
            waitedEvent = true;
        }
    }

end:
    TraceEventImpl("VirtualHost::WaitEvent()", resultEvent);
    return resultEvent;
}

std::shared_ptr<VirtualEventInfo> VirtualHost::ReadEvent()
{
    std::shared_ptr<VirtualEventInfo> resultEvent;

    auto now = GetCurrentTimeImpl();
    auto tick = GetNextTickImpl(now);
    auto safetyZone = IsSafetyZoneImpl(now);

    if (abortFlag_) {
        goto end;
    }

    if (tick && now >= tick->Time) {
        // Time to generate IO tick event. IO tick takes priority over other events.
        if (DoNextTickImpl(now, tick->Number)) {
            resultEvent = GenerateTickEvent(*tick);
            resultEvent->EnqueueTime = now;
            resultEvent->DequeueTime = now;
            goto end;
        }
    }

    if (tick && safetyZone) {
        // Safety zone, ignore events until next tick.
        goto end;
    }

    resultEvent = ReadEventImpl();
    if (resultEvent) {
        resultEvent->DequeueTime = now;
    }

end:
    if (resultEvent) {
        TraceEventImpl("VirtualHost::ReadEvent()", resultEvent);
    }
    return resultEvent;
}

std::shared_ptr<VirtualEventInfo> VirtualHost::AllocateEvent(VirtualEventType eventType)
{
    auto result = AllocateEventImpl(eventType);

    if (!result) {
        tracer_->Message("VirtualHost::AllocateEventImpl() returned null");
    }

    return result;
}

void VirtualHost::EnqueueEvent(std::shared_ptr<VirtualEventInfo> event)
{
    if (abortFlag_) {
        return;
    }

    if (!event) {
        return;
    }

    event->EnqueueTime = GetCurrentTimeImpl();

    TraceEventImpl("VirtualHost::EnqueueEvent()", event);

    return EnqueueEventImpl(std::move(event));
}

void VirtualHost::AbortEvents()
{
    bool expected = false;
    if (!abortFlag_.compare_exchange_strong(expected, true)) {
        // Already aborted.
        return;
    }

    tracer_->Message("VirtualHost::AbortEvents()");

    // Interrupt WaitEvent() sleeping in safety zone.
    sleepSem_.Close();

    // Interrupt WaitEvent() sleeping on event semaphore.
    AbortEventsImpl();
}

UInt64 VirtualHost::PendingEventsImpl() const
{
    return eventCount_;
}

std::shared_ptr<VirtualEventInfo> VirtualHost::WaitEventImpl(
    std::optional<chrono::steady_clock::time_point> deadline)
{
    if (deadline) {
        if (!eventSem_.TimedWait(*deadline)) {
            // Timeout or aborted.
            return nullptr;
        }
    } else {
        if (!eventSem_.Wait()) {
            // Aborted.
            return nullptr;
        }
    }

    auto event = eventQueue_.PopNode();
    if (!event) {
        // Should not happen.
        return nullptr;
    }

    eventCount_--;
    return event;
}

std::shared_ptr<VirtualEventInfo> VirtualHost::ReadEventImpl()
{
    if (!eventSem_.TryWait()) {
        // Aborted.
        return nullptr;
    }

    auto event = eventQueue_.PopNode();
    if (!event) {
        // Should not happen.
        return nullptr;
    }

    eventCount_--;
    return event;
}

std::shared_ptr<VirtualEventInfo> VirtualHost::AllocateEventImpl(
    VirtualEventType eventType)
{
    return eventPool_.Allocate(eventType);
}

void VirtualHost::EnqueueEventImpl(std::shared_ptr<VirtualEventInfo> event)
{
    eventCount_++;
    eventQueue_.PushNode(std::move(event));
    eventSem_.Post();
}

void VirtualHost::AbortEventsImpl()
{
    eventSem_.Close();
}

void VirtualHost::TraceEventImpl(const char* operation,
    const std::shared_ptr<VirtualEventInfo>& event)
{
    if (!params_.EnableEventTracing) {
        return;
    }

    if (event && event->EventType == VirtualEventType::IOTimerTick &&
        !params_.EnableIOTracing) {
        return;
    }

    if (!event) {
        tracer_->Message("%s <null event>", operation);
        return;
    }

    const long long enqTimeUs =
        chrono::duration_cast<chrono::microseconds>(event->EnqueueTime.time_since_epoch())
            .count();
    const long long deqTimeUs =
        chrono::duration_cast<chrono::microseconds>(event->DequeueTime.time_since_epoch())
            .count();

    switch (event->EventType) {
    case VirtualEventType::ObjectPropertyChange:
        if (event->ObjectPropertyChange) {
            tracer_->Message(
                "%s ObjectPropertyChange enqTime=%lldus deqTime=%lldus"
                " objectID=%u selector=%s scope=%s element=%u",
                operation,
                enqTimeUs,
                deqTimeUs,
                static_cast<unsigned>(event->ObjectPropertyChange->ObjectID),
                PropertySelectorToString(
                    event->ObjectPropertyChange->PropertyAddress.mSelector)
                    .c_str(),
                PropertyScopeToString(event->ObjectPropertyChange->PropertyAddress.mScope)
                    .c_str(),
                static_cast<unsigned>(
                    event->ObjectPropertyChange->PropertyAddress.mElement));
        } else {
            tracer_->Message(
                "%s ObjectPropertyChange enqTime=%lldus deqTime=%lldus"
                " <null payload>",
                operation,
                enqTimeUs,
                deqTimeUs);
        }
        return;

    case VirtualEventType::DeviceConfigurationRequest:
        if (event->DeviceConfigurationRequest) {
            tracer_->Message(
                "%s DeviceConfigurationRequest enqTime=%lldus deqTime=%lldus"
                " deviceID=%u changeAction=%llu changeInfo=%p",
                operation,
                enqTimeUs,
                deqTimeUs,
                static_cast<unsigned>(event->DeviceConfigurationRequest->DeviceID),
                static_cast<unsigned long long>(
                    event->DeviceConfigurationRequest->ChangeAction),
                event->DeviceConfigurationRequest->ChangeInfo);
        } else {
            tracer_->Message(
                "%s DeviceConfigurationRequest enqTime=%lldus deqTime=%lldus"
                " <null payload>",
                operation,
                enqTimeUs,
                deqTimeUs);
        }
        return;

    case VirtualEventType::StorageDataChange:
        if (event->StorageDataChange) {
            tracer_->Message(
                "%s StorageDataChange enqTime=%lldus deqTime=%lldus"
                " storageKey=\"%s\"",
                operation,
                enqTimeUs,
                deqTimeUs,
                event->StorageDataChange->StorageKey.c_str());
        } else {
            tracer_->Message(
                "%s StorageDataChange enqTime=%lldus deqTime=%lldus"
                " <null payload>",
                operation,
                enqTimeUs,
                deqTimeUs);
        }
        return;

    case VirtualEventType::IOTimerTick:
        if (event->IOTimerTick) {
            tracer_->Message(
                "%s IOTimerTick enqTime=%lldus deqTime=%lldus"
                " tickNumber=%lld tickTime=%lldus",
                operation,
                enqTimeUs,
                deqTimeUs,
                static_cast<long long>(event->IOTimerTick->Number),
                static_cast<long long>(chrono::duration_cast<chrono::microseconds>(
                    event->IOTimerTick->Time.time_since_epoch())
                                           .count()));
        } else {
            tracer_->Message(
                "%s IOTimerTick enqTime=%lldus deqTime=%lldus"
                " <null payload>",
                operation,
                enqTimeUs,
                deqTimeUs);
        }
        return;

    case VirtualEventType::CustomEvent:
        tracer_->Message("%s CustomEvent enqTime=%lldus deqTime=%lldus",
            operation,
            enqTimeUs,
            deqTimeUs);
        return;
    }

    tracer_->Message("%s <unknown event> enqTime=%lldus deqTime=%lldus",
        operation,
        enqTimeUs,
        deqTimeUs);
}

chrono::steady_clock::time_point VirtualHost::GetCurrentTimeImpl() const
{
    return chrono::steady_clock::now();
}

bool VirtualHost::IsSafetyZoneImpl(chrono::steady_clock::time_point currentTime) const
{
    if (params_.TicksPerSecond <= 0.0 || params_.TickSafetyZone.count() <= 0) {
        return false;
    }

    auto tick = GetNextTickImpl(currentTime);
    if (!tick) {
        return false;
    }

    if (currentTime < tick->Time - params_.TickSafetyZone) {
        return false;
    }

    return true;
}

std::optional<VirtualEventInfo::IOTimerTickInfo> VirtualHost::GetNextTickImpl(
    chrono::steady_clock::time_point currentTime) const
{
    if (params_.TicksPerSecond <= 0.0) {
        return std::nullopt;
    }

    std::call_once(timerInitFlag_, [&]() {
        timerStartTime_ = currentTime;
        timerTickNum_ = -1;
    });

    const double nsPerTick = 1e9 / params_.TicksPerSecond;

    auto nextTickNum = timerTickNum_.load() + 1;
    auto nextTickTime =
        timerStartTime_ + chrono::nanoseconds(SInt64(nsPerTick * nextTickNum));
    auto nextTickLateTime =
        timerStartTime_ + chrono::nanoseconds(SInt64(nsPerTick * (nextTickNum + 1)));

    if (params_.SkipLateTicks && currentTime >= nextTickLateTime) {
        const auto elapsedNs = double(
            chrono::duration_cast<chrono::nanoseconds>(currentTime - timerStartTime_)
                .count());

        const auto nearestTick =
            SInt64(std::round(elapsedNs / 1e9 * params_.TicksPerSecond));

        if (nextTickNum < nearestTick) {
            nextTickNum = nearestTick;
            nextTickTime =
                timerStartTime_ + chrono::nanoseconds(SInt64(nsPerTick * nearestTick));
        }
    }

    VirtualEventInfo::IOTimerTickInfo tick;
    tick.Number = nextTickNum;
    tick.Time = nextTickTime;

    return tick;
}

bool VirtualHost::DoNextTickImpl(chrono::steady_clock::time_point currentTime,
    SInt64 tickNum)
{
    if (params_.TicksPerSecond <= 0.0) {
        return false;
    }

    std::call_once(timerInitFlag_, [&]() {
        timerStartTime_ = currentTime;
        timerTickNum_ = -1;
    });

    SInt64 oldTickNum = timerTickNum_.load();
    bool success = false;

    while (oldTickNum < tickNum) {
        if (timerTickNum_.compare_exchange_weak(oldTickNum, tickNum)) {
            success = true;
            break;
        }
    }

    if (params_.EnableIOTracing) {
        tracer_->Message("VirtualHost::DoNextTickImpl() %s tick: %lld -> %lld",
            success ? "switched" : "rejected",
            static_cast<long long>(oldTickNum),
            static_cast<long long>(tickNum));
    }

    return success;
}

void VirtualHost::TraceTickImpl(const VirtualEventInfo::IOTimerTickInfo& tickInfo,
    bool safetyZone,
    const chrono::steady_clock::time_point& currentTime,
    const chrono::steady_clock::time_point& wakeupTime)
{
    if (!params_.EnableIOTracing) {
        return;
    }

    tracer_->Message(
        "VirtualHost::TraceTickImpl()"
        " nextTick=%lld nextTickTime=%+.3fms wakeupTime=%+.3fms safetyZone=%s",
        static_cast<long long>(tickInfo.Number),
        chrono::duration_cast<chrono::nanoseconds>(tickInfo.Time - currentTime).count() /
            1e6,
        chrono::duration_cast<chrono::nanoseconds>(wakeupTime - currentTime).count() /
            1e6,
        safetyZone ? "true" : "false");
}

std::shared_ptr<VirtualEventInfo> VirtualHost::GenerateTickEvent(
    const VirtualEventInfo::IOTimerTickInfo& tick)
{
    auto event = AllocateEvent(VirtualEventType::IOTimerTick);

    event->IOTimerTick.emplace(tick);

    return event;
}

OSStatus VirtualHost::PropertiesChangedJumper(AudioServerPlugInHostRef inHost,
    AudioObjectID inObjectID,
    UInt32 inNumberAddresses,
    const AudioObjectPropertyAddress* inAddresses)
{
    VirtualHost* host = GetHost(inHost);
    if (!host) {
        return kAudioHardwareUnspecifiedError;
    }

    return host->PropertiesChanged(inObjectID, inNumberAddresses, inAddresses);
}

OSStatus VirtualHost::RequestDeviceConfigurationChangeJumper(
    AudioServerPlugInHostRef inHost,
    AudioObjectID inDeviceObjectID,
    UInt64 inChangeAction,
    void* inChangeInfo)
{
    VirtualHost* host = GetHost(inHost);
    if (!host) {
        return kAudioHardwareUnspecifiedError;
    }

    return host->RequestDeviceConfigurationChange(
        inDeviceObjectID, inChangeAction, inChangeInfo);
}

OSStatus VirtualHost::CopyFromStorageJumper(AudioServerPlugInHostRef inHost,
    CFStringRef inKey,
    CFPropertyListRef* outData)
{
    VirtualHost* host = GetHost(inHost);
    if (!host) {
        return kAudioHardwareUnspecifiedError;
    }

    return host->CopyFromStorage(inKey, outData);
}

OSStatus VirtualHost::WriteToStorageJumper(AudioServerPlugInHostRef inHost,
    CFStringRef inKey,
    CFPropertyListRef inData)
{
    VirtualHost* host = GetHost(inHost);
    if (!host) {
        return kAudioHardwareUnspecifiedError;
    }

    return host->WriteToStorage(inKey, inData);
}

OSStatus VirtualHost::DeleteFromStorageJumper(AudioServerPlugInHostRef inHost,
    CFStringRef inKey)
{
    VirtualHost* host = GetHost(inHost);
    if (!host) {
        return kAudioHardwareUnspecifiedError;
    }

    return host->DeleteFromStorage(inKey);
}

OSStatus VirtualHost::PropertiesChanged(AudioObjectID inObjectID,
    UInt32 inNumberAddresses,
    const AudioObjectPropertyAddress* inAddresses)
{
    Tracer::Operation op;
    op.Name = "VirtualHost::PropertiesChanged()";
    op.ObjectID = inObjectID;

    tracer_->OperationBegin(op);

    tracer_->Message("generating event(s) for %u property address(es)",
        static_cast<unsigned>(inNumberAddresses));

    OSStatus status = kAudioHardwareNoError;

    for (UInt32 i = 0; i < inNumberAddresses; ++i) {
        auto event = AllocateEvent(VirtualEventType::ObjectPropertyChange);

        event->ObjectPropertyChange.emplace();
        event->ObjectPropertyChange->ObjectID = inObjectID;
        event->ObjectPropertyChange->PropertyAddress = inAddresses[i];

        EnqueueEvent(std::move(event));
    }

    tracer_->OperationEnd(op, status);

    return status;
}

OSStatus VirtualHost::RequestDeviceConfigurationChange(AudioObjectID inDeviceObjectID,
    UInt64 inChangeAction,
    void* inChangeInfo)
{
    Tracer::Operation op;
    op.Name = "VirtualHost::RequestDeviceConfigurationChange()";
    op.ObjectID = inDeviceObjectID;

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    auto event = AllocateEvent(VirtualEventType::DeviceConfigurationRequest);

    event->DeviceConfigurationRequest.emplace();
    event->DeviceConfigurationRequest->DeviceID = inDeviceObjectID;
    event->DeviceConfigurationRequest->ChangeAction = inChangeAction;
    event->DeviceConfigurationRequest->ChangeInfo = inChangeInfo;

    EnqueueEvent(std::move(event));

    tracer_->OperationEnd(op, status);

    return status;
}

OSStatus VirtualHost::CopyFromStorage(CFStringRef inKey, CFPropertyListRef* outData) const
{
    Tracer::Operation op;
    op.Name = "VirtualHost::CopyFromStorage()";
    op.Flags = Tracer::Flags::Readonly;

    if (params_.EnableStorageTracing) {
        tracer_->OperationBegin(op);
    }

    OSStatus status = kAudioHardwareNoError;

    std::string key;
    std::vector<UInt8> serializedData;

    if (!inKey) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("key is null");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (!outData) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("output data is null");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (!Convert::FromFoundation(inKey, key)) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("can't decode key");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    status = CopyFromStorageImpl(key, serializedData);
    if (status != kAudioHardwareNoError) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("VirtualHost::CopyFromStorageImpl() failed");
        }
        goto end;
    }

    if (params_.EnableStorageTracing) {
        tracer_->Message("VirtualHost::CopyFromStorageImpl() key=\"%s\" dataSize=%lu",
            key.c_str(),
            static_cast<unsigned long>(serializedData.size()));
    }

    if (!Convert::Deserialize(serializedData, *outData)) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("can't deserialize data");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

end:
    if (params_.EnableStorageTracing) {
        tracer_->OperationEnd(op, status);
    }

    return status;
}

OSStatus VirtualHost::WriteToStorage(CFStringRef inKey, CFPropertyListRef inData)
{
    Tracer::Operation op;
    op.Name = "VirtualHost::WriteToStorage()";

    if (params_.EnableStorageTracing) {
        tracer_->OperationBegin(op);
    }

    OSStatus status = kAudioHardwareNoError;

    std::string key;
    std::vector<UInt8> serializedData;
    std::shared_ptr<VirtualEventInfo> event;

    if (!inKey) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("key is null");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (!inData) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("input data is null");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (!Convert::FromFoundation(inKey, key)) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("can't decode key");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    Convert::Serialize(inData, serializedData);
    if (serializedData.empty()) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("can't serialize data");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (params_.EnableStorageTracing) {
        tracer_->Message("VirtualHost::WriteToStorageImpl() key=\"%s\" dataSize=%lu",
            key.c_str(),
            static_cast<unsigned long>(serializedData.size()));
    }

    status = WriteToStorageImpl(key, serializedData);
    if (status != kAudioHardwareNoError) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("VirtualHost::WriteToStorageImpl() failed");
        }
        goto end;
    }

    event = AllocateEvent(VirtualEventType::StorageDataChange);

    event->StorageDataChange.emplace();
    event->StorageDataChange->StorageKey = std::move(key);

    EnqueueEvent(std::move(event));

end:
    if (params_.EnableStorageTracing) {
        tracer_->OperationEnd(op, status);
    }

    return status;
}

OSStatus VirtualHost::DeleteFromStorage(CFStringRef inKey)
{
    Tracer::Operation op;
    op.Name = "VirtualHost::DeleteFromStorage()";

    if (params_.EnableStorageTracing) {
        tracer_->OperationBegin(op);
    }

    OSStatus status = kAudioHardwareNoError;

    std::string key;
    std::shared_ptr<VirtualEventInfo> event;

    if (!inKey) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("key is null");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (!Convert::FromFoundation(inKey, key)) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("can't decode key");
        }
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (params_.EnableStorageTracing) {
        tracer_->Message("VirtualHost::DeleteFromStorageImpl() key=\"%s\"", key.c_str());
    }

    status = DeleteFromStorageImpl(key);
    if (status != kAudioHardwareNoError) {
        if (params_.EnableStorageTracing) {
            tracer_->Message("VirtualHost::DeleteFromStorageImpl() failed");
        }
        goto end;
    }

    event = AllocateEvent(VirtualEventType::StorageDataChange);

    event->StorageDataChange.emplace();
    event->StorageDataChange->StorageKey = std::move(key);

    EnqueueEvent(std::move(event));

end:
    if (params_.EnableStorageTracing) {
        tracer_->OperationEnd(op, status);
    }

    return status;
}

OSStatus VirtualHost::CopyFromStorageImpl(const std::string& key,
    std::vector<UInt8>& outData) const
{
    std::shared_lock lock(storageMutex_);

    auto it = storage_.find(key);
    if (it == storage_.end()) {
        return kAudioHardwareUnknownPropertyError;
    }

    outData = it->second;
    return kAudioHardwareNoError;
}

OSStatus VirtualHost::WriteToStorageImpl(const std::string& key,
    const std::vector<UInt8>& data)
{
    std::unique_lock lock(storageMutex_);

    storage_[key] = data;
    return kAudioHardwareNoError;
}

OSStatus VirtualHost::DeleteFromStorageImpl(const std::string& key)
{
    std::unique_lock lock(storageMutex_);

    auto it = storage_.find(key);
    if (it == storage_.end()) {
        return kAudioHardwareUnknownPropertyError;
    }

    storage_.erase(it);
    return kAudioHardwareNoError;
}

} // namespace aspl
