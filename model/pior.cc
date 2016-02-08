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
* Author: Piotr Lechowicz <piotr.lechowicz@nokia.com>

Level	Meaning
LOG_LEVEL_ERROR	Only LOG_ERROR severity class messages.
LOG_LEVEL_WARN	LOG_WARN and above.
LOG_LEVEL_DEBUG	LOG_DEBUG and above.
LOG_LEVEL_INFO	LOG_INFO and above.
LOG_LEVEL_FUNCTION	LOG_FUNCTION and above.
LOG_LEVEL_LOGIC	LOG_LOGIC and above.
LOG_LEVEL_ALL	All severity classes.
LOG_ALL	Synonym for LOG_LEVEL_ALL
*/
#include <iomanip>

#include "pior.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/assert.h"
#include "ns3/unused.h"
#include "ns3/random-variable-stream.h"
#include "ns3/node.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/timer.h"
#include "ns3/ipv4-packet-info-tag.h"

NS_LOG_COMPONENT_DEFINE ("PIORoutingProtocol");

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED (PIORoutingProtocol);
/* 
* my Routing Protocol
*/
PIORoutingProtocol::PIORoutingProtocol() :  m_ipv4 (0),
                                              m_initialized (false)
{
  m_rng = CreateObject<UniformRandomVariable> ();
}

PIORoutingProtocol::~PIORoutingProtocol () {/*destructor*/}

TypeId PIORoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PIORoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<PIORoutingProtocol> ()
    .AddAttribute ( "KeepAliveInterval","The time between two Keep Alive Messages.",
			              TimeValue (Seconds(30)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_kamTimer),
			              MakeTimeChecker ())
    .AddAttribute ( "NeighborTimeoutDelay","The delay to mark a neighbor as unresponsive.",
			              TimeValue (Seconds(60)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_neighborTimeoutDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "GarbageCollection","The delay to remove unresponsive neighbors from the neighbor table.",
			              TimeValue (Seconds(10)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_garbageCollectionDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "StartupDelay", "Maximum random delay for protocol startup.",
                    TimeValue (Seconds(1)),
                    MakeTimeAccessor (&PIORoutingProtocol::m_startupDelay),
                    MakeTimeChecker ())
    .AddAttribute ( "SplitHorizon", "Split Horizon strategy.",
                    EnumValue (SPLIT_HORIZON),
                    MakeEnumAccessor (&PIORoutingProtocol::m_splitHorizonStrategy),
                    MakeEnumChecker (NO_SPLIT_HORIZON, "NoSplitHorizon",
                                      SPLIT_HORIZON, "SplitHorizon",
                                      POISON_REVERSE, "PoisonReverse"))
    .AddAttribute ( "RouteTimeoutDelay","The delay to mark a route as invalidate.",
			              TimeValue (Seconds(180)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_routeTimeoutDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "MinTriggeredCooldown","Minimum time gap between two triggered updates.",
			              TimeValue (Seconds(1)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_minTriggeredCooldownDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "MaxTriggeredCooldown","Maximum time gap between two triggered updates.",
			              TimeValue (Seconds(5)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_maxTriggeredCooldownDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "PeriodicUpdateInterval","Duration between two periodic updates.",
			              TimeValue (Seconds(30)), /*This has to be adjust according to the user's requirment*/
			              MakeTimeAccessor (&PIORoutingProtocol::m_periodicUpdateDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "PrintingMethod", "Specify which table has to be print.",
                    EnumValue (DONT_PRINT),
                    MakeEnumAccessor (&PIORoutingProtocol::m_print),
                    MakeEnumChecker ( MAIN_R_TABLE, "MainRoutingTable",
                                      N_TABLE, "NeighborTable"))
  ;
  return tid;
}

bool
PIORoutingProtocol::IsInitialized()
{
  return m_initialized;
}

void 
PIORoutingProtocol::DoInitialize ()
{
  NS_LOG_FUNCTION (this);

  //bool addedGlobal = false;

  m_initialized = true;

  Ipv4RoutingProtocol::DoInitialize ();

  int n = m_ipv4->GetObject<Node>()->GetId();
  NS_LOG_LOGIC ("DoInitialize: node=" << n);

  if (n == 2)
    {
      AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), 1, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), 3, 0, 2, Seconds (0), Seconds (0));

      //AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("15.16.16.2"), 2, 2, 4, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("203.15.19.2"), 3, 3, 4, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), Ipv4Address ("15.16.16.2"), 2, 2, 4, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), Ipv4Address ("15.16.16.2"), 2, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), Ipv4Address ("15.16.16.2"), 2, 1, 2, Seconds (500), Seconds (500));

      //AddHostRouteTo (Ipv4Address host, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime)
      //                                                              Ipv4Address nextHop,
      //AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime)
      //AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface)
      //AddNetworkRouteTo (Ipv4Address ("0.0.0.0"), Ipv4Mask::GetZero (), nextHop, interface, 0, 0, Seconds (0), Seconds (0));
      //AddNetworkRouteTo (ifaceNetworkAddress, ifaceNetMask, interface, 0, 0, Seconds (0), Seconds (0));
      //AddNetworkRouteTo (Ipv4Address ("193.168.16.0"), Ipv4Mask ("/30"), 1, 42, 8, Seconds (500), Seconds (500));
    }
  else if (n == 3)
    {
      AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), 1, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), 3, 0, 2, Seconds (0), Seconds (0));

      AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), Ipv4Address ("11.118.126.2"), 3, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), Ipv4Address ("11.118.126.2"), 3, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("201.13.15.2"), 2, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), Ipv4Address ("15.16.16.1"), 1, 1, 2, Seconds (500), Seconds (500));
    }
  else if (n == 4)
    {
      AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), 1, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), 3, 0, 2, Seconds (0), Seconds (0));

      AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("10.10.10.2"), 2, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), Ipv4Address ("10.10.10.2"), 2, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), Ipv4Address ("203.15.19.1"), 1, 1, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), Ipv4Address ("203.15.19.1"), 1, 1, 2, Seconds (500), Seconds (500));
    }
  else if (n == 5)
    {
      AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), 1, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), 3, 0, 2, Seconds (0), Seconds (0));

      AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), Ipv4Address ("201.13.15.1"), 1, 1, 4, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), Ipv4Address ("201.13.15.1"), 1, 1, 4, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), Ipv4Address ("201.13.15.1"), 1, 2, 2, Seconds (500), Seconds (500));
      AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), Ipv4Address ("201.13.15.1"), 1, 2, 2, Seconds (500), Seconds (500));
    }
}

