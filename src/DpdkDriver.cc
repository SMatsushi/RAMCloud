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
 * Implementation for #RAMCloud::DpdkDriver, an DPDK packet driver
 * based on InfUdDriver.cc
 * DPDK: http://dpdk.org/
 */

/*
 * Todo:
 *   - add copyright notice
 *   - update the documentation of the methods
 *   - cleanup unused variables, methods
 *
 * Candidates of reworking, refactoring
 *   - throw an exception instead of exit(2) on error
 *   - implements dumpStats(), newAddress(). see Driver.h
 *   - support multiple dpdk ports
 *   - rename environment varialbes to have unified prefix
 *   - set the dpdk rte arguments based on a envrinment variable
 */
#undef NOT_USED

#include <unistd.h>
#include "Common.h"
#include "FastTransport.h"
#include "ShortMacros.h"
#include "ServiceLocator.h"
#if !defined(TIME_ATTACK)
#include "PcapFile.h"
#endif

#include "DpdkDriver.h"

// provide the compiler with branch prediction information.
#ifndef likely
#define   likely(x)  __builtin_expect((x), 1)
#endif
#ifndef unlikely
#define unlikely(x)  __builtin_expect((x), 0)
#endif

#define BITS_PER_HEX 4

namespace RAMCloud {

// REVISIT: support multiple dpdk ports
bool DpdkDriver::instantiated = false;

/**
 * Construct a DpdkDriver.
 *
 * \param context
 *      Overall information about the RAMCloud server or client.
 * \param localServiceLocator
 *      Specifies a particular port on which this driver will listen
 *      for incoming packets.
 */
DpdkDriver::DpdkDriver(Context* context,
        const ServiceLocator* localServiceLocator)
    : context(context)
    , incomingPacketHandler()
    , packetBufPool()
    , packetBufsUtilized(0)
    , localMac()
    , locatorString()
    , poller()
    , enabled_port_mask(0)
#if defined(NOT_USED)
    , rx_queue_per_lcore(1)
#endif
    , pktmbuf_pool(NULL)
    , port(0)
    , port_conf()
    , rx_conf()
    , tx_conf()
{
    // REVISIT
    if (instantiated) {
        LOG(ERROR, "Can't instantiate multiple DpdkDriver class.\n");
        throw;
    }
    instantiated = true;

    if (localServiceLocator != NULL) {
        locatorString = localServiceLocator->getOriginalString();
    }

    // REVISIT: Initialize DPDK: simplify and flexible

    char *coremask = getenv("COREMASK");
    char arg[16+(RTE_MAX_LCORE/BITS_PER_HEX)];

    if (coremask) {
        snprintf(arg, sizeof(arg), "-c %s", coremask);
    } else {
        int ncpus;
        uint64_t mask;
        ncpus = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
        if (ncpus < 1) ncpus = 1;
        mask = ncpus >= RTE_MAX_LCORE ? (uint64_t)-1 : (1ULL<<ncpus)-1;
        snprintf(arg, sizeof(arg), "-c %0*lx",
                 static_cast<int>(RTE_MAX_LCORE/BITS_PER_HEX), mask);
        // REVISIT: the number of online cpus may not be continuous.
    }
    LOG(NOTICE, "DPDK arg: %s", arg);

    // REVISIT: -c coremask -n memory_channels
    const char *argv[] = {"", arg, "-n 4", "--", NULL};
    int argc = static_cast<int>(sizeof(argv)/sizeof(argv[0]));
    int ret = rte_eal_init(argc, const_cast<char **>(argv));
    if (ret < 0) {
        LOG(ERROR, "Invalid EAL arguments\n");
        exit(EXIT_FAILURE);
    }
    // argc -= ret;
    // argv += ret;

#if defined(NOT_USED)
    rx_queue_per_lcore = 1;
#endif

    // REVISIT: currently assume RTE_MAX_ETHPORTS <= 32
    char *portmask = getenv("PORTMASK");
    enabled_port_mask = 1;
    if (portmask) {
        enabled_port_mask = static_cast<uint32_t>(strtoul(portmask, NULL, 16));
        if (enabled_port_mask == 0) {
            LOG(ERROR, "Invalid PORTMASK: %s. assume 1", portmask);
            enabled_port_mask = 1;
        }
    }
    LOG(NOTICE, "portmask: 0x%08x", enabled_port_mask);

    int ports = init_dpdk(enabled_port_mask, ports_eth_addr);
    if (ports < 1) {
        LOG(ERROR, "No DPDK port available\n");
        exit(EXIT_FAILURE);
    }

    // REVISIT:
    // determine the port to use
    char *dpdkport = getenv("DPDKPORT");
    port = 0;
    if (dpdkport) {
        port = static_cast<uint32_t>(strtoul(dpdkport, NULL, 16));
    }
    if ((enabled_port_mask & (1 << port)) == 0) {
        LOG(NOTICE, "Invalid PORTMASK and/or DPDKPORT value\n");
        LOG(NOTICE, "use the lowest port found in PORTMASK\n");
        for (int portid = 0; portid < ports; portid++) {
            // at least 1 port in enabled_port_mask is available.
            if (enabled_port_mask & (1 << portid)) {
                port = portid;
                break;
            }
        }
    }
    // REVISIT: support multiple dpdk ports
    localMac.construct(ports_eth_addr[port].addr_bytes);
    LOG(NOTICE, "port=%d, mac=%s\n", port, localMac->toString().c_str());
}

/**
 * Destroy a DpdkDriver.
 */
DpdkDriver::~DpdkDriver()
{
    if (packetBufsUtilized != 0)
        LOG(ERROR, "DpdkDriver deleted with %d packets still in use",
            packetBufsUtilized);
    // REVISIT: Cleanup DPDK resources
}

/**
 * Invoked by a transport to associate itself with this
 * driver, so that the driver can invoke the transport's
 * incoming packet handler whenever packets arrive.
 * \param incomingPacketHandler
 *      A functor which will be invoked for each incoming packet.
 *      This should be allocated with new and this Driver instance will
 *      take care of deleting it.
 */
void
DpdkDriver::connect(IncomingPacketHandler* incomingPacketHandler)
{
    this->incomingPacketHandler.reset(incomingPacketHandler);
    poller.construct(context, this);
}

/**
 * Breaks the association between this driver and a particular
 * transport instance, if there was one. Once this method has
 * returned incoming packets will be ignored or discarded until
 * #connect is invoked again.
 */
void
DpdkDriver::disconnect()
{
    if (poller)
        poller.destroy();
    this->incomingPacketHandler.reset();
}

/**
 * The maximum number of bytes this Driver can transmit in a single call
 * to sendPacket including both header and payload.
 */
uint32_t
DpdkDriver::getMaxPacketSize()
{
    return MAX_PAYLOAD_SIZE;
}

#if defined(NOT_USED)
/**
 * Return a free transmit buffer
 */
struct rte_mbuf *
DpdkDriver::getTransmitBuffer()
{
    rte_mbuf *m = rte_pktmbuf_alloc(pktmbuf_pool);
    if (unlikely(m == NULL)) {
        LOG(ERROR, "rte_pktmbuf_alloc() failed\n");
        exit(EXIT_FAILURE);
    }
    return m;
}
#endif

/**
 * Invoked by a transport when it has finished processing the data
 * in an incoming packet; used by drivers to recycle packet buffers
 * at a safe time.
 *
 * \param payload
 *      The payload field from the Received object used to pass the
 *      packet to the transport when it was received.
 */
void
DpdkDriver::release(char *payload)
{
    // Must sync with the dispatch thread, since this method could potentially
    // be invoked in a worker.
    Dispatch::Lock _(context->dispatch);

    // Note: the payload is actually contained in a PacketBuf structure,
    // which we return to a pool for reuse later.
    packetBufsUtilized--;
    assert(packetBufsUtilized >= 0);
    packetBufPool.destroy(
        reinterpret_cast<PacketBuf*>(payload - OFFSET_OF(PacketBuf, payload)));
}


/**
 * Send a single packet out over this Driver. The method doesn't return
 * until header and payload have been read and the packet is "on the wire";
 * the caller can safely discard or reuse the memory associated with payload
 * and header once the method returns.  If an error occurs, this method
 * will log the error and return without sending anything; this method
 * does not throw exceptions.
 *
 * header provides a means to slip data onto the front of the packet
 * without having to pay for a prepend to the Buffer containing the
 * packet payload data.
 *
 * \param recipient
 *      The address the packet should go to.
 * \param header
 *      Bytes placed in the packet ahead of those from payload.
 * \param headerLen
 *      Length in bytes of the data in header.
 * \param payload
 *      A buffer iterator positioned at the bytes for the payload to
 *      follow the headerLen bytes from header.  May be NULL to
 *      indicate "no payload".
 */
void
DpdkDriver::sendPacket(const Address *recipient,
                      const void *header,
                      uint32_t headerLen,
                      Buffer::Iterator *payload)
{
    uint32_t totalLength = headerLen +
                           (payload ? payload->size() : 0);
    assert(totalLength <= MAX_PAYLOAD_SIZE);


    rte_mbuf *pkt = rte_pktmbuf_alloc(pktmbuf_pool);
    if (unlikely(pkt == NULL)) {
        LOG(ERROR, "rte_pktmbuf_alloc() failed\n");
        exit(EXIT_FAILURE);
    }
    char *data = rte_pktmbuf_mtod(pkt, char *);
    char *p = data;

    auto& ethHdr = *new(p) EthernetHeader;
    memcpy(ethHdr.destAddress,
           static_cast<const MacAddress*>(recipient)->address, 6);
    // REVISIT: switch multiple mac address according to dpdk port
    memcpy(ethHdr.sourceAddress, localMac->address, 6);
    ethHdr.etherType = HTONS(0x8001);
    ethHdr.length = downCast<uint16_t>(totalLength);
    p += sizeof(ethHdr);

    memcpy(p, header, headerLen);
    p += headerLen;
    while (payload && !payload->isDone()) {
        memcpy(p, payload->getData(), payload->getLength());
        p += payload->getLength();
        payload->next();
    }
    uint32_t length = static_cast<uint32_t>(p - data);

    pkt->data_len = static_cast<uint16_t>(length);  // REVISIT
    pkt->pkt_len  = length;  // REVISIT
#if !defined(TIME_ATTACK)
    rte_mbuf_sanity_check(pkt, 1);

    if (unlikely(pcapFile))
        pcapFile->append(data, length);
#endif

    if (unlikely(rte_eth_tx_burst((uint8_t)port, 0, &pkt, 1) == 0)) {
        LOG(ERROR, "rte_eth_tx_burst() failed\n");
        exit(EXIT_FAILURE);  // REVISIT
    }
}


// Dispatch::Polloer.poll()
int
DpdkDriver::Poller::poll()
{
    assert(driver->context->dispatch->isDispatchThread());

    rte_mbuf *pkt = NULL;

    // following unlikely() seems to be inverse condition against the real world
    // situation, but when receive any packet, hit the branch prediction
    // and results in better performance for receiving packets.
    if (unlikely(rte_eth_rx_burst((uint8_t)driver->port, 0, &pkt, 1) == 0)) {
        return 0;
    }

    char *data = rte_pktmbuf_mtod(pkt, char *);
#if !defined(TIME_ATTACK)
    if (unlikely(pcapFile))
        pcapFile->append(data, pkt->pkt_len);

    if (unlikely(pkt->pkt_len < 60)) {
        LOG(ERROR, "received impossibly short packet!");
        rte_pktmbuf_free(pkt);
        return 1;
    }
#endif

    PacketBuf* buffer = driver->packetBufPool.construct();

    auto& ethHdr = *reinterpret_cast<EthernetHeader*>(data);
    Received received;
    MacAddress srcmac(ethHdr.sourceAddress);

    if (unlikely(ethHdr.etherType != HTONS(0x8001))) {
        LOG(DEBUG, "unknown ether type: %x\n", NTOHS(ethHdr.etherType));
        driver->packetBufPool.destroy(buffer);
        rte_pktmbuf_free(pkt);
        return 1;
    }

    received.driver = driver;
    received.payload = buffer->payload;
    received.sender = &srcmac;
    received.len = ethHdr.length;  // payload length

#if !defined(TIME_ATTACK)
    if (unlikely(received.len + sizeof(ethHdr) > pkt->pkt_len)) {
        LOG(ERROR, "corrupt packet");
        driver->packetBufPool.destroy(buffer);
        rte_pktmbuf_free(pkt);
        return 1;
    }
#endif

    // copy from the dpdk buffer into our dynamically allocated buffer.
    memcpy(received.payload, data + sizeof(ethHdr), received.len);

    driver->packetBufsUtilized++;

    (*driver->incomingPacketHandler)(&received);

    // post the original dpdk buffer back to the receive queue
    rte_pktmbuf_free(pkt);

    return 1;
}

/**
 * Return the ServiceLocator for this Driver. If the Driver
 * was not provided static parameters (e.g. fixed TCP or UDP port),
 * this function will return a SerivceLocator with those dynamically
 * allocated attributes.
 *
 * Enlisting the dynamic ServiceLocator with the Coordinator permits
 * other hosts to contact dynamically addressed services.
 */
string
DpdkDriver::getServiceLocator()
{
    return locatorString;
}

#if defined(NOT_USED)
/**
 * Try to receive a message from the given Queue Pair if one
 * is available. Do not block.
 *
 * \param[in] qp
 *      The queue pair to poll for a received message.
 * \param[in] sourceAddress
 *      Optional. If not NULL, store the sender's address here.
 * \return
 *      NULL if no message is available. Otherwise, a pointer to
 *      a BufferDescriptor containing the message.
 * \throw TransportException
 *      if polling failed.
 */
rte_mbuf *
DpdkDriver::tryReceive(int portid)
{
    struct rte_mbuf *m;
    int nb_rx = rte_eth_rx_burst((uint8_t)portid, 0, &m, 1);
    if (likely(nb_rx == 0)) {
        return NULL;
    }
    return m;
}

/**
 * Add the given BufferDescriptor to the receive queue for the given
 * QueuePair.
 *
 * \param[in] bd
 *      The BufferDescriptor to enqueue.
 */
void
DpdkDriver::postReceive(rte_mbuf *pkt)
{
    rte_pktmbuf_free(pkt);
}

/**
 * Asychronously transmit the packet described by 'bd' on queue pair 'qp'.
 * This function returns immediately.
 *
 * \param[in] qp
 *      The QueuePair on which to transmit the packet.
 * \param[in] bd
 *      The BufferDescriptor that contains the data to be transmitted.
 * \param[in] length
 *      The number of bytes used by the packet in the given BufferDescriptor.
 * \param[in] address
 *      UD queue pairs only. The address of the host to send to.
 * \param[in] remoteQKey
 *      UD queue pairs only. The Q_Key of the remote pair to send to.
 * \throw TransportException
 *      if the send post fails.
 */
void
DpdkDriver::postSend(int port, rte_mbuf *pkt)
{
    int ret = rte_eth_tx_burst((uint8_t)port, 0, &pkt, 1);
    if (unlikely(ret == 0)) {
        LOG(ERROR, "rte_eth_tx_burst() failed\n");
        exit(EXIT_FAILURE);  // REVISIT
        // throw TransportException(HERE, "rte_eth_tx_burst() failed");
    }
}
#endif

char *
DpdkDriver::eth2str(struct ether_addr *addr)
{
#define ADDRFMT  "%02X:%02X:%02X:%02X:%02X:%02X"
#define ADDRSIZE (2*ETHER_ADDR_LEN+1*(ETHER_ADDR_LEN-1)+1) // 2*6 + sep*5 + nul
  static char buf[ADDRSIZE];

  snprintf(buf, ADDRSIZE, ADDRFMT,
           addr->addr_bytes[0], addr->addr_bytes[1], addr->addr_bytes[2],
           addr->addr_bytes[3], addr->addr_bytes[4], addr->addr_bytes[5]);
  return buf;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
    uint8_t portid, count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;

    LOG(NOTICE, "\nChecking link status\n");
    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        all_ports_up = 1;
        for (portid = 0; portid < port_num; portid++) {
            if ((port_mask & (1 << portid)) == 0)
                continue;
            memset(&link, 0, sizeof(link));
            rte_eth_link_get_nowait(portid, &link);
            /* print link status if flag set */
            if (print_flag == 1) {
                if (link.link_status)
                    LOG(NOTICE, "Port %d Link Up - speed %u "
                           "Mbps - %s\n", (uint8_t)portid,
                           (unsigned)link.link_speed,
                           (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                           ("full-duplex") : ("half-duplex\n"));
                else
                    LOG(NOTICE, "Port %d Link Down\n",
                           (uint8_t)portid);
                continue;
            }
            /* clear all_ports_up flag if any link down */
            if (link.link_status == 0) {
                all_ports_up = 0;
                break;
            }
        }
        /* after finally printing all link status, get out */
        if (print_flag == 1)
            break;

        if (all_ports_up == 0) {
            rte_delay_ms(CHECK_INTERVAL);
        }

        /* set the print_flag if all ports up or timeout */
        if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
            print_flag = 1;
            LOG(NOTICE, "done\n");
        }
    }
}

