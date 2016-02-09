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
 */

/* Network topology
//
//       SRC
//        |<=== source network
//        A
//       / \
//      /   \
//     B---- C
//      \   /
//       \ /
//        D
//        |<=== target network
//       DST
//
// Examining the .pcap files with Wireshark can confirm this effect.*/

#include <fstream>

#include "ns3/core-module.h"
#include "ns3/pio-module.h"
#include "ns3/pio.h"

#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PIOSimpleRouting");

/**
 * \brief Down a given link
 * \param NodeA and nodeB Nodes that connected via the link
 * \param intA and intB interfaces that the link is connected on
 */
void MakeLinkDown (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t intA, uint32_t intB)
{
  nodeA->GetObject<Ipv4>()->SetDown (intA);
  nodeB->GetObject<Ipv4>()->SetDown (intB);
}

/**
 * \brief Up a given link
 * \param NodeA and nodeB Nodes that connected via the link
 * \param intA and intB interfaces that the link is connected on
 */
void MakeLinkUp (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t intA, uint32_t intB)
{
  nodeA->GetObject<Ipv4>()->SetUp (intA);
  nodeB->GetObject<Ipv4>()->SetUp (intB);
}

/**
 * \brief Down a given intarface
 * \param NodeA node ID
 * \param intA interface number
 */
void MakeInterfaceDown (Ptr<Node> nodeA, uint32_t intA)
{
  nodeA->GetObject<Ipv4>()->SetDown (intA);
}

/**
 * \brief Up a given intarface
 * \param NodeA node ID
 * \param intA interface number
 */
void MakeInterfaceUp (Ptr<Node> nodeA, uint32_t intA)
{
  nodeA->GetObject<Ipv4>()->SetUp (intA);
}

void showIf(Ptr<Node> node)
{
    NS_LOG_UNCOND ("Node " << node->GetId ());
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address addr;
    int cif = ipv4->GetNInterfaces();
    for (int i = 0; i < cif; i++)
    {
        int cad = ipv4->GetNAddresses (i);
        for (int a = 0; a < cad; a++)
        {
            addr = ipv4->GetAddress (i, a).GetLocal ();
            NS_LOG_UNCOND ("if " << i << " addr " << a << " has " << addr);
        }
    }
    NS_LOG_UNCOND ("");
}