void 
PIORoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
}

void 
PIORoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
}

void 
PIORoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << interface << " address " << address);
}

void
PIORoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << interface << " address " << address);
}

void 
PIORoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);

  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
  
  uint32_t i = 0;
  m_ipv4 = ipv4;

  for (i = 0; i < m_ipv4->GetNInterfaces (); i++)
  {
    if (m_ipv4->IsUp (i))
    {
      NotifyInterfaceUp (i);
    }
    else
    {
      NotifyInterfaceDown (i);
    }
  }
}

void 
PIORoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
{
  NS_LOG_FUNCTION (this << stream);

  std::ostream* os = stream->GetStream ();

	if (m_print == N_TABLE)
	{
    NS_LOG_LOGIC ("PIO: printing the neighbor table");
            	
  	*os << "Node: " << GetObject<Node> ()->GetId ()
    	  << " Time: " << Simulator::Now ().GetSeconds () << "s "
     		<< "PIO Neighbor Table" << '\n';
        //PrintNeighborTable (stream);
	}
	else if (m_print == MAIN_R_TABLE)
	{
    NS_LOG_LOGIC ("PIO: printing the routing table");
    
		*os << "Node: " << GetObject<Node> ()->GetId ()
    	  << " Time: " << Simulator::Now ().GetSeconds () << "s "
     		<< "PIO Routing Table" << '\n';

    *os << "Destination         Gateway          If  Seq#    Metric  Validity Changed Expire in (s)" << '\n';
    *os << "------------------  ---------------  --  ------  ------  -------- ------- -------------" << '\n';

    for (RoutesCI it = m_routing.begin ();  it!= m_routing.end (); it++)
    {
      PIORoutingEntry *route = it->first;
      Validity validity = route->GetValidity ();

      if (validity == VALID || validity == LHOST || validity == INVALID)
      {
        std::ostringstream dest, gateway, val;

        // Destination Network
        dest << route->GetDestNetwork () << "/" << int (route->GetDestNetworkMask ().GetPrefixLength ());
        *os << std::setiosflags (std::ios::left) << std::setw (20) << dest.str ();
        
        // Gateway Address        
        gateway << route->GetGateway ();
        *os << std::setiosflags (std::ios::left) << std::setw (17) << gateway.str ();
        
        // Output interface
        *os << std::setiosflags (std::ios::left) << std::setw (4) << route->GetInterface ();
        
        // Sequence number of the route
        *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetSequenceNo ();
        
        // Metric of the route
        *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetMetric ();
        
        //Validity of the route
        if (route->GetValidity () == VALID)
          val << "VALID";
        else if (route->GetValidity () == INVALID)
          val << "INVALID";
        else if (route->GetValidity () == LHOST)
          val << "Loc. Host";
        *os << std::setiosflags (std::ios::left) << std::setw (10) << val.str ();
        
        // Changed flag of the route
        *os << std::setiosflags (std::ios::left) << std::setw (7) << route->GetRouteChanged ();
        
        // printing how many seconds left for next event trigger
        *os << std::setiosflags (std::ios::left) << std::setw (8) << Simulator::GetDelayLeft (it->second).GetSeconds ();
       
        *os << '\n';
      }        
    }
	}    
}

