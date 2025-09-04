#ifndef MULTI_HOP_LORA_HEADER_H
#define MULTI_HOP_LORA_HEADER_H

#include "ns3/header.h"
#include <vector>
#include <cstdint>

namespace ns3 {

class MultiHopLoraHeader: public Header
{
public:
    MultiHopLoraHeader();
    virtual ~MultiHopLoraHeader();

    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);
    virtual void Print(std::ostream &os) const;

    //Setters
    void SetLfid(uint32_t lfid);
    void SetLnid(uint32_t lnid);
    void SetLpty(uint8_t lpty); // PERBAIKAN: Mengganti nama dari SetLpth menjadi SetLpty
    void SetLh(uint8_t lh);
    void SetLgw(uint8_t lgw);
    void AddNodeToPath(uint32_t nodeId);

    //Getters
    uint32_t GetLfid(void) const;
    uint32_t GetLnid(void) const;
    uint8_t GetLpty(void) const;
    uint8_t GetLh(void) const;
    uint8_t GetLgw(void) const;
    const std::vector<uint32_t>& GetPath(void) const;

private:
    uint32_t m_lfid; //packet ID
    uint32_t m_lnid; //original sender ID
    uint8_t m_lpty; //packet type
    uint8_t m_lh; //hop count
    uint8_t m_lgw; //Distance to Gateway of the last hop
    std::vector<uint32_t> m_path; // PERBAIKAN: Mengganti tipe data dari uint32_t menjadi vector
};

} //namespace ns3

#endif //MULTI_HOP_LORA_HEADER_H