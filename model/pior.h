/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Janaka Wijekoon, Hiroaki Nishi Laboratory, Keio University, Japan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Janaka Wijekoon <janaka@west.sd.ekio.ac.jp>, Hiroaki Nishi <west@sd.keio.ac.jp>
 */

#ifndef PIO_H
#define PIO_H

#include <cassert>
#include <list>
#include <sys/types.h>

//#include "ns3/pio-header.h"

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"

#include "ns3/random-variable-stream.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/timer.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {

/**
 * \defgroup PIO Distance Vector Routing Protocol.
 *
 */ 

#define PIO_PORT 272
#define PIO_LISTEN_PORT 273

/**
 * Split Horizon strategy type.
 */
enum SplitHorizonType {
  NO_SPLIT_HORIZON,//!< No Split Horizon
  SPLIT_HORIZON,   //!< Split Horizon
  POISON_REVERSE,  //!< Poison Reverse Split Horizon
};

/**
 * Printing Options.
 */
enum PrintingOption {
  DONT_PRINT, //!< Do not print any table (Default state)
  MAIN_R_TABLE, //!< Print the main routing table
  N_TABLE, //!< Print the neighbor table
};

/**
 * Set the validity of both route and neighbor records 
 */
enum Validity {
  VALID, //!< route and neighbor records are valid
  INVALID, //!< route and neighbor records are invalid
  LHOST, //!< indicate that the route is the local host
};

/**
 * Set the Update type
 */
enum UpdateType
{
  PERIODIC, //!< Periodic Update
  TRIGGERED, //!< Triggered Update
};


/**
  * \ingroup PIO
  * \brief PIO Routing Table Entry
 */
class PIORoutingEntry : public Ipv4RoutingTableEntry
{
public:
  PIORoutingEntry (void);

  /**
   * \brief Constructor
   * \param network network address
   * \param networkMask network mask of the given destination network
   * \param nextHop next hop address to route the packet
   * \param interface interface index
   */
  PIORoutingEntry (Ipv4Address network = Ipv4Address (), 
                     Ipv4Mask networkMask = Ipv4Mask (), 
                     Ipv4Address nextHop = Ipv4Address (), 
                     uint32_t interface = 0);

  /**
   * \brief Constructor
   * \param network network address
   * \param networkMask network mask of the given destination network
   * \param interface interface index
   */
  PIORoutingEntry (Ipv4Address network = Ipv4Address (), 
                     Ipv4Mask networkMask = Ipv4Mask (),
                     uint32_t interface = 0);

  /**
   * \brief Constructor for creating a host route
   * \param host server's IP address
   * \param interface connected interface
   */
  PIORoutingEntry (Ipv4Address host = Ipv4Address (),
                     uint32_t interface = 0);

  virtual ~PIORoutingEntry ();

  /**
  * \brief Get and Set Sequence Number of the route record
  * \param sequenceNumber the sequence number of the route record
  * \returns the sequence number of the route record
  */
  uint16_t GetSequenceNo (void) const
  {
    return m_sequenceNo;
  }
  void SetSequenceNo (uint16_t sequenceNo)
  {
    m_sequenceNo = sequenceNo;
  }

  /**
  * \brief Get and Set metric 
  * the metric is the hop count to the destination network
  * \param metric hop count
  * \returns the hop count
  */
  uint16_t GetMetric (void) const
  {
    return m_metric;
  }
  void SetMetric (uint16_t metric)
  {
    m_metric = metric;
  }

  /**
  * \brief Get and Set route's status
  * The changed routes are scheduled for a triggered update.
  * After a Triggered/periodic update, changed flag is set to zero.
  *
  * \param changed true if the route is changed
  * \returns true if the route is changed
  */
  bool GetRouteChanged (void) const
  {
    return m_changed;
  }
  void SetRouteChanged (bool changed)
  {
    m_changed = changed;
  }

  /**
  * \brief Get and Set the route's status 
  * the route's validity is changed according to the expirationtime.
  * all new routes are first with status VALID.
  * all INVALID routes are deleted after the garbage collection time
  *
  * \param validity the status of the route
  * \returns the status of the route record
  */
  Validity GetValidity () const
  {
    return Validity (m_validity);
  }
  void SetValidity (Validity validity)
  {
    m_validity = validity;
  }

private:
  uint16_t m_sequenceNo; //!< sequence number of the route record
  uint16_t m_metric; //!< route metric
  bool m_changed; //!< route has been updated
  Validity m_validity; //!< validity of the routing record
}; // PIO Routing Table Entry

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<< (std::ostream& os, PIORoutingEntry const& route);

/**
 * \ingroup PIO
 *
 * \brief the PIO protocol management methods and vriables.
 */
class PIORoutingProtocol : public Ipv4RoutingProtocol
{
public:
  PIORoutingProtocol ();
  virtual ~PIORoutingProtocol ();

  /**
   * \brief Get the type ID
   * \return type ID
   */
  static TypeId GetTypeId (void);

  // \name From Ipv4RoutingProtocol
  // \{
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb, 
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const;
  // \}

  bool IsInitialized();

  /**
  * \brief look up for a forwarding route in the routing table.
  *
  * \param address destination address
  * \param dev output net-device if any (assigned 0 otherwise)
  * \return Ipv4Route where that the given packet has to be forwarded 
  */
  Ptr<Ipv4Route> LookupRoute (Ipv4Address address, Ptr<NetDevice> dev = 0);