void 
PIORoutingProtocol::AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface);

  PIORoutingEntry* route = new PIORoutingEntry (network, networkMask, nextHop, interface);
  route->SetSequenceNo (sequenceNo);
  route->SetMetric (metric);
  route->SetValidity (VALID);
  route->SetRouteChanged (true); 

  EventId invalidateEvent;
  
  if (network == "0.0.0.0" && networkMask == Ipv4Mask::GetZero ())
  {
    // Add the default Route. As the default route is added manual by either
    // another routing protocol or administrator, this route is not set to expire. 
    // We add the route to the routing table as does for a normal route.
    // However, as this route is not setting to expire, we do not set the invalidate event.
    // Further, as the route is set as valid, the route will be advertise in periodic update.
    // Thus, as the route is not going to change, the route is not included in to the triggered update.
    
    invalidateEvent = EventId ();
  }
  else
  {
    Time delay = timeoutTime + Seconds (m_rng->GetValue (0, 5));
    invalidateEvent = Simulator::Schedule (delay, &PIORoutingProtocol::InvalidateRoute, this, route);
  }

  NS_LOG_LOGIC ("PIO: adding the nextHop route " << *route << " to the routing table");
  m_routing.push_front (std::make_pair (route, invalidateEvent));
}

void 
PIORoutingProtocol::AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << interface);

  PIORoutingEntry* route = new PIORoutingEntry (network, networkMask, interface);
  route->SetSequenceNo (sequenceNo);
  route->SetMetric (metric);
  route->SetValidity (VALID);
  route->SetRouteChanged (true); 

  EventId invalidateEvent;

  if ((timeoutTime.GetSeconds () == 0) && (garbageCollectionTime.GetSeconds () == 0))
    invalidateEvent = EventId ();
  else
  {
    Time delay = timeoutTime + Seconds (m_rng->GetValue (0, 5));
    EventId invalidateEvent = Simulator::Schedule (delay, &PIORoutingProtocol::InvalidateRoute, this, route);
  }

  NS_LOG_LOGIC ("PIO: adding the interface route " << *route << " to the routing table");
  m_routing.push_front (std::make_pair (route, invalidateEvent));
}

void 
PIORoutingProtocol::AddHostRouteTo (Ipv4Address host, uint32_t interface, uint16_t metric, uint16_t sequenceNo, Time timeoutTime, Time garbageCollectionTime)
{
  NS_LOG_FUNCTION ("AddHostRouteTo: " << this << host << interface);

  PIORoutingEntry* route = new PIORoutingEntry (host, interface);

  if (host == "127.0.0.1")
  {
    route->SetValidity (LHOST); // Neither valid nor invalid
    route->SetSequenceNo (0);
    route->SetMetric (0);
    route->SetRouteChanged (false); 
    m_routing.push_front (std::make_pair (route, EventId ()));
  }
  else
  {
    route->SetValidity (VALID);
    route->SetSequenceNo (sequenceNo);
    route->SetMetric (metric);
    route->SetRouteChanged (true); 

    Time delay = timeoutTime + Seconds (m_rng->GetValue (0, 5));
    EventId invalidateEvent = Simulator::Schedule (delay, &PIORoutingProtocol::InvalidateRoute, this, route);

    NS_LOG_LOGIC ("PIO: adding the host route " << *route << " to the routing table");
    m_routing.push_front (std::make_pair (route, invalidateEvent));
  }
}

void 
PIORoutingProtocol::AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface)
{
  NS_LOG_FUNCTION (this << nextHop << interface);
    
  AddNetworkRouteTo (Ipv4Address ("0.0.0.0"), Ipv4Mask::GetZero (), nextHop, interface, 0, 0, Seconds (0), Seconds (0));
  NS_LOG_LOGIC ("PIO: adding the default route to the routing table of " << this->GetTypeId ());
}

