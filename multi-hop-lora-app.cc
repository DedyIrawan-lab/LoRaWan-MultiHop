#include "multi-hop-lora-app.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/lora-phy.h"
#include "ns3/random-variable-stream.h"

namespace ns3{

NS_LOG_COMPONENT_DEFINE("MultiHopLoraApp");
NS_OBJECT_ENSURE_REGISTERED(MultiHopLoraApp);

//initialize static constants
const Time MultiHopLoraApp::MIN_WAIT = MilliSeconds(61); //based on ToA for SF7, 11 bytes
const Time MultiHopLoraApp::MAX_WAIT = MilliSeconds(183); //3 * ToA

TypeId
MultiHopLoraApp::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MultiHopLoraApp")
    .SetParent<Application>()
    .SetGroupName("lorawan")
    .AddConstructor<MultiHopLoraApp>()
    ;
    return tid;
}

MultiHopLoraApp::MultiHopLoraApp(): m_nodeId(0),m_lgw(255),m_isGateway(false),m_isSource(false),m_packetInterval(Seconds(20.0)),m_packetSize(32),m_packetsSent(0),m_lfidCounter(0)
{}

MultiHopLoraApp::~MultiHopLoraApp()
{
    m_socket = 0;
}

void
MultiHopLoraApp::Setup(uint32_t nodeId, uint8_t lgw, bool isGateway, bool isSource, double packetInterval, uint32_t packetSize)
{
    m_nodeId = nodeId;
    m_lgw = lgw;
    m_isGateway = isGateway;
    m_isSource = isSource;
    m_packetInterval = Seconds(packetInterval); // PERBAIKAN: Mengonversi double ke Time
    m_packetSize = packetSize;
    m_lfidCounter = m_nodeId << 16; //ensure unique LFIDs per node
}

void
MultiHopLoraApp::StartApplication(void)
{
    // ... (kode tidak berubah)
    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);

        //we are not using IP, so we use packet socket over the LoraNetDevice
        //the address is the device's MAC address
        //to broadcast, we need the broadcast MAC address
        Ptr<NetDevice> dev = GetNode()->GetDevice(0);
        m_broadcastAddress = dev->GetBroadcast();

        m_socket->Bind(); //bind to any available port
        m_socket->SetAllowBroadcast(true);
        m_socket->SetRecvCallback(MakeCallback(&MultiHopLoraApp::ReceivePacket, this));
    }

    if (m_isSource)
    {
        ScheduleTx();
    }
}

void
MultiHopLoraApp::StopApplication(void)
{
    // ... (kode tidak berubah)
    Simulator::Cancel(m_sendEvent);
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>);
        m_socket = 0;
    }
}

void
MultiHopLoraApp::ScheduleTx(void)
{
    m_sendEvent = Simulator::Schedule(m_packetInterval, &MultiHopLoraApp::SendPacket, this);
}

void
MultiHopLoraApp::SendPacket(void)
{
    // ... (kode tidak berubah)
    NS_LOG_FUNCTION(this);
    MultiHopLoraHeader header;
    header.SetLfid(m_lfidCounter++);
    header.SetLnid(m_nodeId);
    header.SetLpty(1); //type 1 for data
    header.SetLh(1); //first hop
    header.SetLgw(m_lgw);
    header.AddNodeToPath(m_nodeId);

    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    packet->AddHeader(header);

    m_socket->SendTo(packet, 0, m_broadcastAddress);
    m_packetsSent++;

    NS_LOG_INFO("Node " << m_nodeId << "(Source) sent packet with LFID " << header.GetLfid() << " at " << Simulator::Now().GetSeconds() << "s");

    ScheduleTx();
}

