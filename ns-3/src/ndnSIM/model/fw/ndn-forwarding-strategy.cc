/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
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
 * Author:  Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *          Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-forwarding-strategy.h"

#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-fib2.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-app-face.h"

#include "ns3/assert.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/string.h"

#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"


#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tuple/tuple.hpp>
namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ForwardingStrategy);

NS_LOG_COMPONENT_DEFINE (ForwardingStrategy::GetLogName ().c_str ());

std::string
ForwardingStrategy::GetLogName ()
{
  return "ndn.fw";
}

TypeId ForwardingStrategy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ForwardingStrategy")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutInterests",  "OutInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outInterests))
    .AddTraceSource ("InInterests",   "InInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inInterests))
    .AddTraceSource ("DropInterests", "DropInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterests))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutData",  "OutData",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outData))
    .AddTraceSource ("InData",   "InData",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inData))
    .AddTraceSource ("DropData", "DropData", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropData))

    .AddAttribute ("CacheUnsolicitedData", "Cache overheard data that have not been requested",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedData),
                   MakeBooleanChecker ())

    .AddAttribute ("DetectRetransmissions", "If non-duplicate interest is received on the same face more than once, "
                                            "it is considered a retransmission",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_detectRetransmissions),
                   MakeBooleanChecker ())
    ;
  return tid;
}

ForwardingStrategy::ForwardingStrategy ()
{
	srand(time(0));
}

ForwardingStrategy::~ForwardingStrategy ()
{
}

void
ForwardingStrategy::NotifyNewAggregate ()
{
  if (m_pit == 0)
    {
      m_pit = GetObject<Pit> ();
    }
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_fib2 == 0)
  	{
  		m_fib2 = GetObject<Fib2> ();
  	}
  if (m_contentStore == 0)
    {
      m_contentStore = GetObject<ContentStore> ();
    }

  Object::NotifyNewAggregate ();
}

void
ForwardingStrategy::DoDispose ()
{
  m_pit = 0;
  m_contentStore = 0;
  m_fib = 0;
  m_fib2 = 0;

  Object::DoDispose ();
}

void
ForwardingStrategy::OnInterest (Ptr<Face> inFace,
                                Ptr<const Interest> header,
                                Ptr<const Packet> origPacket)
{
  m_inInterests (header, inFace);

  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
  bool similarInterest = false;	//by Yuanjie: we don't check retransmission
  if (pitEntry == 0)
    {
      similarInterest = false;
      pitEntry = m_pit->Create (header);
      if (pitEntry != 0)
        {
          DidCreatePitEntry (inFace, header, origPacket, pitEntry);
        }
      else
        {
          return;
        }
    }

  bool isDuplicated = true;
  if (!pitEntry->IsNonceSeen (header->GetNonce ()))
    {
      pitEntry->AddSeenNonce (header->GetNonce ());
      isDuplicated = false;
    }

  if (isDuplicated)
    {
    	NS_LOG_UNCOND("ForwardingStrategy::OnInterest: Duplicated Interest?");
      DidReceiveDuplicateInterest (inFace, header, origPacket, pitEntry);
      return;
    }

  Ptr<Packet> contentObject;
  Ptr<const ContentObject> contentObjectHeader; // used for tracing
  Ptr<const Packet> payload; // used for tracing
  boost::tie (contentObject, contentObjectHeader, payload) = m_contentStore->Lookup (header);
  if (contentObject != 0)
    {
      NS_ASSERT (contentObjectHeader != 0);

      FwHopCountTag hopCountTag;
      if (origPacket->PeekPacketTag (hopCountTag))
        {
          contentObject->AddPacketTag (hopCountTag);
        }

      pitEntry->AddIncoming (inFace/*, Seconds (1.0)*/);

      // Do data plane performance measurements
      WillSatisfyPendingInterest (0, pitEntry);

      // Actually satisfy pending interest
      SatisfyPendingInterest (0, contentObjectHeader, payload, contentObject, pitEntry);
      return;
    }

  if (similarInterest && ShouldSuppressIncomingInterest (inFace, header, origPacket, pitEntry))
    {
      pitEntry->AddIncoming (inFace/*, header->GetInterestLifetime ()*/);
      // update PIT entry lifetime
      pitEntry->UpdateLifetime (header->GetInterestLifetime ());

      // Suppress this interest if we're still expecting data from some other face
      NS_LOG_DEBUG ("Suppress interests");
      m_dropInterests (header, inFace);	
      NS_LOG_UNCOND ("Suppress interests");
      DidSuppressSimilarInterest (inFace, header, origPacket, pitEntry);
      return;
    }

  if (similarInterest)
    {
      DidForwardSimilarInterest (inFace, header, origPacket, pitEntry);
    }

  PropagateInterest (inFace, header, origPacket, pitEntry);
}

