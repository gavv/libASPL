#include <aspl/Driver.hpp>

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>

#include <gtest/gtest.h>

struct ClientsTest : ::testing::Test
{
    std::shared_ptr<aspl::Tracer> tracer =
        std::make_shared<aspl::Tracer>(aspl::Tracer::Mode::Noop);

    std::shared_ptr<aspl::Context> context = std::make_shared<aspl::Context>(tracer);

    std::shared_ptr<aspl::Plugin> plugin = std::make_shared<aspl::Plugin>(context);

    std::shared_ptr<aspl::Driver> driver =
        std::make_shared<aspl::Driver>(context, plugin);

    std::shared_ptr<aspl::Device> device = std::make_shared<aspl::Device>(context);

    void SetUp() override
    {
        plugin->AddDevice(device);
    }
};

TEST_F(ClientsTest, NoClients)
{
    ASSERT_EQ(0, device->GetClientCount());

    auto clients = device->GetClients();
    ASSERT_TRUE(clients.empty());

    auto client = device->GetClientByID(0);
    ASSERT_FALSE(client);
}

TEST_F(ClientsTest, AddClient)
{
    const auto iface = driver->GetPluginInterface();

    AudioServerPlugInClientInfo clientInfo;
    clientInfo.mClientID = 111;
    clientInfo.mProcessID = 222;
    clientInfo.mIsNativeEndian = true;
    clientInfo.mBundleID = CFSTR("test");

    ASSERT_EQ(kAudioHardwareNoError,
        iface.AddDeviceClient(driver->GetReference(), device->GetID(), &clientInfo));

    ASSERT_EQ(1, device->GetClientCount());

    auto clients = device->GetClients();
    ASSERT_EQ(1, clients.size());

    auto client = device->GetClientByID(clientInfo.mClientID);
    ASSERT_TRUE(client);
    ASSERT_EQ(client, clients[0]);

    EXPECT_EQ(client->GetClientID(), clientInfo.mClientID);
    EXPECT_EQ(client->GetProcessID(), clientInfo.mProcessID);
    EXPECT_EQ(client->GetIsNativeEndian(), clientInfo.mIsNativeEndian);
    EXPECT_EQ(client->GetBundleID(), "test");
}

TEST_F(ClientsTest, RemoveClient)
{
    const auto iface = driver->GetPluginInterface();

    AudioServerPlugInClientInfo clientInfo;
    clientInfo.mClientID = 111;
    clientInfo.mProcessID = 222;
    clientInfo.mIsNativeEndian = true;
    clientInfo.mBundleID = CFSTR("test");

    ASSERT_EQ(kAudioHardwareNoError,
        iface.AddDeviceClient(driver->GetReference(), device->GetID(), &clientInfo));

    ASSERT_EQ(1, device->GetClientCount());

    ASSERT_EQ(kAudioHardwareNoError,
        iface.RemoveDeviceClient(driver->GetReference(), device->GetID(), &clientInfo));

    ASSERT_EQ(0, device->GetClientCount());

    auto clients = device->GetClients();
    ASSERT_TRUE(clients.empty());

    auto client = device->GetClientByID(clientInfo.mClientID);
    ASSERT_FALSE(client);
}