void 
PIORoutingProtocol::InvalidateRoute (PIORoutingEntry *route)
{
  NS_LOG_FUNCTION (this << *route);

  for (RoutesI it = m_routing.begin (); it != m_routing.end (); it++)
    {
      if (it->first == route)
        {
          it->first->SetValidity (INVALID);
          it->first->SetRouteChanged (true);
          it->second.Cancel ();
          it->second = Simulator::Schedule (m_garbageCollectionDelay, &PIORoutingProtocol::DeleteRoute, this, it->first);
          return;
        }
    }
  NS_LOG_INFO ("PIO: Cannot find a route to invalidate.");
}

void 
PIORoutingProtocol::DeleteRoute (PIORoutingEntry *route)
{
  NS_LOG_FUNCTION (this << *route);

  for (RoutesI it = m_routing.begin (); it != m_routing.end (); it++)
    {
      if (it->first == route)
        {
          delete route;
          m_routing.erase (it);
          return;
        }
    }
  NS_LOG_INFO ("PIO: Cannot find a route to delete.");
}

bool 
PIORoutingProtocol::InvalidateRoutesForInterface (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);

  bool retVal = false;

  for (RoutesI it = m_routing.begin (); it != m_routing.end (); it++)
  {
    if ((it->first->GetInterface () == interface) && (it->first->GetValidity () == VALID))
    {
      it->first->SetValidity (INVALID);
      it->first->SetRouteChanged (true);
      
      it->second.Cancel ();
      it->second = Simulator::Schedule (m_garbageCollectionDelay, &PIORoutingProtocol::DeleteRoute, this, it->first);
      retVal = true;
    }
  }
  
  if (retVal == false)
    NS_LOG_INFO ("PIO: no route found for the given interface.");    
  
  return retVal;
}

bool 
PIORoutingProtocol::InvalidateBrokenRoutes(Ipv4Address destination, Ipv4Mask destinationMask)
{
  NS_LOG_FUNCTION (this << destination << destinationMask);
  
  bool retVal = false;

  for (RoutesI it = m_routing.begin (); it != m_routing.end (); it++)
  {
    if ((it->first->GetDestNetwork () == destination) && 
				(it->first->GetDestNetworkMask () == destinationMask) && 
				(it->first->GetValidity () == VALID))
    {
      it->first->SetValidity (INVALID);
      it->first->SetRouteChanged (true);
      
      it->second.Cancel ();
      it->second = Simulator::Schedule (m_garbageCollectionDelay, &PIORoutingProtocol::DeleteRoute, this, it->first);
      retVal = true;
    }
  }
  
  if (retVal == false)
    NS_LOG_INFO ("PIO: no route found for the given destination network.");
  
  return retVal;
}

bool 
PIORoutingProtocol::InvalidateRoutesForGateway (Ipv4Address gateway)
{
  NS_LOG_FUNCTION (this << gateway);
  
  bool retVal = false;

  for (RoutesI it = m_routing.begin (); it != m_routing.end (); it++)
  {
    if ((it->first->GetGateway () == gateway) &&
				(it->first->GetValidity () == VALID))
    {
      it->first->SetValidity (INVALID);
      it->first->SetRouteChanged (true);
      
      it->second.Cancel ();
      it->second = Simulator::Schedule (m_garbageCollectionDelay, &PIORoutingProtocol::DeleteRoute, this, it->first);
      retVal = true;
    }
  }
  if (retVal == false)
    NS_LOG_INFO ("PIO: no route found for the given gateway.");
    
  return retVal;
}

bool
PIORoutingProtocol::IsLocalRouteAvailable (Ipv4Address address, Ipv4Mask mask)
{
  bool retVal = false;

  for (RoutesI it = m_routing.begin ();  it != m_routing.end (); it++)
  {
    if ((it->first->GetDestNetwork () == address) &&
        (it->first->GetDestNetworkMask () == mask) &&
        (it->first->GetGateway () == Ipv4Address::GetZero ()))
    {
      return (retVal = true);
    }
  }
  return retVal;   
}

