#include "iCachePort.hh"

namespace Emulator
{

iCachePort::iCachePort(uint64_t Latency, BaseDRAM* dram)
    : TraceObject("iCachePort"),
      m_iCacheRespLatch("iCache",Latency),
      m_baseDRAM(dram)
{}

iCachePort::~iCachePort(){}

void 
iCachePort::ReceiveFetchReq(MemReq_t mem_req,std::function<void(MemResp_t)> CallBackFunc){
    MemResp_t resp;
    resp.Id         = mem_req.Id;
    resp.Address    = mem_req.Address;
    resp.Opcode     = mem_req.Opcode;
    resp.Length     = mem_req.Length;
    resp.Data       = NULL;
    if(this->m_baseDRAM->checkRange(mem_req.Address)){
        resp.Excp.valid = false;
        resp.Excp.Cause = 0;
        resp.Excp.Tval  = 0;
        this->m_baseDRAM->read(mem_req.Address,&resp.Data,mem_req.Length);
    }else{
        resp.Excp.valid = true;
        resp.Excp.Cause = RISCV::INSTR_ACCESS_FAULT;
        resp.Excp.Tval  = mem_req.Address;
    }
    this->m_iCacheRespLatch.InPort->set({CallBackFunc,resp});
    DPRINTF(ICacheReq,"address {:#x}",mem_req.Address);
}

void 
iCachePort::Reset(){
    this->m_iCacheRespLatch.reset();
}   

void 
iCachePort::Evaluate(){
    if(this->m_iCacheRespLatch.OutPort->valid){
        this->m_iCacheRespLatch.OutPort->data.first(this->m_iCacheRespLatch.OutPort->data.second);
    }
}

void 
iCachePort::Advance(){
    this->m_iCacheRespLatch.advance();
}
    
} // namespace Emulator