void
ForwardingStrategy::OnData (Ptr<Face> inFace,
                            Ptr<const ContentObject> header,
                            Ptr<Packet> payload,
                            Ptr<const Packet> origPacket)
{
  NS_LOG_FUNCTION (inFace << header->GetName () << payload << origPacket);
  m_inData (header, payload, inFace);
  

  // Lookup PIT entry
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
  if (pitEntry == 0)
    {
      bool cached = false;

      if (m_cacheUnsolicitedData)
        {
          FwHopCountTag hopCountTag;

          Ptr<Packet> payloadCopy = payload->Copy ();
          payloadCopy->RemovePacketTag (hopCountTag);

          // Optimistically add or update entry in the content store
          cached = m_contentStore->Add (header, payloadCopy);
        }
      else
        {
          // Drop data packet if PIT entry is not found
          // (unsolicited data packets should not "poison" content store)

          //drop dulicated or not requested data packet
          m_dropData (header, payload, inFace);
        }

      DidReceiveUnsolicitedData (inFace, header, payload, origPacket, cached);
      return;
    }
  else
    {
    	//Update counter
    	/////////////////////////////////////////////////////
    	Ptr<fib::Entry> fibEntry=pitEntry->GetFibEntry();
    	fibEntry->IncreaseData();
    	fib::FaceMetricContainer::type::index<fib::i_face>::type::iterator record
      = fibEntry->m_faces.get<fib::i_face> ().find (inFace);
      if(record==fibEntry->m_faces.get<fib::i_face> ().end ())
      {
      	NS_LOG_UNCOND("FIB does not exist");
      }
      
     	
     	bool update = true;
     	/*BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
     	{
     		if(fibEntry->m_faces.get<fib::i_face> ().find (incoming.m_face)
     		!= fibEntry->m_faces.get<fib::i_face> ().end ())
     		{
     			update = false;
     			break;
     		}
     	}*/
     	
     	if(update)
     	{
     		fibEntry->m_faces.modify (record,
                    ll::bind (&fib::FaceMetric::IncreaseDataIn, ll::_1));
	      if(header->GetCE()==1)
	      	fibEntry->m_faces.modify (record,
	                      ll::bind (&fib::FaceMetric::IncreaseDataCE, ll::_1));
     	}
   		
     	
      
      
    	/////////////////////////////////////////////////////
      bool cached = false;

      FwHopCountTag hopCountTag;
      if (payload->PeekPacketTag (hopCountTag))
        {
          Ptr<Packet> payloadCopy = payload->Copy ();
          payloadCopy->RemovePacketTag (hopCountTag);

          // Add or update entry in the content store
          cached = m_contentStore->Add (header, payloadCopy);
        }
      else
        {
          // Add or update entry in the content store
          cached = m_contentStore->Add (header, payload); // no need for extra copy
        }

      DidReceiveSolicitedData (inFace, header, payload, origPacket, cached);
    }

  while (pitEntry != 0)
    {
      // Do data plane performance measurements
      WillSatisfyPendingInterest (inFace, pitEntry);

      // Actually satisfy pending interest
      SatisfyPendingInterest (inFace, header, payload, origPacket, pitEntry);

      // Lookup another PIT entry
      pitEntry = m_pit->Lookup (*header);
    }
}

