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

#ifndef PIO_HELPER_H
#define PIO_HELPER_H

#include "ns3/pio.h"

#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/object-factory.h"

namespace ns3 {

/**
 * \brief Helper class that adds PIO routing to nodes.
 *
 * This class is expected to be used in conjunction with
 * ns3::InternetStackHelper::SetRoutingHelper
 *
 */

class PIOHelper : public Ipv4RoutingHelper
{
public:
  /*
   * Constructor.
   */
  PIOHelper (); 

  /**
   * \brief Construct an PIOHelper from previously
   * initialized instance (Copy Constructor).
   */
  PIOHelper (const PIOHelper &);

  /**
   * Ending the instance by destructing the PIOHelper
   */
  virtual ~PIOHelper ();

  /**
   * \brief This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   * \returns pointer to clone of this PIOHelper
   *
   */
  PIOHelper* Copy (void) const;

  /**
   * \brief This method will be called by ns3::InternetStackHelper::Install  
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

  /**
   * \brief This method controls the attributes of ns3::PIO
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set.
   */
  void Set (std::string name, const AttributeValue &value);

  int IsIni (Ptr<Node> node);

  /**
   * \brief Install a default route for the node.
   * The traffic will be forwarded to the nextHop, located on the specified
   * interface, unless a specific route record is found.
   *
   * \param node the node
   * \param nextHop the next hop
   * \param interface the network interface
   */
  void SetDefRoute (Ptr<Node> node, Ipv4Address nextHop, uint32_t interface);

  /**
   * \brief Exclude an interface from PIO protocol.
   *
   * You have to call this function BEFORE installing PIO for the nodes.
   *
   * Note: the exclusion means that PIO route updates will not be propagated on the excluded interface.
   * The network prefix on that interface will be still considered in PIO.
   *
   * \param node the node
   * \param interface the network interface to be excluded
   */
  void ExcludeInterface (Ptr<Node> node, uint32_t interface);

private:
  /**
   * \brief Assignment operator declared private and not implemented to disallow
   * assignment and prevent the compiler for inserting its own.
   */
  PIOHelper &operator = (const PIOHelper &o);

  ObjectFactory m_factory; //!< Object Factory

  std::map< Ptr<Node>, std::set<uint32_t> > m_interfaceExclusions; //!< Interface Exclusion set

}; // end of the PIOHelper class

}
#endif /* PIO_HELPER_H */

