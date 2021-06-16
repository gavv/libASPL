// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Driver.hpp>

#include <CoreAudio/AudioServerPlugIn.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

// Control and I/O request handler.
class ExampleHandler : public aspl::ControlRequestHandler, public aspl::IORequestHandler
{
public:
    // Handler will send samples to this address via UDP.
    static constexpr const char* SocketAddr = "127.0.0.1";
    static constexpr short SocketPort = 4444;
    static constexpr UInt32 MaxPacketSize = 512;

    // Invoked on control thread before first I/O request.
    OSStatus OnStartIO() override
    {
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == -1) {
            return kAudioHardwareUnspecifiedError;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SocketPort);
        inet_pton(AF_INET, SocketAddr, &addr.sin_addr);

        if (connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            close(socket_);
            socket_ = -1;
            return kAudioHardwareUnspecifiedError;
        }

        return kAudioHardwareNoError;
    }

    // Invoked on control thread after last I/O request.
    void OnStopIO() override
    {
        if (socket_ != -1) {
            close(socket_);
            socket_ = -1;
        }
    }

    // Invoked on realtime I/O thread to write mixed data from clients.
    void OnWriteMixedOutput(const std::shared_ptr<aspl::Stream>& stream,
        Float64 zeroTimestamp,
        Float64 timestamp,
        const void* buff,
        UInt32 buffBytesSize) override
    {
        while (buffBytesSize != 0) {
            const UInt32 packetBytesSize = std::min(buffBytesSize, MaxPacketSize);

            send(socket_, buff, packetBytesSize, MSG_DONTWAIT);

            buff = reinterpret_cast<const UInt8*>(buff) + packetBytesSize;
            buffBytesSize -= packetBytesSize;
        }
    }

private:
    int socket_ = -1;
};

std::shared_ptr<aspl::Driver> CreateExampleDriver()
{
    // Create context, shared between all other objects.
    // You can provide custom tracer here.
    auto context = std::make_shared<aspl::Context>();

    // Create device object with some custom parameters.
    aspl::DeviceParameters deviceParams;
    deviceParams.Name = "Example Device";
    deviceParams.SampleRate = 44100;
    deviceParams.ChannelCount = 2;
    deviceParams.EnableMixing = true;
    deviceParams.EnableRealtimeTracing = false;

    auto device = std::make_shared<aspl::Device>(context, deviceParams);

    // Add to device one stream, one volume control, and one mute control.
    // Associate volume and mute control with the stream.
    //
    // If desired, you can provide parameters for streams and controls as well,
    // but for simplicity we use defaults here.
    //
    // HAL will use stream and controls to determine how to work with our device
    // and to store volume and mute settings.
    //
    // IORequestHandler will use stream to apply stored volume and mute settings.
    device->AddStreamWithControlsAsync(aspl::Direction::Output);

    // Create and set custom handler for both control and I/O requests.
    // You can use separate handlers, but here we use one.
    auto handler = std::make_shared<ExampleHandler>();

    device->SetControlHandler(handler);
    device->SetIOHandler(handler);

    // Create plugin object, the root of the object hierarchy, and add
    // our device to it.
    //
    // The main purpose of plugin is to provide the list of devices to HAL.
    //
    // For simplicity we use default parameters.
    auto plugin = std::make_shared<aspl::Plugin>(context);

    plugin->AddDevice(device);

    // Create driver, the top-level entry point.
    // Driver owns plugin object and thus the whole object hierarchy,
    // and provides C interface for HAL.
    auto driver = std::make_shared<aspl::Driver>(context, plugin);

    return driver;
}

} // namespace

extern "C" void* ExampleEntryPoint(CFAllocatorRef allocator, CFUUIDRef typeUUID)
{
    // The UUID of the plug-in type (443ABAB8-E7B3-491A-B985-BEB9187030DB).
    if (!CFEqual(typeUUID, kAudioServerPlugInTypeUUID)) {
        return nullptr;
    }

    // Store shared pointer to the driver to keep it alive.
    static std::shared_ptr<aspl::Driver> driver = CreateExampleDriver();

    return driver->GetReference();
}