void
ForwardingStrategy::DidCreatePitEntry (Ptr<Face> inFace,
                                       Ptr<const Interest> header,
                                       Ptr<const Packet> origPacket,
                                       Ptr<pit::Entry> pitEntrypitEntry)
{
}

void
ForwardingStrategy::FailedToCreatePitEntry (Ptr<Face> inFace,
                                            Ptr<const Interest> header,
                                            Ptr<const Packet> origPacket)
{
  m_dropInterests (header, inFace);
}

void
ForwardingStrategy::DidReceiveDuplicateInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> header,
                                                 Ptr<const Packet> origPacket,
                                                 Ptr<pit::Entry> pitEntry)
{
  /////////////////////////////////////////////////////////////////////////////////////////
  //                                                                                     //
  // !!!! IMPORTANT CHANGE !!!! Duplicate interests will create incoming face entry !!!! //
  //                                                                                     //
  /////////////////////////////////////////////////////////////////////////////////////////
  pitEntry->AddIncoming (inFace);
  m_dropInterests (header, inFace);
}

void
ForwardingStrategy::DidSuppressSimilarInterest (Ptr<Face> face,
                                                Ptr<const Interest> header,
                                                Ptr<const Packet> origPacket,
                                                Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidForwardSimilarInterest (Ptr<Face> inFace,
                                               Ptr<const Interest> header,
                                               Ptr<const Packet> origPacket,
                                               Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidExhaustForwardingOptions (Ptr<Face> inFace,
                                                 Ptr<const Interest> header,
                                                 Ptr<const Packet> origPacket,
                                                 Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this << boost::cref (*inFace));
  if (pitEntry->AreAllOutgoingInVain ())
    {
      m_dropInterests (header, inFace);

      // All incoming interests cannot be satisfied. Remove them
      pitEntry->ClearIncoming ();

      // Remove also outgoing
      pitEntry->ClearOutgoing ();

      // Set pruning timout on PIT entry (instead of deleting the record)
      m_pit->MarkErased (pitEntry);
    }
}



bool
ForwardingStrategy::DetectRetransmittedInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> header,
                                                 Ptr<const Packet> packet,
                                                 Ptr<pit::Entry> pitEntry)
{
  pit::Entry::in_iterator existingInFace = pitEntry->GetIncoming ().find (inFace);

  bool isRetransmitted = false;

  if (existingInFace != pitEntry->GetIncoming ().end ())
    {
      // this is almost definitely a retransmission. But should we trust the user on that?
      isRetransmitted = true;
    }

  return isRetransmitted;
}

