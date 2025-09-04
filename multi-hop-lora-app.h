#ifndef MULTI_HOP_LORA_APP_H
#define MULTI_HOP_LORA_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/lora-net-device.h"
#include "multi-hop-lora-header.h"
#include <map>
#include <vector>

namespace ns3{

class MultiHopLoraApp: public Application
{
public:
    static TypeId GetTypeId(void);
    MultiHopLoraApp();
    virtual ~MultiHopLoraApp();

    // PERBAIKAN: Mengubah tipe data packetInterval menjadi double agar konsisten
    void Setup(uint32_t nodeId, uint8_t lgw, bool isGateway, bool isSource, double packetInterval, uint32_t packetSize);

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

private:
    //packet generation and sending
    void ScheduleTx(void);
    void SendPacket(void);

    //packet reception and processing
    void ReceivePacket(Ptr<Socket> socket);
    void ProcessDuplicates (uint32_t lfid);

    //node configuration
    uint32_t m_nodeId;
    uint8_t m_lgw;
    bool m_isGateway;
    bool m_isSource;

    //application parameters
    Time m_packetInterval;
    uint32_t m_packetSize;
    uint32_t m_packetsSent;
    uint32_t m_lfidCounter;

    //socket and network
    Ptr<Socket> m_socket;
    Address m_broadcastAddress;

    //forwarding logic state
    std::map<uint32_t, Time> m_packetCache;
    std::map<uint32_t, std::vector<Ptr<Packet>>> m_dupllicateBuffer;

    //simulation control
    EventId m_sendEvent;

    //constants from paper
    static const uint8_t MAX_HOPS = 10;
    static const uint8_t RETREAT_FACTOR = 1;
    static const Time MIN_WAIT;
    static const Time MAX_WAIT;
};

} //namespace ns3

#endif //MULTI_HOP_LORA_APP_H