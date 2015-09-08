/*
 * Copyright (c) 2010-2014 Stanford University
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

/**
 * \file
 * Header file for #RAMCloud::DpdkDriver.
 */

/*
 * Todo:
 *   - add copyright notice
 *   - cleanup unused variables, methods
 *
 * Candidates of reworking, refactoring
 *   - support multiple dpdk ports
 *
 */

#ifndef RAMCLOUD_DPDKDRIVER_H
#define RAMCLOUD_DPDKDRIVER_H

#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <stdint.h>

#include <vector>

#include "Common.h"
#include "Dispatch.h"
#include "Driver.h"
#include "MacAddress.h"
#include "ObjectPool.h"
#include "Tub.h"

namespace RAMCloud {

/**
 * A Driver for Intel DPDK communication.
 * Simple packet send/receive style interface. See Driver for more detail.
 */
class DpdkDriver : public Driver {
  public:
    explicit DpdkDriver(Context* context,
                        const ServiceLocator* localServiceLocator = NULL);
    virtual ~DpdkDriver();
    virtual void connect(IncomingPacketHandler* incomingPacketHandler);
    virtual void disconnect();
    virtual uint32_t getMaxPacketSize();
    virtual void release(char *payload);
    virtual void sendPacket(const Address *recipient,
                            const void *header,
                            uint32_t headerLen,
                            Buffer::Iterator *payload);

    virtual string getServiceLocator();

#if defined(NOT_USED)
    virtual struct rte_mbuf *getTransmitBuffer();
    virtual struct rte_mbuf *tryReceive(int portid);
    virtual void postReceive(struct rte_mbuf *pkt);
    virtual void postSend(int port, rte_mbuf *pkt);
#endif
    virtual int init_dpdk(uint32_t portmask, struct ether_addr *eth_addr);
    virtual char *eth2str(struct ether_addr *eth_addr);

    virtual Driver::Address* newAddress(const ServiceLocator& serviceLocator) {
        return new MacAddress(serviceLocator.getOption<const char*>("mac"));
    }

    struct EthernetHeader {
        uint8_t destAddress[6];
        uint8_t sourceAddress[6];
        uint16_t etherType;         // network order
        uint16_t length;            // host order, length of payload,
                                    // used to drop padding from end of short
                                    // packets
    } __attribute__((packed));

    /// The maximum number bytes we can stuff in a Ethernet packet payload.
    static const uint32_t MAX_PAYLOAD_SIZE = 1500 + 14 - sizeof(EthernetHeader);

    /**
     * Structure to hold an incoming packet.
     */
    struct PacketBuf {
        PacketBuf() : macAddress() {}
        /**
         * Address of sender (used to send reply).
         */
        Tub<MacAddress> macAddress;
        /**
         * Packet data (may not fill all of the allocated space).
         */
        char payload[MAX_PAYLOAD_SIZE];
    };

    /// Shared RAMCloud information.
    Context* context;

    /// Handler to invoke whenever packets arrive.
    /// NULL means #connect hasn't been called yet.
    std::unique_ptr<IncomingPacketHandler> incomingPacketHandler;

    /// Holds packet buffers that are no longer in use, for use in future
    /// requests; saves the overhead of calling malloc/free for each request.
    ObjectPool<PacketBuf> packetBufPool;

    /// Tracks number of outstanding allocated payloads.  For detecting leaks.
    int packetBufsUtilized;

#if defined(NOT_USED)
    /// Counts the number of packet buffers freed during destructors;
    /// used primarily for testing.
    static int packetBufsFreed;
#endif


    // REVISIT: support multiple dpdk ports
    Tub<MacAddress> localMac;           // our MAC address

    /// The original ServiceLocator string. May be empty if the constructor
    /// argument was NULL. May also differ if dynamic ports are used.
    string locatorString;

    /**
     * The following object is invoked by the dispatcher's polling loop;
     * it reads incoming packets and passes them on to #transport.
     */
    class Poller : public Dispatch::Poller {
      public:
        explicit Poller(Context* context, DpdkDriver* driver)
            : Dispatch::Poller(context->dispatch, "DpdkDriver::Poller")
            , driver(driver) { }
        virtual int poll();
      private:
        // Driver on whose behalf this poller operates.
        DpdkDriver* driver;
        DISALLOW_COPY_AND_ASSIGN(Poller);
    };
    Tub<Poller> poller;

    // REVISIT: DPDK stuff here
#define MAX_PKT_BURST 32

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

/* mempool parameters */
#define NB_MBUF 8192
#define MBUF_SIZE (2048 + static_cast<unsigned int>(sizeof(struct rte_mbuf)) + \
                   RTE_PKTMBUF_HEADROOM)
#define CACHE_SIZE 32

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static const uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static const uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

#if defined(NOT_USED)
struct mbuf_table {
    unsigned len;
    struct rte_mbuf *m_table[MAX_PKT_BURST];
};

struct lcore_queue_conf {
    unsigned n_rx_port;
    unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
    struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
#endif

/* ethernet addresses of ports */
struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
uint32_t enabled_port_mask;

#if defined(NOT_USED)
unsigned int rx_queue_per_lcore;
#endif

    struct rte_mempool *pktmbuf_pool;
    int port;

    // REVISIT: support multiple dpdk ports
    struct rte_eth_conf port_conf;
    struct rte_eth_rxconf rx_conf;
    struct rte_eth_txconf tx_conf;

    // REVISIT: support multiple instance
    static bool instantiated;

    DISALLOW_COPY_AND_ASSIGN(DpdkDriver);
};

} // end RAMCloud

#endif  // RAMCLOUD_DPDKDRIVER_H

/*
 * Local Variables:
 * c-file-style: "ramcloud"
 * End:
 */
