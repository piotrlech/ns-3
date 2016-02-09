/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Andrew McGregor
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
 * Codel, the COntrolled DELay Queueing discipline
 * Based on ns2 simulation code presented by Kathie Nichols
 *
 * This port based on linux kernel code by
 * Authors:	Dave Täht <d@taht.net>
 *		Eric Dumazet <edumazet@google.com>
 *
 * Ported to ns-3 by: Andrew McGregor <andrewmcgr@gmail.com>
*/

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/abort.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include "aqm.h"

NS_LOG_COMPONENT_DEFINE ("AqmQueue");

namespace ns3 {

/* borrowed from the linux kernel */
static inline uint32_t ReciprocalDivide (uint32_t A, uint32_t R)
{
  return (uint32_t)(((uint64_t)A * R) >> 32);
}

/* end kernel borrowings */

static uint32_t AqmGetTime (void)
{
  Time time = Simulator::Now ();
  uint64_t ns = time.GetNanoSeconds ();

  return ns >> AQM_SHIFT;
}

static uint32_t AqmGetContext (void)
{
  uint32_t ctx = Simulator::GetContext ();
  return ctx;
}

static void AqmSetWait (void)
{
  int interf = 2;

  Ptr<Node> node = NodeList::GetNode (AqmGetContext());
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  NS_LOG_DEBUG("AqmSetWait Context = " << AqmGetContext() << ", AqmGetTime = " << AqmGetTime() << ", Node = " << node << ", IP addr = " << ipv4->GetAddress (interf, 0).GetLocal () << ", GetHighDelay = " << AqmQueue::GetHighDelay ());
  Ptr<NetDevice> netDev =  node->GetDevice(interf);
  Ptr<Channel> P2Plink  =  netDev->GetChannel();
  P2Plink->SetAttribute(std::string("Delay"), TimeValue(AqmQueue::GetHighDelay ()));
  return;
}

static void AqmSetNoWait (void)
{
  int interf = 1;

  Ptr<Node> node = NodeList::GetNode (AqmGetContext());
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  NS_LOG_DEBUG("AqmSetNoWait Context = " << AqmGetContext() << ", AqmGetTime = " << AqmGetTime() << ", Node = " << node << ", IP addr = " << ipv4->GetAddress (interf, 0).GetLocal () << ", GetLowDelay = " << AqmQueue::GetLowDelay ());
  Ptr<NetDevice> netDev =  node->GetDevice(interf);
  Ptr<Channel> P2Plink  =  netDev->GetChannel();
  P2Plink->SetAttribute(std::string("Delay"), TimeValue(AqmQueue::GetLowDelay ()));
  //NS_LOG_DEBUG("GetLowDelay = " << AqmQueue::GetLowDelay ());
  return;
}

class AqmTimestampTag : public Tag
{
public:
  AqmTimestampTag ();
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  Time GetTxTime (void) const;
private:
  uint64_t m_creationTime;
};

AqmTimestampTag::AqmTimestampTag ()
  : m_creationTime (Simulator::Now ().GetTimeStep ())
{
}

TypeId
AqmTimestampTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AqmTimestampTag")
    .SetParent<Tag> ()
    .AddConstructor<AqmTimestampTag> ()
    .AddAttribute ("CreationTime",
                   "The time at which the timestamp was created",
                   StringValue ("0.0s"),
                   MakeTimeAccessor (&AqmTimestampTag::GetTxTime),
                   MakeTimeChecker ())
  ;
  return tid;
}

TypeId
AqmTimestampTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
AqmTimestampTag::GetSerializedSize (void) const
{
  return 8;
}
void
AqmTimestampTag::Serialize (TagBuffer i) const
{
  i.WriteU64 (m_creationTime);
}
void
AqmTimestampTag::Deserialize (TagBuffer i)
{
  m_creationTime = i.ReadU64 ();
}
void
AqmTimestampTag::Print (std::ostream &os) const
{
  os << "CreationTime=" << m_creationTime;
}
Time
AqmTimestampTag::GetTxTime (void) const
{
  return TimeStep (m_creationTime);
}

NS_OBJECT_ENSURE_REGISTERED (AqmQueue);