  /**
   * \brief Add a default route to the router.
   *
   * The default route is usually installed manually, or it is the result of
   * some "other" routing protocol (e.g., BGP).
   *
   * \param nextHop the next hop
   * \param interface the interface
   */
  void AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface);

  /**
   * \brief Add route to network where the gateway address is known.
   * \param network network address
   * \param networkMask network prefix
   * \param nextHop next hop address to route the packet.
   * \param interface interface index
   * \param metric the cumulative hop count to the destination network
   * \param sequenceNo sequence number of the received route
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route has to be removeded from the table
   */
  void AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime);

  /**
   * \brief Add route to network where the gateway is not needed. Such routes are usefull to add
   * routes about the localy connected networks.
   * \param network network address
   * \param networkMask network prefix
   * \param interface interface index
   * \param metric the cumulative hop count to the destination network
   * \param sequenceNo sequence number of the received route
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route has to be removeded from the table
   */
  void AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime);

  /**
   * \brief Add route to a host.
   * \param network network address
   * \param interface interface index
   * \param metric the cumulative hop count to the destination network
   * \param sequenceNo sequence number of the received route
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route has to be removeded from the table
   */
  void AddHostRouteTo (Ipv4Address host, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime);

protected:
  /**
   * \brief Dispose this object.
   */
  virtual void DoDispose ();

  /**
   * Start protocol operation
   */
  void DoInitialize ();
private:

  // \name for route management
  // \{
  /// Container for a Route table entry 
  typedef std::pair <PIORoutingEntry*, EventId> RouteTableRecord;
  
  /// Container for an instance of the neighbor table
  typedef std::list<std::pair <PIORoutingEntry*, EventId> > RoutingTableInstance;

  /// Iterator for the Neighbor table entry container
  typedef std::list<std::pair <PIORoutingEntry*, EventId> >::iterator RoutesI;

  /// Constant Iterator for the Neighbor table entry container
  typedef std::list<std::pair <PIORoutingEntry*, EventId> >::const_iterator RoutesCI;

  /**
   * \brief Invalidate a route.
   * \param route the route to be removed
   */
  void InvalidateRoute (PIORoutingEntry *route);

  /**
   * \brief Delete a route.
   * \param route the route to be removed
   */
  void DeleteRoute (PIORoutingEntry *route);

  /**
   * \brief Invalidate routes for a given interface.
   * \param interface the route to be removed
   * \return true if found a route
   */  
  bool InvalidateRoutesForInterface (uint32_t interface);

  /**
   * \brief Invalidate broken routes.
   * broken routes are separated using the sequence number. All ODD valued sequence numbers indicate that the route is
   * is a broken route
   * \param destination destination network
   * \param destinationMask mask of the destination network
   * \return true if found route(s)
   */  
  bool InvalidateBrokenRoutes(Ipv4Address destination, Ipv4Mask destinationMask);

  /**
   * \brief Once a neighbor record is invalidated, all routes that has the gateway as the invalidated neighbor is also
   * invalidated
   * \param gateway neighbor address
   * \return true if found route(s)
   */  
  bool InvalidateRoutesForGateway (Ipv4Address gateway);

  /**
   * \brief check for the locally connected networks.
   * \param address network address
   * \param mask mask of the network
   * \return true if found route(s)
   */

  bool IsLocalRouteAvailable (Ipv4Address address, Ipv4Mask mask);

  /**
   * \Find and return a route record for given network and mask pair.
   * \param address network address
   * \param mask mask of the network
   * \return true and the found route record
   */
  bool FindRouteRecord (Ipv4Address address, Ipv4Mask mask, RoutesI &foundRoute);


  RoutingTableInstance m_routing;
  Ptr<Ipv4> m_ipv4; //!< IPv4 reference  
  bool m_initialized; //!< flag that indicates the protocol is already initialized.
  Ptr<UniformRandomVariable> m_rng; //!< Rng stream.
  
  std::set<uint32_t> m_interfaceExclusions; //!< Set of excluded interfaces
  
  PrintingOption m_print; //!< Printing Type
  SplitHorizonType m_splitHorizonStrategy; //!< Split Horizon strategy

  
  EventId m_nextPeriodicUpdate; //!< Next periodic update event
  EventId m_nextTriggeredUpdate; //!< Next triggered update event

  Time m_startupDelay; //!< Random delay before protocol start-up.  
  Time m_minTriggeredCooldownDelay; //!< minimum cool-down delay between two triggered updates
  Time m_maxTriggeredCooldownDelay; //!< maximum cool-down delay between two triggered updates
  Time m_periodicUpdateDelay; //!< delay between two periodic updates
  Time m_routeTimeoutDelay; //!< delay that a route will be available as a VALID route

  // note: Since the result of socket->GetBoundNetDevice ()->GetIfIndex () is ambiguity and 
  // it depends on the interface initialization (i.e., if the loopback is already up), the socket
  // list is manual created in this implementation.
  // At the time neighbors are discovered, <socket, interface> is then added to the relevant neighbor.
  
  /// Socket list type
  typedef std::map< Ptr<Socket>, uint32_t> SocketList;
  /// Socket list type iterator
  typedef std::map<Ptr<Socket>, uint32_t>::iterator SocketListI;
  /// Socket list type const iterator
  typedef std::map<Ptr<Socket>, uint32_t>::const_iterator SocketListCI;

  SocketList m_sendSocketList; //!< list of sockets (socket, interface index)
  Ptr<Socket> m_recvSocket; //!< receive socket

  // \}

  /**
  * \brief return if the neighboer table is empty or not
  */
  bool IsEmpty(void);

  Time m_kamTimer; //!< time between two keep alive messages 
  Time m_neighborTimeoutDelay; //!< Delay that determines the neighbor is UNRESPONSIVE
  Time m_garbageCollectionDelay; //!< Delay before remove UNRESPONSIVE route/neighbor record
  
  EventId m_nextKeepAliveMessage; //!< next Keep Alive Message event
  // \}
}; // PIO Routing Protocol
}
#endif /* PIO_H */