bool
PIORoutingProtocol::FindRouteRecord (Ipv4Address address, Ipv4Mask mask, RoutesI &foundRoute)
{
  bool retVal = false;

  for (RoutesI it = m_routing.begin ();  it!= m_routing.end (); it++)
  {
    if ((it->first->GetDestNetwork () == address) &&
        (it->first->GetDestNetworkMask () == mask) &&
        (it->first->GetGateway () != Ipv4Address::GetZero ()))
    {
      foundRoute = it;
      retVal = true;
      break;
    }
  }
  return retVal;    
}

Ptr<Ipv4Route> 
PIORoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                            Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << oif);
  
  Ptr<Ipv4Route> rtEntry = 0;
  Ipv4Address destination = header.GetDestination ();
  
  if (destination.IsMulticast ())
  {
    // Note:  Multicast routes for outbound packets are stored in the normal unicast table.
    // This is a well-known property of sockets implementation on many Unix variants.
    // So, just log it and follow the static route search for multicasting also
    NS_LOG_LOGIC ("RouteOutput (): Multicast destination");
  }
  
  rtEntry  = LookupRoute (destination, oif);
  
  if (rtEntry)
  {
    NS_LOG_LOGIC ("PIO: found the route" << rtEntry);  
    sockerr = Socket::ERROR_NOTERROR;
  }
  else
  {
    NS_LOG_LOGIC ("PIO: no route entry found. Returning the Socket Error");  
    sockerr = Socket::ERROR_NOROUTETOHOST;
  }
  return rtEntry;  
}

bool 
PIORoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                 UnicastForwardCallback ucb, MulticastForwardCallback mcb, 
                 LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev);
  
  bool retVal = false;
  
  NS_ASSERT (m_ipv4 != 0);
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);
  Ipv4Address dst = header.GetDestination ();
  
  if (dst.IsMulticast ())
  {
    NS_LOG_LOGIC ("PIO: Multicast routes are not supported by the PIO");
    return (retVal = false); // Let other routing protocols try to handle this
  }
  
  // First find the local interfaces and forward the packet locally.
  // Note: As T. Pecorella mentioned in the RIPng implementation,
  // this method is also check every interface before forward the packet among the local interfaces.
  // However, if we enable the configuration option as mentioned in the \RFC{1222},
  // this forwarding can be done bit intelligently.
  
  for (uint32_t j = 0; j < m_ipv4->GetNInterfaces (); j ++)
  {
    for (uint32_t i = 0; i < m_ipv4->GetNAddresses (j); i ++)
    {
      Ipv4InterfaceAddress iface = m_ipv4->GetAddress (j, i);
      Ipv4Address address = iface.GetLocal ();
    
      if (address.IsEqual (header.GetDestination ()))
      {
        if (j == iif)
        {
          NS_LOG_LOGIC ("PIO: packet is for me and forwarding it for the interface " << iif);
        }
        else
        {
          NS_LOG_LOGIC ("PIO: packet is for me but for different interface " << j);
        }
        
        lcb (p, header, iif);
        return (retVal = true);
      }
      
      NS_LOG_LOGIC ("Address " << address << " is not a match");
    }
  }
  
  // Check the input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
  {
    NS_LOG_LOGIC ("PIO: packet forwarding is disabled for this interface " << iif);
    
    ecb (p, header, Socket::ERROR_NOROUTETOHOST);
    return (retVal = false);
  }
  
  // Finally, check for route and forwad the packet to the next hop
  NS_LOG_LOGIC ("PIO: finding a route in the routing table");
  
  Ptr<Ipv4Route> route = LookupRoute (header.GetDestination ());
  
  if (route != 0)
  {
    NS_LOG_LOGIC ("PIO: found a route and calling uni-cast callback");
    ucb (route, p, header);  // uni-cast forwarding callback
    return (retVal = true);
  }
  else
  {
    NS_LOG_LOGIC ("PIO: no route found");
    return (retVal = false);      
  }
}

