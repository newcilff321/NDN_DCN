/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 UCLA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Yuanjie Li <yuanjie.li@cs.ucla.edu>
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/names.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/config.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/net-device.h"
#include "ns3/channel.h"
#include "ns3/callback.h"
#include "ns3/node.h"
#include "ns3/core-config.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/callback.h"

#include "../model/ndn-net-device-face.h"
//#include "../model/ndn-l3-protocol.h"
#include "../model/ndn-bcube-l3-protocol.h"

#include "ns3/ndn-forwarding-strategy.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-fib2.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-name.h"
#include "ns3/ndn-content-store.h"

#include "ns3/node-list.h"
// #include "ns3/loopback-net-device.h"

#include "ns3/data-rate.h"

#include "ndn-face-container.h"
#include "ndn-bcube-stack-helper.h"

#include <limits>
#include <map>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

NS_LOG_COMPONENT_DEFINE ("ndn.BCubeStackHelper");

namespace ns3 {
namespace ndn {

BCubeStackHelper::BCubeStackHelper ()
  : m_limitsEnabled (false)
{
  m_ndnFactory.         SetTypeId ("ns3::ndn::BCubeL3Protocol");
  m_strategyFactory.    SetTypeId ("ns3::ndn::fw::Flooding");
  m_contentStoreFactory.SetTypeId ("ns3::ndn::cs::Lru");
  m_fibFactory.         SetTypeId ("ns3::ndn::fib::Default");
  m_fib2Factory.				SetTypeId ("ns3::ndn::fib2::Default");
  m_pitFactory.         SetTypeId ("ns3::ndn::pit::Persistent");

  
}

BCubeStackHelper::~BCubeStackHelper ()
{
}

void
BCubeStackHelper::SetStackAttributes (const std::string &attr1, const std::string &value1,
                                 const std::string &attr2, const std::string &value2,
                                 const std::string &attr3, const std::string &value3,
                                 const std::string &attr4, const std::string &value4)
{
  if (attr1 != "")
      m_ndnFactory.Set (attr1, StringValue (value1));
  if (attr2 != "")
      m_ndnFactory.Set (attr2, StringValue (value2));
  if (attr3 != "")
      m_ndnFactory.Set (attr3, StringValue (value3));
  if (attr4 != "")
      m_ndnFactory.Set (attr4, StringValue (value4));
}

void
BCubeStackHelper::SetForwardingStrategy (const std::string &strategy,
                                    const std::string &attr1, const std::string &value1,
                                    const std::string &attr2, const std::string &value2,
                                    const std::string &attr3, const std::string &value3,
                                    const std::string &attr4, const std::string &value4)
{
  m_strategyFactory.SetTypeId (strategy);
  if (attr1 != "")
      m_strategyFactory.Set (attr1, StringValue (value1));
  if (attr2 != "")
      m_strategyFactory.Set (attr2, StringValue (value2));
  if (attr3 != "")
      m_strategyFactory.Set (attr3, StringValue (value3));
  if (attr4 != "")
      m_strategyFactory.Set (attr4, StringValue (value4));
}

void
BCubeStackHelper::SetContentStore (const std::string &contentStore,
                              const std::string &attr1, const std::string &value1,
                              const std::string &attr2, const std::string &value2,
                              const std::string &attr3, const std::string &value3,
                              const std::string &attr4, const std::string &value4)
{
  m_contentStoreFactory.SetTypeId (contentStore);
  if (attr1 != "")
      m_contentStoreFactory.Set (attr1, StringValue (value1));
  if (attr2 != "")
      m_contentStoreFactory.Set (attr2, StringValue (value2));
  if (attr3 != "")
      m_contentStoreFactory.Set (attr3, StringValue (value3));
  if (attr4 != "")
      m_contentStoreFactory.Set (attr4, StringValue (value4));
}

void
BCubeStackHelper::SetPit (const std::string &pitClass,
                     const std::string &attr1, const std::string &value1,
                     const std::string &attr2, const std::string &value2,
                     const std::string &attr3, const std::string &value3,
                     const std::string &attr4, const std::string &value4)
{
  m_pitFactory.SetTypeId (pitClass);
  if (attr1 != "")
      m_pitFactory.Set (attr1, StringValue (value1));
  if (attr2 != "")
      m_pitFactory.Set (attr2, StringValue (value2));
  if (attr3 != "")
      m_pitFactory.Set (attr3, StringValue (value3));
  if (attr4 != "")
      m_pitFactory.Set (attr4, StringValue (value4));
}

void
BCubeStackHelper::SetFib (const std::string &fibClass,
                     const std::string &attr1, const std::string &value1,
                     const std::string &attr2, const std::string &value2,
                     const std::string &attr3, const std::string &value3,
                     const std::string &attr4, const std::string &value4)
{
  m_fibFactory.SetTypeId (fibClass);
  if (attr1 != "")
      m_fibFactory.Set (attr1, StringValue (value1));
  if (attr2 != "")
      m_fibFactory.Set (attr2, StringValue (value2));
  if (attr3 != "")
      m_fibFactory.Set (attr3, StringValue (value3));
  if (attr4 != "")
      m_fibFactory.Set (attr4, StringValue (value4));
}

void
BCubeStackHelper::SetFib2 (const std::string &fib2Class,
                     const std::string &attr1, const std::string &value1,
                     const std::string &attr2, const std::string &value2,
                     const std::string &attr3, const std::string &value3,
                     const std::string &attr4, const std::string &value4)
{
  m_fib2Factory.SetTypeId (fib2Class);
  if (attr1 != "")
      m_fib2Factory.Set (attr1, StringValue (value1));
  if (attr2 != "")
      m_fib2Factory.Set (attr2, StringValue (value2));
  if (attr3 != "")
      m_fib2Factory.Set (attr3, StringValue (value3));
  if (attr4 != "")
      m_fib2Factory.Set (attr4, StringValue (value4));
}

void
BCubeStackHelper::EnableLimits (bool enable/* = true*/,
                           Time avgRtt/*=Seconds(0.1)*/,
                           uint32_t avgContentObject/*=1100*/,
                           uint32_t avgInterest/*=40*/)
{
  NS_LOG_INFO ("EnableLimits: " << enable);
  m_limitsEnabled = enable;
  m_avgRtt = avgRtt;
  m_avgContentObjectSize = avgContentObject;
  m_avgInterestSize = avgInterest;
}

Ptr<FaceContainer>
BCubeStackHelper::Install (const NodeContainer &c) const
{
  Ptr<FaceContainer> faces = Create<FaceContainer> ();
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      faces->AddAll (Install (*i));
    }
  return faces;
}

Ptr<FaceContainer>
BCubeStackHelper::InstallAll () const
{
  return Install (NodeContainer::GetGlobal ());
}

Ptr<FaceContainer>
BCubeStackHelper::Install (Ptr<Node> node) const
{
  // NS_ASSERT_MSG (m_forwarding, "SetForwardingHelper() should be set prior calling Install() method");
  Ptr<FaceContainer> faces = Create<FaceContainer> ();

  if (node->GetObject<BCubeL3Protocol> () != 0)
    {
      NS_FATAL_ERROR ("BCubeStackHelper::Install (): Installing "
                      "a NdnStack to a node with an existing Ndn object");
      return 0;
    }

  // Create BCubeL3Protocol
  Ptr<BCubeL3Protocol> ndn = m_ndnFactory.Create<BCubeL3Protocol> ();

  // Create and aggregate FIB
  Ptr<Fib> fib = m_fibFactory.Create<Fib> ();
  ndn->AggregateObject (fib);
  
  // Create and aggregate FIB2
  Ptr<Fib2> fib2 = m_fib2Factory.Create<Fib2> ();
  ndn->AggregateObject (fib2);

  // Create and aggregate PIT
  ndn->AggregateObject (m_pitFactory.Create<Pit> ());

  // Create and aggregate forwarding strategy
  ndn->AggregateObject (m_strategyFactory.Create<ForwardingStrategy> ());

  // Create and aggregate content store
  ndn->AggregateObject (m_contentStoreFactory.Create<ContentStore> ());

  // Aggregate BCubeL3Protocol on node
  node->AggregateObject (ndn);

  for (uint32_t index=0; index < node->GetNDevices (); index++)
    {
      Ptr<NetDevice> device = node->GetDevice (index);
      // This check does not make sense: LoopbackNetDevice is installed only if IP stack is installed,
      // Normally, ndnSIM works without IP stack, so no reason to check
      // if (DynamicCast<LoopbackNetDevice> (device) != 0)
      //   continue; // don't create face for a LoopbackNetDevice

      PairFace res = PointToPointNetDeviceCallBack (node, ndn, device);

      
      if (res.first == 0 || res.second == 0)
        {
          return 0;
        }

      res.first->SetUp ();
      res.second->SetUp ();
      faces->Add (res.first);	//upload face
      faces->Add (res.second); //download face
    }

  return faces;
}

PairFace
BCubeStackHelper::PointToPointNetDeviceCallBack (Ptr<Node> node, Ptr<BCubeL3Protocol> ndn, Ptr<NetDevice> device) const
{
  NS_LOG_DEBUG ("Creating point-to-point NetDeviceFace on node " << node->GetId ());

  Ptr<NetDeviceFace> uploadface = CreateObject<NetDeviceFace> (node, device);
  Ptr<NetDeviceFace> downloadface = CreateObject<NetDeviceFace> (node, device);

  ndn->AddFace (uploadface, downloadface);
  
  PairFace pair;
  pair.first = uploadface;
  pair.second = downloadface;
  if (m_limitsEnabled)
    {
      Ptr<Limits> uploadlimits = uploadface->GetObject<Limits> ();
      Ptr<Limits> downloadlimits = downloadface->GetObject<Limits> ();
      if (uploadlimits == 0 || downloadlimits == 0)
        {
          NS_FATAL_ERROR ("Limits are enabled, but the selected forwarding strategy does not support limits. Please revise your scenario");
          exit (1);
        }

      NS_LOG_INFO ("Limits are enabled");
      Ptr<PointToPointNetDevice> p2p = DynamicCast<PointToPointNetDevice> (device);
      if (p2p != 0)
        {
          // Setup bucket filtering
          // Assume that we know average data packet size, and this size is equal default size
          // Set maximum buckets (averaging over 1 second)

          DataRateValue dataRate; device->GetAttribute ("DataRate", dataRate);
          TimeValue linkDelay;   device->GetChannel ()->GetAttribute ("Delay", linkDelay);

          NS_LOG_INFO("DataRate for this link is " << dataRate.Get());

          double maxInterestPackets = 1.0  * dataRate.Get ().GetBitRate () / 8.0 / (m_avgContentObjectSize + m_avgInterestSize);
          // NS_LOG_INFO ("Max packets per second: " << maxInterestPackets);
          // NS_LOG_INFO ("Max burst: " << m_avgRtt.ToDouble (Time::S) * maxInterestPackets);
          NS_LOG_INFO ("MaxLimit: " << (int)(m_avgRtt.ToDouble (Time::S) * maxInterestPackets));

          // Set max to BDP
          uploadlimits->SetLimits (maxInterestPackets, m_avgRtt.ToDouble (Time::S));
          uploadlimits->SetLinkDelay (linkDelay.Get ().ToDouble (Time::S));
          downloadlimits->SetLimits (maxInterestPackets, m_avgRtt.ToDouble (Time::S));
          downloadlimits->SetLinkDelay (linkDelay.Get ().ToDouble (Time::S));
        }
    }

  return pair;
}


Ptr<FaceContainer>
BCubeStackHelper::Install (const std::string &nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return Install (node);
}

void
BCubeStackHelper::AddRoute (Ptr<Node> node, const std::string &prefix, Ptr<Face> face, int32_t metric)
{
  NS_LOG_LOGIC ("[" << node->GetId () << "]$ route add " << prefix << " via " << *face << " metric " << metric);

  Ptr<Fib>  fib  = node->GetObject<Fib> ();

  NameValue prefixValue;
  prefixValue.DeserializeFromString (prefix, MakeNameChecker ());
  fib->Add (prefixValue.Get (), face, metric);
}

void
BCubeStackHelper::AddRoute (Ptr<Node> node, const std::string &prefix, uint32_t faceId, int32_t metric)
{
  Ptr<BCubeL3Protocol>     ndn = node->GetObject<BCubeL3Protocol> ();
  NS_ASSERT_MSG (ndn != 0, "Ndn stack should be installed on the node");

  Ptr<Face> face = ndn->GetUploadFace (faceId);
  NS_ASSERT_MSG (face != 0, "Face with ID [" << faceId << "] does not exist on node [" << node->GetId () << "]");

  AddRoute (node, prefix, face, metric);
}


void
BCubeStackHelper::AddRoute (Ptr<Node> node, const std::string &prefix, Ptr<Node> otherNode, int32_t metric)
{
  for (uint32_t deviceId = 0; deviceId < node->GetNDevices (); deviceId ++)
    {
      Ptr<PointToPointNetDevice> netDevice = DynamicCast<PointToPointNetDevice> (node->GetDevice (deviceId));
      if (netDevice == 0)
        continue;

      Ptr<Channel> channel = netDevice->GetChannel ();
      if (channel == 0)
        continue;

      if (channel->GetDevice (0)->GetNode () == otherNode ||
          channel->GetDevice (1)->GetNode () == otherNode)
        {
          Ptr<BCubeL3Protocol> ndn = node->GetObject<BCubeL3Protocol> ();
          NS_ASSERT_MSG (ndn != 0, "Ndn stack should be installed on the node");

          Ptr<Face> face = ndn->GetUploadFaceByNetDevice (netDevice);
          NS_ASSERT_MSG (face != 0, "There is no face associated with the p2p link");

          AddRoute (node, prefix, face, metric);

          return;
        }
    }

  NS_FATAL_ERROR ("Cannot add route: Node# " << node->GetId () << " and Node# " << otherNode->GetId () << " are not connected");
}

void
BCubeStackHelper::AddRoute (const std::string &nodeName, const std::string &prefix, const std::string &otherNodeName, int32_t metric)
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  NS_ASSERT_MSG (node != 0, "Node [" << nodeName << "] does not exist");

  Ptr<Node> otherNode = Names::Find<Node> (otherNodeName);
  NS_ASSERT_MSG (otherNode != 0, "Node [" << otherNodeName << "] does not exist");

  AddRoute (node, prefix, otherNode, metric);
}


} // namespace ndn
} // namespace ns3