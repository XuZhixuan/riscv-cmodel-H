#ifndef __DCACHEPORT_HH__
#define __DCACHEPORT_HH__ 

#include "../Pipeline/Pipe_data.hh"
#include "../../Common/Common.hh"
#include "../../Memory/BaseDRAM.hh"
//#include "../../CLINT/BaseCLINT.hh"

namespace Emulator
{
class BaseCLINT;

class dCachePort : public Trace::TraceObject
{
private:

    BaseDRAM*              m_baseDRAM;
    BaseCLINT*             m_baseCLINT;

    TimeBuffer<std::pair<std::function<void(MemResp_t)>,MemResp_t>>  m_dCacheRespLatch;	
public:
    
    dCachePort(uint64_t Latency , BaseDRAM* dram, BaseCLINT* clint);
    
    ~dCachePort();

    void ReceiveMemReq(MemReq_t mem_req, std::function<void(MemResp_t)> CallBackfunc);

	MemResp_t ReceivePTWReq(MemReq_t mem_req, std::function<void(MemResp_t)> CallBackfunc,uint64_t& _pteBits);

    void Reset();

    void Evaluate();

    void Advance();

    bool ReservationValidGet(Addr_t addr);
    
    bool ReservationValidSet(Addr_t addr, bool valid);
    
	void AmoOpCal(MemReq_t &mem_req, char* storeVal, uint64_t& op1, uint64_t& op2);
    bool AmoIsSc(InsnPtr_t insn);
};


} // namespace Emulator




#endif	