int 
main (int argc, char *argv[])
{
  bool verbose = false;
  bool MTable = true; //!< printing the main table
  bool NTable = false; //!< printing the neighbor table
  bool showPings = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  //cmd.AddValue ("NTable", "Print the Neighbor Table", NTable);
  cmd.AddValue ("MTable", "Print the Main Routing Table", MTable);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("PIOSimpleRouting", LOG_LEVEL_INFO);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_INFO);
      LogComponentEnable ("Ipv6Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("NdiscCache", LOG_LEVEL_ALL);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
    }

  if (showPings)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    }
  LogComponentEnable ("PIORoutingProtocol", LOG_LEVEL_WARN);

  NS_LOG_INFO ("Create nodes.");
  Ptr<Node> src = CreateObject<Node> ();
  Names::Add ("SrcNode", src);
  Ptr<Node> dst = CreateObject<Node> ();
  Names::Add ("DstNode", dst);
  Ptr<Node> a = CreateObject<Node> ();
  Names::Add ("RouterA", a);
  Ptr<Node> b = CreateObject<Node> ();
  Names::Add ("RouterB", b);
  Ptr<Node> c = CreateObject<Node> ();
  Names::Add ("RouterC", c);
  Ptr<Node> d = CreateObject<Node> ();
  Names::Add ("RouterD", d);
  NodeContainer net1 (src, a);
  NodeContainer net2 (a, b);
  NodeContainer net3 (a, c);
  NodeContainer net6 (b, c);
  NodeContainer net4 (b, d);
  NodeContainer net5 (c, d);
  NodeContainer net7 (d, dst);
  NodeContainer routers (a, b, c, d);
  NodeContainer nodes (src, dst);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer ndc1 = p2p.Install (net1);
  NetDeviceContainer ndc2 = p2p.Install (net2);
  NetDeviceContainer ndc3 = p2p.Install (net3);
  NetDeviceContainer ndc4 = p2p.Install (net4);
  NetDeviceContainer ndc5 = p2p.Install (net5);
  NetDeviceContainer ndc6 = p2p.Install (net6);
  NetDeviceContainer ndc7 = p2p.Install (net7);

  NS_LOG_INFO ("Create IPv4 and routing");
  PIOHelper piorRouting;

  NS_LOG_INFO ("Assign the printing...");
  if (MTable)
    piorRouting.Set ("PrintingMethod", EnumValue(MAIN_R_TABLE));
  else if (NTable) 
    piorRouting.Set ("PrintingMethod", EnumValue(N_TABLE));

  Ipv4ListRoutingHelper list;
  list.Add (piorRouting, 0);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (routers);

  InternetStackHelper internetNodes;
  internetNodes.Install (nodes);

  NS_LOG_INFO ("Assign IPv4 Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.16.0","255.255.255.252");
  Ipv4InterfaceContainer iic1 = ipv4.Assign (ndc1);

  ipv4.SetBase ("15.16.16.0","255.255.255.0");
  Ipv4InterfaceContainer iic2 = ipv4.Assign (ndc2);

  ipv4.SetBase ("203.15.19.0","255.255.255.0");
  Ipv4InterfaceContainer iic3 = ipv4.Assign (ndc3);

  ipv4.SetBase ("201.13.15.0","255.255.255.0");
  Ipv4InterfaceContainer iic4 = ipv4.Assign (ndc4);

  ipv4.SetBase ("10.10.10.0","255.255.255.0");
  Ipv4InterfaceContainer iic5 = ipv4.Assign (ndc5);

  ipv4.SetBase ("11.118.126.0","255.255.255.0");
  Ipv4InterfaceContainer iic6 = ipv4.Assign (ndc6);

  ipv4.SetBase ("172.16.1.0","255.255.255.252");
  Ipv4InterfaceContainer iic7 = ipv4.Assign (ndc7);

  NS_LOG_INFO ("Setting the default gateways of the Source and Destination.");
  Ipv4StaticRoutingHelper statRouting;
  
  // setting up the 'A' as the default gateway of the 'Src'
  Ptr<Ipv4StaticRouting> statSrc = statRouting.GetStaticRouting (src->GetObject<Ipv4> ());
  statSrc->SetDefaultRoute (a->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), 1, 1);

  // setting up the 'D' as the default gateway of the 'Dst'
  Ptr<Ipv4StaticRouting> statDst = statRouting.GetStaticRouting (dst->GetObject<Ipv4> ());
  statDst->SetDefaultRoute (d->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal (), 1, 1);

  if(false)
    {
      NS_LOG_UNCOND ("src");
      showIf(src);
      NS_LOG_UNCOND ("a");
      showIf(a);
      NS_LOG_UNCOND ("b");
      showIf(b);
      NS_LOG_UNCOND ("c");
      showIf(c);
      NS_LOG_UNCOND ("d");
      showIf(d);
      NS_LOG_UNCOND ("dst");
      showIf(dst);
    }

  PIOHelper routingHelper;
  NS_LOG_UNCOND ("IsIni routingHelper: " << routingHelper.IsIni(a));

  Ptr<PIORoutingProtocol> pior = routingHelper.GetPIORouting (a->GetObject<Ipv4> ());
  if (pior)
    {
      NS_LOG_UNCOND ("IsIni piorProto: " << pior->IsInitialized());
      pior->AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), 1, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), 3, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("203.15.19.2"), 3, 3, 4, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("15.16.16.2"), 2, 2, 4, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), Ipv4Address ("15.16.16.2"), 2, 2, 4, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), Ipv4Address ("15.16.16.2"), 2, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), Ipv4Address ("15.16.16.2"), 2, 1, 2, Seconds (500), Seconds (500));
    }
  else
    NS_LOG_UNCOND ("IsIni piorProto: NULL");

  pior = routingHelper.GetPIORouting (b->GetObject<Ipv4> ());
  if (pior)
    {
      pior->AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), 1, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), 3, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), Ipv4Address ("11.118.126.2"), 3, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), Ipv4Address ("11.118.126.2"), 3, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("201.13.15.2"), 2, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), Ipv4Address ("15.16.16.1"), 1, 1, 2, Seconds (500), Seconds (500));
    }

  pior = routingHelper.GetPIORouting (c->GetObject<Ipv4> ());
  if (pior)
    {
      pior->AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), 1, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), 3, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), Ipv4Address ("10.10.10.2"), 2, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), Ipv4Address ("10.10.10.2"), 2, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), Ipv4Address ("203.15.19.1"), 1, 1, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), Ipv4Address ("203.15.19.1"), 1, 1, 2, Seconds (500), Seconds (500));
    }

  pior = routingHelper.GetPIORouting (d->GetObject<Ipv4> ());
  if (pior)
    {
      pior->AddHostRouteTo (Ipv4Address ("127.0.0.1"), 0, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("201.13.15.0"), Ipv4Mask ("/24"), 1, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("10.10.10.0"), Ipv4Mask ("/24"), 2, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("172.16.1.0"), Ipv4Mask ("/30"), 3, 0, 2, Seconds (0), Seconds (0));
      pior->AddNetworkRouteTo (Ipv4Address ("192.168.16.0"), Ipv4Mask ("/30"), Ipv4Address ("201.13.15.1"), 1, 1, 4, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("203.15.19.0"), Ipv4Mask ("/24"), Ipv4Address ("201.13.15.1"), 1, 1, 4, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("11.118.126.0"), Ipv4Mask ("/24"), Ipv4Address ("201.13.15.1"), 1, 2, 2, Seconds (500), Seconds (500));
      pior->AddNetworkRouteTo (Ipv4Address ("15.16.16.0"), Ipv4Mask ("/24"), Ipv4Address ("201.13.15.1"), 1, 2, 2, Seconds (500), Seconds (500));
    }

  // Enable the printing option for the listRouting
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
  if (MTable || NTable)
    {
      routingHelper.PrintRoutingTableEvery (Seconds (30), a, routingStream);
      routingHelper.PrintRoutingTableEvery (Seconds (30), b, routingStream);
      routingHelper.PrintRoutingTableEvery (Seconds (30), c, routingStream);
      routingHelper.PrintRoutingTableEvery (Seconds (30), d, routingStream);
    }

  NS_LOG_INFO ("Setting up UDP echo server and client.");
  //create server
  uint16_t port = 9; // well-known echo port number
  UdpEchoServerHelper server (port);
  ApplicationContainer apps = server.Install (dst);

  apps.Start (Seconds (40.0));
  apps.Stop (Seconds (80.0));

  //create client
  Ptr<Ipv4> ipv4dst = dst->GetObject<Ipv4> ();

  UdpEchoClientHelper client (ipv4dst->GetAddress (1, 0).GetLocal (), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  
  apps = client.Install (src);
  
  apps.Start (Seconds (40.0));
  apps.Stop (Seconds (45.0));

  // In order to obtain how PIO reacts when broken links and broken interfaces, 
  // uncomment the following as prefer.
  // Further set the time as prefer.
  
  // make a link down and up  
  //Simulator::Schedule (Seconds (40), &MakeLinkDown, b, d, 3, 2); 
  //Simulator::Schedule (Seconds (185), &MakeLinkUp, b, d, 3, 2); 

  // make a interface down and up
  //Simulator::Schedule (Seconds (40), &MakeInterfaceDown, d, 2); 
  //Simulator::Schedule (Seconds (1l85), &MakeInterfaceUp, d, 2); 

  p2p.EnablePcapAll ("proute", true);

  Simulator::Stop (Seconds (59));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}