void
ForwardingStrategy::SatisfyPendingInterest (Ptr<Face> inFace,
                                            Ptr<const ContentObject> header,
                                            Ptr<const Packet> payload,
                                            Ptr<const Packet> origPacket,
                                            Ptr<pit::Entry> pitEntry)
{
	//if inFace==0, it is a cache hit
  if (inFace != 0)
    pitEntry->RemoveIncoming (inFace);

	Ptr<fib::Entry> fibEntry=pitEntry->GetFibEntry();
	/*if(inFace!=0)
		fibEntry->IncreaseData();*/
	
	fib::FaceMetricContainer::type::index<fib::i_face>::type::iterator record;
	if (inFace != 0)
	{
		record = fibEntry->m_faces.get<fib::i_face> ().find (inFace); 
		NS_ASSERT(record!=fibEntry->m_faces.get<fib::i_face> ().end ());
	}
	
	//Get total incoming data rate
	uint32_t max_data_in = 0;
	BOOST_FOREACH (const fib::FaceMetric &metricFace, fibEntry->m_faces.get<fib::i_metric> ())
	{
		max_data_in += metricFace.GetDataIn();
	}
	     
		
  //satisfy all pending incoming Interests
  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {
    	//by Felix: mark the data packet
    	////////////////////////////////////////////////////////////////////
    	Ptr<Packet> target = origPacket->Copy();
    	Ptr<ContentObject> NewHeader = Create<ContentObject> ();
    	target->RemoveHeader(*NewHeader);
    	
    	if(DynamicCast<AppFace>(incoming.m_face)==0)
    	{
    		
    		Ptr<fib2::Entry> fib2Entry=pitEntry->GetFib2Entry();	
	    	fib2::FaceMetricContainer::type::index<fib2::i_face>::type::iterator record2
	      = fib2Entry->m_faces.get<fib2::i_face> ().find (incoming.m_face); 
	      
	      //NS_LOG_UNCOND("node "<<inFace->GetNode()->GetId()<<" face "<<incoming.m_face->GetId());
	      NS_ASSERT(record2!=fib2Entry->m_faces.get<fib2::i_face> ().end ());
	      
	      //update dataout counter
	      fib2Entry->m_faces.modify (record2,
	                      ll::bind (&fib2::FaceMetric::IncreaseDataOut, ll::_1));
				//mark the data with probability
	      //we need to consider real BW consumption too	
	      uint32_t max_data_out = max_data_in;
	      //uint32_t max_data_out = inFace==0 ? 0 : record->GetDataIn();
	      
	      for (fib2::FaceMetricContainer::type::iterator face = fib2Entry->m_faces.begin ();
	       face != fib2Entry->m_faces.end ();
	       face++)
	    	{
	      	if(max_data_out<face->GetDataOut())
	      		//max_data_out = face->GetDataOut();
	      		max_data_out += face->GetDataOut();
	    	}
	    	
	    		if(max_data_out)
		    	{
		    		uint32_t N = rand()%max_data_out;
			    	if(N<=record2->GetDataOut())
			    		NewHeader->SetCE(1);
			    	else
			    		NewHeader->SetCE(0);
		    	}
		    	else	//no data forwarded yet
		    		NewHeader->SetCE(1);
	    		
	    	
    	}
    	else	//local application
    	{
    		if(inFace==0)	//local cache hit should not increase rate
    			NewHeader->SetCE(2);
    	}
    	
    	
    		
    	target->AddHeader(*NewHeader);	
    	////////////////////////////////////////////////////////////////////
      
      bool ok = incoming.m_face->Send (target);

      DidSendOutData (inFace, incoming.m_face, header, payload, origPacket, pitEntry);
      NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

      if (!ok)
        {
          m_dropData (header, payload, incoming.m_face);
          NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
        }
    }

  // All incoming interests are satisfied. Remove them
  pitEntry->ClearIncoming ();

  // Remove all outgoing faces
  pitEntry->ClearOutgoing ();

  // Set pruning timout on PIT entry (instead of deleting the record)
  m_pit->MarkErased (pitEntry);
}