void
MultiHopLoraApp::ReceivePacket(Ptr<Socket> socket)
{
    // ... (kode tidak berubah)
    NS_LOG_FUNCTION(this);
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        MultiHopLoraHeader header;
        packet->PeekHeader(header);

        if (m_isGateway)
        {
            NS_LOG_INFO("Gateway " << m_nodeId << " received final packet with LFID " << header.GetLfid() << " from path:");
            header.Print(std::cout);
            std::cout << std::endl;
            continue; //gateway is a sink, does not forward
        }

        //check cache (pseudo step 1)
        if (m_packetCache.find(header.GetLfid()) != m_packetCache.end())
        {
            //LFID is in cache. if it's a new arrival within the waiting window, buffer it.
            //otherwise, it's an old packet,, sso ignore
            if (Simulator::Now() <= m_packetCache[header.GetLfid()])
            {
                m_dupllicateBuffer[header.GetLfid()].push_back(packet->Copy());
            }
            continue;
        }
        
        //check max hops (pseudocode step 1)
        if (header.GetLh() >= MAX_HOPS)
        {
            NS_LOG_INFO("Node " << m_nodeId << " dropped packet with LFID " << header.GetLfid() << " due to max hops.");
            continue;
        }
        
        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
        Time waitTime = MIN_WAIT + MilliSeconds(rand->GetValue() * (MAX_WAIT.GetMilliSeconds() - MIN_WAIT.GetMilliSeconds()));
        Time expiryTime = Simulator::Now() + waitTime;
        
        m_packetCache[header.GetLfid()] = expiryTime;
        m_dupllicateBuffer[header.GetLfid()].push_back(packet->Copy());

        Simulator::Schedule(waitTime, &MultiHopLoraApp::ProcessDuplicates, this, header.GetLfid());
        NS_LOG_INFO("Node " << m_nodeId << " received new packet LFID " << header.GetLfid() << ". Waiting for " << waitTime.GetSeconds() << "s to process duplicates.");   
    }    
}

void
MultiHopLoraApp::ProcessDuplicates(uint32_t lfid) // PERBAIKAN: Menambahkan parameter lfid
{
    NS_LOG_FUNCTION(this << lfid);

    auto it = m_dupllicateBuffer.find(lfid);
    if (it == m_dupllicateBuffer.end() || it->second.empty())
    {
        return; //no packets to process
    }

    std::vector<Ptr<Packet>> candidates = it->second;
    m_dupllicateBuffer.erase(it);

    //---Start Forwarding Policy Logic from Paper---//

    //Create F1 (pseudocode step 3)
    std::vector<Ptr<Packet>> F1;
    for (const auto &pkt : candidates)
    {
        MultiHopLoraHeader hdr;
        pkt->PeekHeader(hdr);
        if (hdr.GetLgw() + RETREAT_FACTOR > m_lgw)
        {
            F1.push_back(pkt);
        }
    }

    if (F1.empty())
    {
        NS_LOG_INFO("Node " << m_nodeId  << " discarded LFID " << lfid << ": No packets satisfied spatial constraint.");
        return;
    }
    
    //create F2 (pseudocode step 4)
    std::vector<Ptr<Packet>> F2;
    uint8_t minHops = 255;
    for (const auto& pkt:F1)
    {
        MultiHopLoraHeader hdr;
        pkt->PeekHeader(hdr);
        if (hdr.GetLh() < minHops)
        {
            minHops = hdr.GetLh();
        }
    }
    for (const auto& pkt:F1)
    {
        MultiHopLoraHeader hdr;
        pkt->PeekHeader(hdr);
        if (hdr.GetLh() == minHops)
        {
            F2.push_back(pkt);
        }
    }

    //final selection (pseudocode step 4)
    Ptr<Packet> bestPacket = nullptr;
    if (!F2.empty())
    {
        if (F2.size() == 1)
        {
            bestPacket = F2.front(); // PERBAIKAN: Mengambil elemen pertama
        }
        else
        {
            uint8_t maxLgw = 0;
            for (const auto& pkt: F2)
            {
                MultiHopLoraHeader hdr;
                pkt->PeekHeader(hdr);
                if (hdr.GetLgw() > maxLgw)
                {
                    maxLgw = hdr.GetLgw();
                    bestPacket = pkt;
                }
            }
        }
    }

    //forward or discard
    if (bestPacket)
    {
        Ptr<Packet> packetToForward = bestPacket->Copy();
        MultiHopLoraHeader header;
        packetToForward->RemoveHeader(header);

        //update header for forwarding
        header.SetLh(header.GetLh() + 1);
        header.SetLgw(m_lgw); //update lgw to this node's value
        header.AddNodeToPath(m_nodeId);

        packetToForward->AddHeader(header);

        m_socket->SendTo(packetToForward, 0, m_broadcastAddress);
        NS_LOG_INFO("Node " << m_nodeId << " forwarded packet with LFID " << lfid << ". New hop count: " << (int)header.GetLh());
    }
    else
    {
        NS_LOG_INFO("Node " << m_nodeId << " discarded LFID " << lfid << " after filtering");
    }
}

} //namespace ns3