TypeId AqmQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AqmQueue")
    .SetParent<Queue> ()
    .AddConstructor<AqmQueue> ()
    .AddAttribute ("Mode",
                   "Whether to use Bytes (see MaxBytes) or Packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_BYTES),
                   MakeEnumAccessor (&AqmQueue::SetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets accepted by this AqmQueue.",
                   UintegerValue (DEFAULT_AQM_LIMIT),
                   MakeUintegerAccessor (&AqmQueue::m_maxPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBytes",
                   "The maximum number of bytes accepted by this AqmQueue.",
                   UintegerValue (1500 * DEFAULT_AQM_LIMIT),
                   MakeUintegerAccessor (&AqmQueue::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinBytes",
                   "The Aqm algorithm minbytes parameter.",
                   UintegerValue (1500),
                   MakeUintegerAccessor (&AqmQueue::m_minBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval",
                   "The Aqm algorithm interval",
                   StringValue ("100ms"),
                   MakeTimeAccessor (&AqmQueue::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("LowDelay",
                  "Low queue delay",
                   StringValue ("5ms"),
                   MakeTimeAccessor (&AqmQueue::m_lowDelay),
                   MakeTimeChecker ())
    .AddAttribute ("HighDelay",
                   "High queue delay",
                   StringValue ("200ms"),
                   MakeTimeAccessor (&AqmQueue::m_highDelay),
                   MakeTimeChecker ())
    .AddAttribute ("Target",
                   "The Aqm algorithm target queue delay",
                   StringValue ("5ms"),
                   MakeTimeAccessor (&AqmQueue::m_target),
                   MakeTimeChecker ())
//          .AddTraceSource ("Count",
//                           "CoDel count",
//                           MakeTraceSourceAccessor (&CoDelQueue::m_count),
//                           "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("Count",
                     "Aqm count",
                     MakeTraceSourceAccessor (&AqmQueue::m_count),
                     "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("DropCount",
                     "Aqm drop count",
                     MakeTraceSourceAccessor (&AqmQueue::m_dropCount),
                     "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("LastCount",
                     "Aqm lastcount",
                     MakeTraceSourceAccessor (&AqmQueue::m_lastCount),
                     "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("DropState",
                     "Dropping state",
                     MakeTraceSourceAccessor (&AqmQueue::m_dropping),
                     "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("BytesInQueue",
                     "Number of bytes in the queue",
                     MakeTraceSourceAccessor (&AqmQueue::m_bytesInQueue),
                     "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("Sojourn",
                     "Time in the queue",
                     MakeTraceSourceAccessor (&AqmQueue::m_sojourn),
                     "ns3::TracedValue::Uint32Callback")
    .AddTraceSource ("DropNext",
                     "Time until next packet drop",
                     MakeTraceSourceAccessor (&AqmQueue::m_dropNext),
                     "ns3::TracedValue::Uint32Callback")
  ;

  return tid;
}

AqmQueue::AqmQueue ()
  : Queue (),
    m_packets (),
    m_maxBytes (),
    m_bytesInQueue (0),
    m_count (0),
    m_dropCount (0),
    m_lastCount (0),
    m_dropping (false),
    m_recInvSqrt (~0U >> REC_INV_SQRT_SHIFT),
    m_firstAboveTime (0),
    m_dropNext (0),
    m_state1 (0),
    m_state2 (0),
    m_state3 (0),
    m_states (0),
    m_dropOverLimit (0),
    m_sojourn (0)
{
  NS_LOG_FUNCTION (this);
}

AqmQueue::~AqmQueue ()
{
  NS_LOG_FUNCTION (this);
}

void
AqmQueue::NewtonStep (void)
{
  NS_LOG_FUNCTION (this);
  uint32_t invsqrt = ((uint32_t) m_recInvSqrt) << REC_INV_SQRT_SHIFT;
  uint32_t invsqrt2 = ((uint64_t) invsqrt * invsqrt) >> 32;
  uint64_t val = (3ll << 32) - ((uint64_t) m_count * invsqrt2);

  val >>= 2; /* avoid overflow */
  val = (val * invsqrt) >> (32 - 2 + 1);
  m_recInvSqrt = val >> REC_INV_SQRT_SHIFT;
}

uint32_t
AqmQueue::ControlLaw (uint32_t t)
{
  NS_LOG_FUNCTION (this);
  return t + ReciprocalDivide (Time2Aqm (m_interval), m_recInvSqrt << REC_INV_SQRT_SHIFT);
}

void
AqmQueue::SetMode (AqmQueue::QueueMode mode)
{
  NS_LOG_FUNCTION (mode);
  m_mode = mode;
}

AqmQueue::QueueMode
AqmQueue::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

bool
AqmQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  sm_lowDelay = m_lowDelay;
  sm_highDelay = m_highDelay;
  if (m_mode == QUEUE_MODE_PACKETS && (m_packets.size () + 1 > m_maxPackets))
    {
      NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
      Drop (p);
      ++m_dropOverLimit;
      return false;
    }

  if (m_mode == QUEUE_MODE_BYTES && (m_bytesInQueue + p->GetSize () > m_maxBytes))
    {
      NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
      Drop (p);
      ++m_dropOverLimit;
      return false;
    }

  // Tag packet with current time for DoDequeue() to compute sojourn time
  AqmTimestampTag tag;
  p->AddPacketTag (tag);

  m_bytesInQueue += p->GetSize ();
  m_packets.push (p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return true;
}

bool
AqmQueue::OkToDrop (Ptr<Packet> p, uint32_t now)
{
  randv = randv * 1664525 + 1013904223;
  
  NS_LOG_FUNCTION (this);
  AqmTimestampTag tag;
  bool okToDrop;
  p->FindFirstMatchingByteTag (tag);
  bool found = p->RemovePacketTag (tag);
  NS_ASSERT_MSG (found, "found a packet without an input timestamp tag");
  NS_UNUSED (found);    //silence compiler warning
  Time delta = Simulator::Now () - tag.GetTxTime ();
  NS_LOG_INFO ("Sojourn time " << delta.GetSeconds ());
  m_sojourn = delta;
  uint32_t sojournTime = Time2Aqm (delta);

  if (AqmTimeBefore (sojournTime, Time2Aqm (m_target))
      || m_bytesInQueue < m_minBytes)
    {
      // went below so we'll stay below for at least q->interval
      NS_LOG_LOGIC ("Sojourn time is below target or number of bytes in queue is less than minBytes; packet should not be dropped");
      m_firstAboveTime = 0;
      return false;
    }
  okToDrop = false;
  if (m_firstAboveTime == 0)
    {
      /* just went above from below. If we stay above
       * for at least q->interval we'll say it's ok to drop
       */
      NS_LOG_LOGIC ("Sojourn time has just gone above target from below, need to stay above for at least q->interval before packet can be dropped. ");
      m_firstAboveTime = now + Time2Aqm (m_interval);
    }
  else
  if (AqmTimeAfter (now, m_firstAboveTime))
    {
      NS_LOG_LOGIC ("Sojourn time has been above target for at least q->interval; it's OK to (possibly) drop packet.");
      okToDrop = true;
      ++m_state1;
    }
  okToDrop = randv < 42949673;  // = 1% * 2^32
  if(okToDrop)
  {
      NS_LOG_LOGIC ("Drop it");
      //Ptr<Node> node = NodeList::GetNode (AqmGetContext());
      //Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      //NS_LOG_DEBUG("AqmGetContext = " << AqmGetContext() << ", AqmGetTime = " << AqmGetTime() << ", Node = " << node << ", IP addr = " << ipv4->GetAddress (1, 0).GetLocal ());
      if(m_highDelay > 0)
      {
        Simulator::ScheduleNow(&AqmSetWait);
        Time symTime = Simulator::Now () + Seconds (0.5);
        Simulator::Schedule(symTime, &AqmSetNoWait);
      }
  }
  return okToDrop;
}

Ptr<Packet>
AqmQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      // Leave dropping state when queue is empty
      m_dropping = false;
      m_firstAboveTime = 0;
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }
  uint32_t now = AqmGetTime ();
  Ptr<Packet> p = m_packets.front ();
  m_packets.pop ();
  m_bytesInQueue -= p->GetSize ();

  NS_LOG_LOGIC ("Popped " << p);
  NS_LOG_LOGIC ("Number packets remaining " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes remaining " << m_bytesInQueue);

  // Determine if p should be dropped
  bool okToDrop = OkToDrop (p, now);

  if (m_dropping)
    { // In the dropping state (sojourn time has gone above target and hasn't come down yet)
      // Check if we can leave the dropping state or next drop should occur
      NS_LOG_LOGIC ("In dropping state, check if it's OK to leave or next drop should occur");
      if (!okToDrop)
        {
          /* sojourn time fell below target - leave dropping state */
          NS_LOG_LOGIC ("Sojourn time goes below target, it's OK to leave dropping state.");
          m_dropping = false;
        }
      else
      if (AqmTimeAfterEq (now, m_dropNext))
        {
          m_state2++;
          while (m_dropping && AqmTimeAfterEq (now, m_dropNext))
            {
              // It's time for the next drop. Drop the current packet and
              // dequeue the next. The dequeue might take us out of dropping
              // state. If not, schedule the next drop.
              // A large amount of packets in queue might result in drop
              // rates so high that the next drop should happen now,
              // hence the while loop.
              NS_LOG_LOGIC ("Sojourn time is still above target and it's time for next drop; dropping " << p);
              Drop (p);
              ++m_dropCount;
              ++m_count;
              NewtonStep ();
              if (m_packets.empty ())
                {
                  m_dropping = false;
                  NS_LOG_LOGIC ("Queue empty");
                  ++m_states;
                  return 0;
                }
              p = m_packets.front ();
              m_packets.pop ();
              m_bytesInQueue -= p->GetSize ();

              NS_LOG_LOGIC ("Popped " << p);
              NS_LOG_LOGIC ("Number packets remaining " << m_packets.size ());
              NS_LOG_LOGIC ("Number bytes remaining " << m_bytesInQueue);

              if (!OkToDrop (p, now))
                {
                  /* leave dropping state */
                  NS_LOG_LOGIC ("Leaving dropping state");
                  m_dropping = false;
                }
              else
                {
                  /* schedule the next drop */
                  NS_LOG_LOGIC ("Running ControlLaw for input m_dropNext: " << (double)m_dropNext / 1000000);
                  m_dropNext = ControlLaw (m_dropNext);
                  NS_LOG_LOGIC ("Scheduled next drop at " << (double)m_dropNext / 1000000);
                }
            }
        }
    }
  else
    {
      // Not in the dropping state
      // Decide if we have to enter the dropping state and drop the first packet
      NS_LOG_LOGIC ("Not in dropping state; decide if we have to enter the state and drop the first packet");
      if (okToDrop)
        {
          // Drop the first packet and enter dropping state unless the queue is empty
          NS_LOG_LOGIC ("Sojourn time goes above target, dropping the first packet " << p << " and entering the dropping state");
          ++m_dropCount;
          Drop (p);
          if (m_packets.empty ())
            {
              m_dropping = false;
              okToDrop = false;
              NS_LOG_LOGIC ("Queue empty");
              ++m_states;
            }
          else
            {
              p = m_packets.front ();
              m_packets.pop ();
              m_bytesInQueue -= p->GetSize ();

              NS_LOG_LOGIC ("Popped " << p);
              NS_LOG_LOGIC ("Number packets remaining " << m_packets.size ());
              NS_LOG_LOGIC ("Number bytes remaining " << m_bytesInQueue);

              okToDrop = OkToDrop (p, now);
              m_dropping = true;
            }
          ++m_state3;
          /*
           * if min went above target close to when we last went below it
           * assume that the drop rate that controlled the queue on the
           * last cycle is a good starting point to control it now.
           */
          int delta = m_count - m_lastCount;
          if (delta > 1 && AqmTimeBefore (now - m_dropNext, 16 * Time2Aqm (m_interval)))
            {
              m_count = delta;
              NewtonStep ();
            }
          else
            {
              m_count = 1;
              m_recInvSqrt = ~0U >> REC_INV_SQRT_SHIFT;
            }
          m_lastCount = m_count;
          NS_LOG_LOGIC ("Running ControlLaw for input now: " << (double)now);
          m_dropNext = ControlLaw (now);
          NS_LOG_LOGIC ("Scheduled next drop at " << (double)m_dropNext / 1000000 << " now " << (double)now / 1000000);
        }
    }
  ++m_states;
  return p;
}

uint32_t
AqmQueue::GetQueueSize (void)
{
  NS_LOG_FUNCTION (this);
  if (GetMode () == QUEUE_MODE_BYTES)
    {
      return m_bytesInQueue;
    }
  else if (GetMode () == QUEUE_MODE_PACKETS)
    {
      return m_packets.size ();
    }
  else
    {
      NS_ABORT_MSG ("Unknown mode.");
    }
}

uint32_t
AqmQueue::GetDropOverLimit (void)
{
  return m_dropOverLimit;
}

uint32_t
AqmQueue::GetDropCount (void)
{
  return m_dropCount;
}

Time AqmQueue::sm_lowDelay;
Time AqmQueue::sm_highDelay;

Time
AqmQueue::GetLowDelay (void)
{
  return sm_lowDelay;
}

Time
AqmQueue::GetHighDelay (void)
{
  return sm_highDelay;
}

Time
AqmQueue::GetTarget (void)
{
  return m_target;
}

Time
AqmQueue::GetInterval (void)
{
  return m_interval;
}

uint32_t
AqmQueue::GetDropNext (void)
{
  return m_dropNext;
}

Ptr<const Packet>
AqmQueue::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.front ();

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return p;
}

bool
AqmQueue::AqmTimeAfter (uint32_t a, uint32_t b)
{
  return  ((int)(a) - (int)(b) > 0);
}

bool
AqmQueue::AqmTimeAfterEq (uint32_t a, uint32_t b)
{
  return ((int)(a) - (int)(b) >= 0);
}

bool
AqmQueue::AqmTimeBefore (uint32_t a, uint32_t b)
{
  return  ((int)(a) - (int)(b) < 0);
}

bool
AqmQueue::AqmTimeBeforeEq (uint32_t a, uint32_t b)
{
  return ((int)(a) - (int)(b) <= 0);
}

uint32_t
AqmQueue::Time2Aqm (Time t)
{
  return (t.GetNanoSeconds () >> AQM_SHIFT);
}


} // namespace ns3