void
ForwardingStrategy::DidReceiveSolicitedData (Ptr<Face> inFace,
                                             Ptr<const ContentObject> header,
                                             Ptr<const Packet> payload,
                                             Ptr<const Packet> origPacket,
                                             bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::DidReceiveUnsolicitedData (Ptr<Face> inFace,
                                               Ptr<const ContentObject> header,
                                               Ptr<const Packet> payload,
                                               Ptr<const Packet> origPacket,
                                               bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::WillSatisfyPendingInterest (Ptr<Face> inFace,
                                                Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator out = pitEntry->GetOutgoing ().find (inFace);

  // If we have sent interest for this data via this face, then update stats.
  if (out != pitEntry->GetOutgoing ().end ())
    {
      pitEntry->GetFibEntry ()->UpdateFaceRtt (inFace, Simulator::Now () - out->m_sendTime);
    }
}

bool
ForwardingStrategy::ShouldSuppressIncomingInterest (Ptr<Face> inFace,
                                                    Ptr<const Interest> header,
                                                    Ptr<const Packet> origPacket,
                                                    Ptr<pit::Entry> pitEntry)
{
  bool isNew = pitEntry->GetIncoming ().size () == 0 && pitEntry->GetOutgoing ().size () == 0;

  if (isNew) return false; // never suppress new interests

  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, header, origPacket, pitEntry);

  if (pitEntry->GetOutgoing ().find (inFace) != pitEntry->GetOutgoing ().end ())
    {
      NS_LOG_DEBUG ("Non duplicate interests from the face we have sent interest to. Don't suppress");
      // got a non-duplicate interest from the face we have sent interest to
      // Probably, there is no point in waiting data from that face... Not sure yet

      // If we're expecting data from the interface we got the interest from ("producer" asks us for "his own" data)
      // Mark interface YELLOW, but keep a small hope that data will come eventually.

      // ?? not sure if we need to do that ?? ...

      // pitEntry->GetFibEntry ()->UpdateStatus (inFace, fib::FaceMetric::NDN_FIB_YELLOW);
    }
  else
    if (!isNew && !isRetransmitted)
      {
        return true;
      }

  return false;
}

void
ForwardingStrategy::PropagateInterest (Ptr<Face> inFace,
                                       Ptr<const Interest> header,
                                       Ptr<const Packet> origPacket,
                                       Ptr<pit::Entry> pitEntry)
{
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, header, origPacket, pitEntry);

  pitEntry->AddIncoming (inFace/*, header->GetInterestLifetime ()*/);
  /// @todo Make lifetime per incoming interface
  pitEntry->UpdateLifetime (header->GetInterestLifetime ());

  bool propagated = DoPropagateInterest (inFace, header, origPacket, pitEntry);

  if (!propagated && isRetransmitted) //give another chance if retransmitted
    {
      // increase max number of allowed retransmissions
      pitEntry->IncreaseAllowedRetxCount ();
			
      // try again
      propagated = DoPropagateInterest (inFace, header, origPacket, pitEntry);
    }

  // if (!propagated)
  //   {
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //     NS_LOG_DEBUG ("+++ Not propagated ["<< header->GetName () <<"], but number of outgoing faces: " << pitEntry->GetOutgoing ().size ());
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //   }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  if (!propagated && pitEntry->AreAllOutgoingInVain ())
    {
      DidExhaustForwardingOptions (inFace, header, origPacket, pitEntry);
    }
}

bool
ForwardingStrategy::CanSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  if (outFace == inFace)
    {
      // NS_LOG_DEBUG ("Same as incoming");
      return false; // same face as incoming, don't forward
    }

  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outFace);

  if (outgoing != pitEntry->GetOutgoing ().end ())
    {
      if (!m_detectRetransmissions)
      {
        return false; // suppress
      } 
      else if (outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
        {
          NS_LOG_DEBUG ("Already forwarded before during this retransmission cycle (" <<outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount () << ")");
          return false; // already forwarded before during this retransmission cycle
        }
   }

  return true;
}


bool
ForwardingStrategy::TrySendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  if (!CanSendOutInterest (inFace, outFace, header, origPacket, pitEntry))
    {
      return false;
    }

  pitEntry->AddOutgoing (outFace); 

  //transmission
  Ptr<Packet> packetToSend = origPacket->Copy ();
  bool successSend = outFace->Send (packetToSend);
  if (!successSend)
    {
      m_dropInterests (header, outFace);
    }

  DidSendOutInterest (inFace, outFace, header, origPacket, pitEntry);

  return true;
}

void
ForwardingStrategy::DidSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  m_outInterests (header, outFace);
}

void
ForwardingStrategy::DidSendOutData (Ptr<Face> inFace,
                                    Ptr<Face> outFace,
                                    Ptr<const ContentObject> header,
                                    Ptr<const Packet> payload,
                                    Ptr<const Packet> origPacket,
                                    Ptr<pit::Entry> pitEntry)
{
  m_outData (header, payload, inFace == 0, outFace);
}

void
ForwardingStrategy::WillEraseTimedOutPendingInterest (Ptr<pit::Entry> pitEntry)
{
  // do nothing for now. may be need to do some logging
}

void
ForwardingStrategy::AddFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::RemoveFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::DidAddFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}

void
ForwardingStrategy::WillRemoveFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}

void
ForwardingStrategy::DidAddFib2Entry (Ptr<fib2::Entry> fib2Entry)
{
  // do nothing here
}

void
ForwardingStrategy::WillRemoveFib2Entry (Ptr<fib2::Entry> fib2Entry)
{
  // do nothing here
}


} // namespace ndn
} // namespace ns3