int
DpdkDriver::init_dpdk(uint32_t port_mask, struct ether_addr *eth_addr)
{
  int ret, portid;
  uint8_t nb_ports, nb_ports_available;
#if defined(NOT_USED)
  unsigned rx_lcore_id;
  struct lcore_queue_conf *qconf;
  struct rte_eth_dev_info dev_info;
#endif

  /* create the mbuf pool */
  pktmbuf_pool = rte_mempool_create("mbuf_pool", NB_MBUF, MBUF_SIZE, CACHE_SIZE,
                                    sizeof(struct rte_pktmbuf_pool_private),
                                    rte_pktmbuf_pool_init, NULL,
#if 0
// #if defined(TIME_ATTACK)
// deprecated
                                    rte_pktmbuf_init_nc, NULL,
#else
                                    rte_pktmbuf_init, NULL,
#endif
                                    rte_socket_id(), 0);
  if (pktmbuf_pool == NULL) {
      LOG(ERROR, "Cannot init mbuf pool\n");
      exit(EXIT_FAILURE);
  }

  nb_ports = rte_eth_dev_count();
  if (nb_ports == 0) {
      LOG(ERROR, "No Ethernet ports - bye\n");
      exit(EXIT_FAILURE);
  }
  if (nb_ports > RTE_MAX_ETHPORTS) {
    nb_ports = RTE_MAX_ETHPORTS;
  }

#if defined(NOT_USED)
  for (portid = 0; portid < nb_ports; portid++) {
    /* skip ports that are not enabled */
    if ((port_mask & (1 << portid)) == 0) {
      continue;
    }
    // REVISIT: dev_info not used. ???
    rte_eth_dev_info_get((uint8_t)portid, &dev_info);
  }

  rx_lcore_id = 0;

  /* Initialize the port/queue configuration of each logical core */
  for (portid = 0; portid < nb_ports; portid++) {
    /* skip ports that are not enabled */
    if ((port_mask & (1 << portid)) == 0) {
      continue;
    }

    /* get the lcore_id for this port */
    while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
           lcore_queue_conf[rx_lcore_id].n_rx_port == rx_queue_per_lcore) {
      rx_lcore_id++;
      if (rx_lcore_id >= RTE_MAX_LCORE) {
          LOG(ERROR, "Not enough cores\n");
          exit(EXIT_FAILURE);
      }
    }
    /* Assigned a new logical core in the loop above. */
    qconf = &lcore_queue_conf[rx_lcore_id];
    qconf->rx_port_list[qconf->n_rx_port] = portid;
    qconf->n_rx_port++;
    LOG(NOTICE, "Lcore %u: RX port %u\n", rx_lcore_id, (unsigned)portid);
  }
