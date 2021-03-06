/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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
 * Author: Yuanjie Li <yuanjie.li@cs.ucla.edu>
 */
// ndncc-multipath-fairness.cc Two consumers. One has two producers, the other has just one
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

using namespace ns3;

void SetLinkFailure(NodeContainer &nodes, Ptr<ns3::ndn::FaceContainer> facecontainer)
{
	//set rate limit to be zero, so we can disable the link
	Ptr<Node> router = nodes.Get (2);
	Ptr<NetDevice> router_to_P2 = router->GetDevice(2);
	
	for(ns3::ndn::FaceContainer::Iterator it = facecontainer->Begin(); it != facecontainer->End(); it++)
	{
		Ptr<ns3::ndn::NetDeviceFace> face = DynamicCast<ns3::ndn::NetDeviceFace>(*it);
		if(face !=0)
		{
			if(face->GetNetDevice() == router_to_P2 && face->GetNode() == router)
			{
				Ptr<ns3::ndn::Limits> limits = face->GetObject<ns3::ndn::Limits> ();
				limits->SetLimits (0.1, 0.2);	
				break;
			}
			 
		}
		else
			continue;
	}
}

int 
main (int argc, char *argv[])
{
	int simulation_time = 400;
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault ("ns3::PointToPointChannel::Delay", StringValue ("1ms"));
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", StringValue ("20"));
  Config::SetDefault ("ns3::ndn::fw::Nacks::EnableNACKs", BooleanValue (true));
  Config::SetDefault ("ns3::ndn::Limits::LimitsDeltaRate::UpdateInterval", StringValue ("1.0")); //This parameter is essential for fairness! We should analyze it.
  Config::SetDefault ("ns3::ndn::ConsumerOm::NackFeedback", StringValue ("1.5"));
  Config::SetDefault ("ns3::ndn::ConsumerOm::DataFeedback", StringValue ("30.0"));
  Config::SetDefault ("ns3::ndn::ConsumerOm::LimitInterval", StringValue ("1.0"));
  Config::SetDefault ("ns3::ndn::ConsumerOm::InitLimit", StringValue ("10.0"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create (5);

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue ("10Mbps"));
  p2p.Install (nodes.Get (0), nodes.Get (2));
  p2p.Install (nodes.Get (1), nodes.Get (2));
  p2p.SetDeviceAttribute("DataRate", StringValue ("2Mbps"));
  p2p.Install (nodes.Get (2), nodes.Get (3));
  p2p.SetDeviceAttribute("DataRate", StringValue ("1Mbps"));
  p2p.Install (nodes.Get (2), nodes.Get (4));
  

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  //ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestCC");
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestCC::PerOutFaceDeltaLimits");
  ndnHelper.EnableLimits(true,Seconds(0.2),40,1100);
  Ptr<ns3::ndn::FaceContainer> facecontainer = ndnHelper.InstallAll ();
  
  //Add routes here
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install (nodes);
  ndnGlobalRoutingHelper.AddOrigins ("/prefix1", nodes.Get (3));
  ndnGlobalRoutingHelper.AddOrigins ("/prefix1", nodes.Get (4));
  ndnGlobalRoutingHelper.AddOrigins ("/prefix2", nodes.Get (4));
  ndnGlobalRoutingHelper.CalculateAllPossibleRoutes ();

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerOm");
  ApplicationContainer consumers;
  consumerHelper.SetPrefix ("/prefix1");
  consumers = consumerHelper.Install (nodes.Get (0)); 
  consumers.Start (Seconds (0));	
  consumers.Stop (Seconds (simulation_time));
  consumerHelper.SetPrefix ("/prefix2");
  consumers = consumerHelper.Install (nodes.Get (1)); 
  consumers.Start (Seconds (0));	
  consumers.Stop (Seconds (simulation_time));
  
  // Producer
  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix ("/prefix1");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
  producerHelper.Install (nodes.Get (3)); 
  producerHelper.Install (nodes.Get (4)); 
  producerHelper.SetPrefix ("/prefix2");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
  producerHelper.Install (nodes.Get (4)); 
  
  Simulator::Schedule(Seconds(50), SetLinkFailure, nodes, facecontainer);
  
  



  Simulator::Stop (Seconds (simulation_time));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
