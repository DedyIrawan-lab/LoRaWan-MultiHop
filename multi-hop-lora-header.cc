#include "multi-hop-lora-header.h"
#include "ns3/log.h"
#include "ns3/assert.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MultiHopLoraHeader");
NS_OBJECT_ENSURE_REGISTERED(MultiHopLoraHeader);

TypeId
MultiHopLoraHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MultiHopLoraHeader")
    .SetParent<Header>()
    .SetGroupName("lorawan")
    .AddConstructor<MultiHopLoraHeader>()
    ;
    return tid;
}

TypeId
MultiHopLoraHeader::GetInstanceTypeId(void)
{
    return GetTypeId();
}

// PERBAIKAN: Menginisialisasi m_path sebagai vector kosong
MultiHopLoraHeader::MultiHopLoraHeader():m_lfid(0),m_lnid(0),m_lpty(0),m_lh(0),m_lgw(0),m_path()
{}

MultiHopLoraHeader::~MultiHopLoraHeader()
{}

uint32_t MultiHopLoraHeader::GetSerializedSize(void) const
{
    //Fixed part: lfid(4)+lnid(4)+lpty(1)+lh(1)+lgw(1)= 11 bytes
    //Variable part: path size (1 byte for count)+ path elements(4 bytes each)
    return 11 + 1 + m_path.size() * sizeof(uint32_t);
}

void
MultiHopLoraHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU32(m_lfid);
    start.WriteHtonU32(m_lnid);
    start.WriteU8(m_lpty);
    start.WriteU8(m_lh);
    start.WriteU8(m_lgw);

    //Serialize the path vector
    uint8_t pathSize = m_path.size();
    start.WriteU8(pathSize);
    for(uint32_t nodeId:m_path)
    {
        start.WriteHtonU32(nodeId);
    }
}

uint32_t
MultiHopLoraHeader::Deserialize(Buffer::Iterator start)
{
    // Must read the fixed part first to determine the variable part's size
    m_lfid = start.ReadNtohU32();
    m_lnid = start.ReadNtohU32();
    m_lpty = start.ReadU8();
    m_lh = start.ReadU8();
    m_lgw = start.ReadU8();

    //deserialize the path vector
    m_path.clear();
    uint8_t pathSize = start.ReadU8();
    for(uint8_t i = 0; i < pathSize; ++i)
    {
        m_path.push_back(start.ReadNtohU32());
    }

    uint32_t bytesConsumed = 11 + 1 + pathSize * sizeof(uint32_t);
    return bytesConsumed;
}

void
MultiHopLoraHeader::Print (std::ostream &os) const
{
    os << "LFId=" << m_lfid << ", LNId=" << m_lnid << ", LH=" << (int)m_lh << ", LGw=" << (int)m_lgw;
    os << ", Path=[";
    for (size_t i = 0; i < m_path.size(); ++i)
    {
        os << m_path[i] << (i == m_path.size() - 1? "" : ",");
    }
    os << "]";
}

// Setters implementation
void MultiHopLoraHeader::SetLfid (uint32_t lfid) { m_lfid = lfid; }
void MultiHopLoraHeader::SetLnid (uint32_t lnid) { m_lnid = lnid; }
void MultiHopLoraHeader::SetLpty (uint8_t lpty) { m_lpty = lpty; }
void MultiHopLoraHeader::SetLh (uint8_t lh) { m_lh = lh; }
void MultiHopLoraHeader::SetLgw (uint8_t lgw) { m_lgw = lgw; }
void MultiHopLoraHeader::AddNodeToPath (uint32_t nodeId) { m_path.push_back(nodeId); }

// Getters implementation
uint32_t MultiHopLoraHeader::GetLfid (void) const { return m_lfid; }
uint32_t MultiHopLoraHeader::GetLnid (void) const { return m_lnid; }
uint8_t MultiHopLoraHeader::GetLpty (void) const { return m_lpty; }
uint8_t MultiHopLoraHeader::GetLh (void) const { return m_lh; }
uint8_t MultiHopLoraHeader::GetLgw (void) const { return m_lgw; }
const std::vector<uint32_t>& MultiHopLoraHeader::GetPath (void) const { return m_path; }

} // namespace ns3
// PERBAIKAN: Menghapus kurung kurawal berlebih