#endif

  nb_ports_available = nb_ports;

  // REVISIT: support multiple dpdk ports
  /* initialize the port configuration parameters */
  port_conf.rxmode.split_hdr_size = 0;
  port_conf.rxmode.header_split   = 0; /* Header split disabled */
  port_conf.rxmode.hw_ip_checksum = 0; /* IP checksum offload disabled */
  port_conf.rxmode.hw_vlan_filter = 0; /* VLAN filtering disabled */
  port_conf.rxmode.jumbo_frame    = 0; /* Jumbo frame support disabled */
  port_conf.rxmode.hw_strip_crc   = 0; /* CRC stripped by hardware */
  port_conf.txmode.mq_mode = ETH_MQ_TX_NONE;

  /* initialize the rx queue configuration parameters */
  rx_conf.rx_thresh.pthresh = RX_PTHRESH; /* rx prefetch threshold reg */
  rx_conf.rx_thresh.hthresh = RX_HTHRESH; /* rx host threshold reg */
  rx_conf.rx_thresh.wthresh = RX_WTHRESH; /* rx write-back threshold reg */

  /* initialize the tx queue configuration parameters */
  tx_conf.tx_thresh.pthresh = TX_PTHRESH; /* tx prefetch threshold reg */
  tx_conf.tx_thresh.hthresh = TX_HTHRESH; /* tx host threshold reg */
  tx_conf.tx_thresh.wthresh = TX_WTHRESH; /* tx write-back threshold reg */
  tx_conf.tx_free_thresh    = 0;
  tx_conf.tx_rs_thresh      = 0;
  /* won't handle mult-segments and offload cases */
  tx_conf.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOOFFLOADS;

  // REVISIT: make check complains of using scanf.
  // should rewrite using strtok(3) and strtoul(3) or something,
  // but it's too much of bother. so just commented out for the momemnt.
  /* retrieve the queue params from QCONF environment variable if defined. */
  /*
  char *queue_params;
  if ((queue_params = getenv("QCONF")) != NULL) {
      uint32_t rxp, rxh, rxw, txp, txh, txw, txf, txr;
      LOG(NOTICE, "QCONF=%s", queue_params);
      if (sscanf(queue_params, "%u,%u,%u,%u,%u,%u,%u,%u",
                 &rxp, &rxh, &rxw, &txp, &txh, &txw, &txf, &txr) == 8) {
          rx_conf.rx_thresh.pthresh = (uint8_t)rxp;
          rx_conf.rx_thresh.hthresh = (uint8_t)rxh;
          rx_conf.rx_thresh.wthresh = (uint8_t)rxw;
          tx_conf.tx_thresh.pthresh = (uint8_t)txp;
          tx_conf.tx_thresh.hthresh = (uint8_t)txh;
          tx_conf.tx_thresh.wthresh = (uint8_t)txw;
          tx_conf.tx_free_thresh    = (uint16_t)txf;
          tx_conf.tx_rs_thresh      = (uint16_t)txr;
      } else {
          LOG(ERROR, "Invalid QCONF value %s. use default.", queue_params);
      }
  }
  */

  LOG(NOTICE, "rx_thresh (prefetch, host, writeback) = %u, %u, %u",
      rx_conf.rx_thresh.pthresh,
      rx_conf.rx_thresh.hthresh,
      rx_conf.rx_thresh.wthresh);
  LOG(NOTICE, "tx_thresh (prefetch, host, writeback, free, rs) = "
      "%u, %u, %u, %u, %u",
      tx_conf.tx_thresh.pthresh,
      tx_conf.tx_thresh.hthresh,
      tx_conf.tx_thresh.wthresh,
      tx_conf.tx_free_thresh,
      tx_conf.tx_rs_thresh);

  /* Initialise each port */
  for (portid = 0; portid < nb_ports; portid++) {
    /* skip ports that are not enabled */
    if ((port_mask & (1 << portid)) == 0) {
      LOG(NOTICE, "Skipping disabled port %u\n", (unsigned) portid);
      nb_ports_available--;
      continue;
    }

    /* init port */
    LOG(NOTICE, "Initializing port %u... ", (unsigned) portid);

    ret = rte_eth_dev_configure((uint8_t)portid, 1, 1, &port_conf);
    if (ret < 0) {
        LOG(ERROR, "Cannot configure device: err=%d, port=%u\n",
            ret, (unsigned) portid);
        exit(EXIT_FAILURE);
    }
    rte_eth_macaddr_get((uint8_t)portid, &eth_addr[portid]);

    // REVISIT:
    //  check whether the macaddr of portid and the ethaddr of the
    //  service locator are matched.

    /* init one RX queue */
    ret = rte_eth_rx_queue_setup((uint8_t)portid, 0, nb_rxd,
                                 rte_eth_dev_socket_id((uint8_t)portid),
                                 &rx_conf, pktmbuf_pool);
    if (ret < 0) {
        LOG(ERROR, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
            ret, (unsigned) portid);
        exit(EXIT_FAILURE);
    }

    /* init one TX queue */
    ret = rte_eth_tx_queue_setup((uint8_t)portid, 0, nb_txd,
                                 rte_eth_dev_socket_id((uint8_t)portid),
                                 &tx_conf);
    if (ret < 0) {
        LOG(ERROR, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
            ret, (unsigned) portid);
        exit(EXIT_FAILURE);
    }

    /* Start device */
    ret = rte_eth_dev_start((uint8_t)portid);
    if (ret < 0) {
        LOG(ERROR, "rte_eth_dev_start:err=%d, port=%u\n",
            ret, (unsigned) portid);
        exit(EXIT_FAILURE);
    }

    LOG(NOTICE, "done: \n");

    // REVISIT:
    // rte_eth_promiscuous_enable(portid);

    LOG(NOTICE, "Port %u, MAC address: %s\n\n",
        (unsigned) portid, eth2str(&eth_addr[portid]));

  }

  if (!nb_ports_available) {
      LOG(ERROR, "All available ports are disabled. Please set portmask.\n");
      exit(EXIT_FAILURE);
  }

  check_all_ports_link_status(nb_ports, port_mask);

#if defined(NOT_USED)
  LOG(NOTICE, "portmask=%u, rxq/core=%u\n", port_mask, rx_queue_per_lcore);
#endif

  return nb_ports;
}


} // namespace RAMCloud

/*
 * Local Variables:
 * c-file-style: "ramcloud"
 * End:
 */