Ptr<Ipv4Route>
PIORoutingProtocol::LookupRoute (Ipv4Address address, Ptr<NetDevice> dev)
{
  NS_LOG_FUNCTION ("LookupRoute: " << this << ", address=" << address << ", dev=" << dev);
  
  Ptr<Ipv4Route> rtentry = 0;
  
  // Note: if the packet is destined for local multicasting group, 
  // the relevant interfaces has to be specified while looking up the route
  if(address.IsLocalMulticast ())
  {
    NS_ASSERT_MSG (m_ipv4->GetInterfaceForDevice (dev), "PIO: destination is for multicasting, and however, no interface index is given!");
    
    rtentry = Create<Ipv4Route> ();
    
    rtentry->SetSource (m_ipv4->SelectSourceAddress (dev, address, Ipv4InterfaceAddress::LINK)); // has to be clarified
    rtentry->SetDestination (address);
    rtentry->SetGateway (Ipv4Address::GetZero ());
    rtentry->SetOutputDevice (dev);
    
    return rtentry;      
  }
  
  //Now, select a route from the routing table which matches the destination address
  
  for (RoutesI it = m_routing.begin (); it != m_routing.end (); it++)
  {
    PIORoutingEntry* routeEntry = it->first;
    
    if (routeEntry->GetValidity () == VALID)
    {
      Ipv4Address destination  = routeEntry->GetDestNetwork ();
      Ipv4Mask mask = routeEntry->GetDestNetworkMask ();
      
      NS_LOG_LOGIC ("PIO: searching for a route to " << address << ", with the mask " << mask);
      
      if (mask.IsMatch (address, destination))
      {
        NS_LOG_LOGIC ("PIO: found a route " << routeEntry << ", with the mask " << mask);
        
        // check the device is given and the packet can be output using this device
        if ((!dev) || (dev == m_ipv4->GetNetDevice (routeEntry->GetInterface ())))
        {
          Ipv4RoutingTableEntry* route = routeEntry;
          uint32_t interfaceIndex = route->GetInterface ();
    
          rtentry = Create<Ipv4Route> ();
          
          rtentry->SetDestination (route->GetDest ());
          rtentry->SetGateway (route->GetGateway ());
          rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIndex));            
          rtentry->SetSource (m_ipv4->SelectSourceAddress (m_ipv4->GetNetDevice (interfaceIndex), route->GetDest (), Ipv4InterfaceAddress::GLOBAL)); // has to be clarified  
          
          // As the route is found, no need of iterating on the routing table any more.
          NS_LOG_LOGIC ("PIO: found a match for the destination " << rtentry->GetDestination () << " via " << rtentry->GetGateway ());          
          break;          
        }
      }
    }
  }

  return rtentry;
}

void 
PIORoutingProtocol::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  
  m_routing.clear ();

  for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
  {
    iter->first->Close ();
  }
  m_sendSocketList.clear ();
  if(m_recvSocket)
    {
      m_recvSocket->Close ();
      m_recvSocket = 0;
    }

  m_nextKeepAliveMessage.Cancel ();
  m_nextKeepAliveMessage = EventId ();
  
  m_nextTriggeredUpdate.Cancel ();
  m_nextTriggeredUpdate = EventId ();

  m_nextPeriodicUpdate.Cancel ();
  m_nextPeriodicUpdate = EventId ();

  m_ipv4 = 0;

}

/*
*  PIORoutingEntry
*/

PIORoutingEntry::PIORoutingEntry () : m_sequenceNo (0),
                                        m_metric (0),
                                        m_changed (false),
                                        m_validity (INVALID)
{
  /*cstrctr*/
}

PIORoutingEntry::PIORoutingEntry (Ipv4Address network, 
                                    Ipv4Mask networkMask, 
                                    Ipv4Address nextHop, 
                                    uint32_t interface) :
                                    Ipv4RoutingTableEntry (PIORoutingEntry::CreateNetworkRouteTo
(network, networkMask, nextHop, interface)), m_sequenceNo (0),
                                             m_metric (0),
                                             m_changed (false),
                                             m_validity (INVALID)
{
  /*cstrctr*/
}

PIORoutingEntry::PIORoutingEntry (Ipv4Address network, 
                                    Ipv4Mask networkMask,
                                    uint32_t interface) :
                                    Ipv4RoutingTableEntry (PIORoutingEntry::CreateNetworkRouteTo
(network, networkMask, interface)), m_sequenceNo (0),
                                    m_metric (0),
                                    m_changed (false),
                                    m_validity (INVALID)
{
  /*cstrctr*/
}

PIORoutingEntry::PIORoutingEntry (Ipv4Address host,
                                    uint32_t interface) :
                                    Ipv4RoutingTableEntry (PIORoutingEntry::CreateHostRouteTo
(host, interface)), m_sequenceNo (0),
                    m_metric (0),
                    m_changed (false),
                    m_validity (INVALID)
{
  /*cstrctr*/
}

PIORoutingEntry::~PIORoutingEntry ()
{
  /*dstrctr*/
}

std::ostream & operator << (std::ostream& os, const PIORoutingEntry& rte)
{
  os << static_cast<const Ipv4RoutingTableEntry &>(rte);
  os << ", metric=" << int (rte.GetMetric ());

  return os;
}

}

