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
*/

#include "pior-helper.h"

#include "ns3/pior.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 {

PIOHelper::PIOHelper () : Ipv4RoutingHelper ()
                            
{
  m_factory.SetTypeId ("ns3::PIORoutingProtocol");
}

PIOHelper::PIOHelper (const PIOHelper &o): m_factory (o.m_factory)
{
  m_interfaceExclusions = o.m_interfaceExclusions;
}

PIOHelper::~PIOHelper ()
{
  m_interfaceExclusions.clear ();
}

PIOHelper* 
PIOHelper::Copy (void) const
{
  return new PIOHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
PIOHelper::Create (Ptr<Node> node) const
{
  Ptr<PIORoutingProtocol> PIORouteProto = m_factory.Create<PIORoutingProtocol> ();

  node->AggregateObject (PIORouteProto);
  return PIORouteProto;
}

void
PIOHelper::Set (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
} 

int PIOHelper::IsIni (Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");

  Ptr<Ipv4RoutingProtocol> rProto = ipv4->GetRoutingProtocol ();
  NS_ASSERT_MSG (rProto, "Ipv4 routing not installed on node");

  Ptr<PIORoutingProtocol> PIO = DynamicCast<PIORoutingProtocol> (rProto);
  if (PIO)
  {
    if (PIO->IsInitialized ())
      return 4;
    else
      return 3;
  }
  else
    return 0;
}

void PIOHelper::SetDefRoute (Ptr<Node> node, Ipv4Address nextHop, uint32_t interface)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
  
  Ptr<Ipv4RoutingProtocol> rProto = ipv4->GetRoutingProtocol ();
  NS_ASSERT_MSG (rProto, "Ipv4 routing not installed on node");
  
  Ptr<PIORoutingProtocol> PIO = DynamicCast<PIORoutingProtocol> (rProto);
  if (PIO)
  {
    PIO->AddDefaultRouteTo (nextHop, interface);
  }
  
  // PIO may also be in a list
  Ptr<Ipv4ListRouting> routeList = DynamicCast<Ipv4ListRouting> (rProto);
  if (routeList)
  {
    int16_t priority;
    Ptr<Ipv4RoutingProtocol> listIpv4Proto;
    Ptr<PIORoutingProtocol> listPIO;
    
    for (uint32_t i = 0; i < routeList->GetNRoutingProtocols (); i++)
    {
      listIpv4Proto = routeList->GetRoutingProtocol (i, priority);
      listPIO = DynamicCast<PIORoutingProtocol> (listIpv4Proto);
      
      if (listPIO)
      {
        PIO->AddDefaultRouteTo (nextHop, interface);
        break;
      }
    }
  }
}

Ptr<PIORoutingProtocol>
PIOHelper::GetPIORouting (Ptr<Ipv4> ipv4) const
{
  //NS_LOG_FUNCTION (this);
  Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol ();
  NS_ASSERT_MSG (ipv4rp, "No routing protocol associated with Ipv4");
  if (DynamicCast<PIORoutingProtocol> (ipv4rp))
    {
      //NS_LOG_LOGIC ("Static routing found as the main IPv4 routing protocol.");
      return DynamicCast<PIORoutingProtocol> (ipv4rp);
    }
  if (DynamicCast<Ipv4ListRouting> (ipv4rp))
    {
      Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting> (ipv4rp);
      int16_t priority;
      for (uint32_t i = 0; i < lrp->GetNRoutingProtocols ();  i++)
        {
          //NS_LOG_LOGIC ("Searching for static routing in list");
          Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol (i, priority);
          if (DynamicCast<PIORoutingProtocol> (temp))
            {
              //NS_LOG_LOGIC ("Found static routing in list");
              return DynamicCast<PIORoutingProtocol> (temp);
            }
        }
    }
  //NS_LOG_LOGIC ("PIO routing not found");
  return 0;
}

void
PIOHelper::ExcludeInterface (Ptr<Node> node, uint32_t interface)
{
  std::map< Ptr<Node>, std::set<uint32_t> >::iterator it = m_interfaceExclusions.find (node);

  if (it == m_interfaceExclusions.end ())
  {
    std::set<uint32_t> interfaces;
    interfaces.insert (interface);

    m_interfaceExclusions.insert (std::make_pair (node, interfaces));
  }
  else
  {
    it->second.insert (interface);
  }
}

}

