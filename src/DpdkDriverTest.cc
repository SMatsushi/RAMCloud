/*
 * Copyright (c) 2011-2014 Stanford University
 * Copyright (c) 2014-2015 NEC Corporation
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DpdkDriverTest.cc
 * based on InfUdpDriverTest.cc
 */

#include "TestUtil.h"
#include "ServiceLocator.h"
#include "MockFastTransport.h"
#include "Tub.h"
#include "DpdkDriver.h"

namespace RAMCloud {

class DpdkDriverTest : public ::testing::Test {
  public:
    Context context;

    DpdkDriverTest()
        : context()
    {}

    // Used to wait for data to arrive on a driver by invoking the
    // dispatcher's polling loop; gives up if a long time goes by with
    // no data.
    const char *receivePacket(MockFastTransport *transport) {
        transport->packetData.clear();
        uint64_t start = Cycles::rdtsc();
        while (true) {
            context.dispatch->poll();
            if (transport->packetData.size() != 0) {
                return transport->packetData.c_str();
            }
            if (Cycles::toSeconds(Cycles::rdtsc() - start) > .1) {
                return "no packet arrived";
            }
        }
    }

  private:
    DISALLOW_COPY_AND_ASSIGN(DpdkDriverTest);
};

#if 0
TEST_F(DpdkDriverTest, loopbackPacket) {
    DpdkDriver *client =
            new DpdkDriver(&context, NULL);
    MockFastTransport clientTransport(&context, client);
    Driver::Address* serverAddress = client->newAddress(
        ServiceLocator("fast+dpdk:mac=52:54:00:5a:11:5f"));

    Buffer message;
    const char *testString = "This is a sample message";
    message.appendExternal(testString, downCast<uint32_t>(strlen(testString)));
    Buffer::Iterator iterator(&message);
    EXPECT_NO_FATAL_FAILURE({
        client->sendPacket(serverAddress, "header:", 7, &iterator);
        });

    EXPECT_STREQ("header:This is a sample message",
            receivePacket(&clientTransport));

    // Send a response back in the other direction.
    message.reset();
    message.appendExternal("response", 8);
    Buffer::Iterator iterator2(&message);
    client->sendPacket(serverAddress, "h:", 2, &iterator2);
    EXPECT_STREQ("h:response", receivePacket(&clientTransport));
    delete serverAddress;
}
#endif

#if 1
TEST_F(DpdkDriverTest, sendPacket) {
    DpdkDriver *client =
            new DpdkDriver(&context, NULL);
    MockFastTransport clientTransport(&context, client);
    Driver::Address* serverAddress = client->newAddress(
        ServiceLocator("fast+dpdk:mac=fe:54:00:5a:11:5f"));

    Buffer message;
    const char *testString = "This is a sample message";
    message.appendExternal(testString, downCast<uint32_t>(strlen(testString)));
    Buffer::Iterator iterator(&message);
    EXPECT_NO_FATAL_FAILURE({
        client->sendPacket(serverAddress, "header:", 7, &iterator);
        });
    delete serverAddress;
}
#endif

#if 0
// currently this test does not work because this test requires
// multiple DpdkDriver instances.
TEST_F(DpdkDriverTest, basics) {
    // Send a packet from a client-style driver to a server-style
    // driver.
    ServiceLocator serverLocator("fast+dpdk:");
    DpdkDriver *server =
            new DpdkDriver(&context, &serverLocator);
    MockFastTransport serverTransport(&context, server);
    DpdkDriver *client =
            new DpdkDriver(&context, NULL);
    MockFastTransport clientTransport(&context, client);
    Driver::Address* serverAddress =
            client->newAddress(ServiceLocator(server->getServiceLocator()));

    Buffer message;
    const char *testString = "This is a sample message";
    message.appendExternal(testString, downCast<uint32_t>(strlen(testString)));
    Buffer::Iterator iterator(&message);
    client->sendPacket(serverAddress, "header:", 7, &iterator);
    EXPECT_STREQ("header:This is a sample message",
            receivePacket(&serverTransport));

    // Send a response back in the other direction.
    message.reset();
    message.appendExternal("response", 8);
    Buffer::Iterator iterator2(&message);
    server->sendPacket(serverTransport.sender, "h:", 2, &iterator2);
    EXPECT_STREQ("h:response", receivePacket(&clientTransport));
    delete serverAddress;
}
#endif

}  // namespace RAMCloud
