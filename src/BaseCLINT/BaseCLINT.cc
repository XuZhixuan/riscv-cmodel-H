#include "BaseCLINT.hh"


namespace Emulator
{
class Simulator;    
BaseCLINT::BaseCLINT( Simulator* sim):TraceObject("CLINT"),m_sim(sim),m_Interleave(5000)
{

}
void BaseCLINT::Init(const YAML::Node& Config){
  m_ThreadCnt = m_sim->getThreads().size();
  
  m_CsrRegMtime = 0;
  m_CsrRegMtimecmp.resize(m_ThreadCnt, 0);
  m_CsrRegMsip.resize(m_ThreadCnt, 0);
  
  const YAML::Node CLINT_cfg = Config["CLINT"];
  m_MsipBaseAddr          = CLINT_cfg["MsipBaseAddr"].as<uint64_t>();
  m_MtimecmpBaseAddr      = CLINT_cfg["MtimecmpBaseAddr"].as<uint64_t>();
  m_MtimeBaseAddr         = CLINT_cfg["MtimeBaseAddr"].as<uint64_t>();
  m_MtimeTickPeriodType   = CLINT_cfg["MtimeTickPeriodType"].as<uint32_t>();
  m_Interleave            = CLINT_cfg["Interleave"].as<uint64_t>();
  
  DPRINTFF(CLINT, "BaseCLINT constructor: m_MtimeTickPeriodType {}", m_MtimeTickPeriodType);
  //
  m_MtimeTickPeriod = 1;
  m_LastMinstret.resize(m_ThreadCnt, 0);
  if(m_MtimeTickPeriodType == 1){
      m_MtimeTickPeriod = 1;
      for(int tid=0; tid<m_ThreadCnt; tid++){
          m_LastMinstret[tid] = 0;
      }
  }

}
bool BaseCLINT::checkRange(uint64_t address){
    if((address >= m_MsipBaseAddr) 
        && (address < m_MsipBaseAddr + m_ThreadCnt*sizeof(msip_t))){
        return true;
    }else if((address >= m_MtimeBaseAddr) 
        && (address < m_MtimeBaseAddr + sizeof(mtime_t))){
        return true;
    }else if((address >= m_MtimecmpBaseAddr) 
        && (address < m_MtimecmpBaseAddr + m_ThreadCnt*sizeof(mtimecmp_t))){
        return true;
    }else {
        //DASSERT(false, "checkRange Access Unknown CLINT Address : {:#x}",address);
        return false;
    }
}

void BaseCLINT::read(uint64_t address,  char* data, uint16_t len)
{
    uint64_t endAddr = address+len;
    if((address >= m_MsipBaseAddr) 
        && (endAddr <= m_MsipBaseAddr + m_ThreadCnt*sizeof(msip_t))){
      
        //cal the threadID according to the address
        uint32_t start_threadID = (address-m_MsipBaseAddr)/sizeof(msip_t);
        uint32_t count_threadID = len/sizeof(msip_t);
        for(int i=0; i<count_threadID; i++){
            xlen_t CsrMipVal = 0;
            this->m_sim->getThreads()[start_threadID+i]->m_ExecContext->ReadCsr(XCSR_MIP, CsrMipVal);
           if(CsrMipVal& XMIP_MSIP){
                m_CsrRegMsip[start_threadID+i] = 1;
            }else{
                m_CsrRegMsip[start_threadID+i] = 0;
            }
        }

        memcpy(data, &m_CsrRegMsip[start_threadID], len);
        return ;
    }else if((address >= m_MtimeBaseAddr) 
        && (endAddr <= m_MtimeBaseAddr + sizeof(mtime_t))){
        
        uint32_t start_threadID = (address-m_MtimeBaseAddr)/sizeof(mtime_t);
        memcpy(data, &m_CsrRegMtime, len);
        return ;
    }else if((address >= m_MtimecmpBaseAddr) 
        && (endAddr <= m_MtimecmpBaseAddr + m_ThreadCnt*sizeof(mtimecmp_t))){
        
        uint32_t start_threadID = (address-m_MtimecmpBaseAddr)/sizeof(mtimecmp_t);
        memcpy(data, &m_CsrRegMtimecmp[start_threadID], len);
        return ;
    }else{
        //DASSERT(false, "Access Unknown CLINT read Address : {:#x}",address);
        return ;
    }
}

void BaseCLINT::write(uint64_t address, const char* data, const uint16_t len, const uint64_t mask)
{
    uint32_t maskLen = 1;
    for(;maskLen<8;maskLen++){
        if(!((mask >> maskLen)&0x1)){
            break;
        }
    }
    uint64_t endAddr = address+maskLen;
    if((address >= m_MsipBaseAddr) 
        && (endAddr <= m_MsipBaseAddr + m_ThreadCnt*sizeof(msip_t))){
        
        //cal the threadID according to the address
        uint32_t start_threadID = (address-m_MsipBaseAddr)/sizeof(msip_t);
        uint32_t count_threadID = maskLen/sizeof(msip_t);
        m_CsrRegMsip[start_threadID] = 0;
        memcpy(&m_CsrRegMsip[start_threadID], data, maskLen);
        for(int i=0; i<count_threadID; i++){
            xlen_t CsrMipVal = 0;
            this->m_sim->getThreads()[start_threadID+i]->m_ExecContext->ReadCsr(XCSR_MIP, CsrMipVal);
            if(m_CsrRegMsip[start_threadID+i]){
                CsrMipVal = CsrMipVal|XMIP_MSIP;
                this->m_sim->getThreads()[start_threadID+i]->m_ExecContext->WriteCsr(XCSR_MIP, CsrMipVal);
            }
        }
        return ;
    }else if((address >= m_MtimeBaseAddr) 
        && (endAddr <= m_MtimeBaseAddr + sizeof(mtime_t))){
        memcpy(&m_MtimeBaseAddr, data, maskLen);

        return ;
    }else if((address >= m_MtimecmpBaseAddr) 
        && (endAddr <= m_MtimecmpBaseAddr + m_ThreadCnt*sizeof(mtimecmp_t))){
        uint64_t val = *(uint64_t*)data;
        //val = 50*val;
        uint32_t start_threadID = (address-m_MtimecmpBaseAddr)/sizeof(mtimecmp_t);
        memcpy(&m_CsrRegMtimecmp[start_threadID], (const char*)&val, maskLen);

        return ;
    }else{
        DASSERT(false, "Access Unknown CLINT write Address : {:#x}",address);
        return ;
    }
}

void BaseCLINT::Tick(){
    if(m_MtimeTickPeriodType == 0){
        m_CsrRegMtime += m_MtimeTickPeriod;
    }else if(m_MtimeTickPeriodType == 1){
        xlen_t CsrMinstretVal = 0;
        this->m_sim->getThreads()[0]->m_ExecContext->ReadCsr(XCSR_MINSTRET, CsrMinstretVal);
        uint64_t cnt = CsrMinstretVal - m_LastMinstret[0];
        m_CsrRegMtime += cnt;
        m_LastMinstret[0] = CsrMinstretVal;
    }
    
    for(int tid=0; tid<m_ThreadCnt; tid++){
        xlen_t CsrMipVal = 0;
        this->m_sim->getThreads()[tid]->m_ExecContext->ReadCsr(XCSR_MIP, CsrMipVal);
        //take interrupt or clear the csr MTIP
        if(!((m_CsrRegMtime+1)%m_Interleave)){
            if(m_CsrRegMtime >= m_CsrRegMtimecmp[tid]){
            DPRINTFF(CLINT, "BaseCLINT Tick:{}  mtimecmp {}",
                m_CsrRegMtime, m_CsrRegMtimecmp[tid]);
                CsrMipVal = CsrMipVal|XMIP_MTIP;
            }else{
                CsrMipVal = CsrMipVal&(~XMIP_MTIP);
            }
        }
        this->m_sim->getThreads()[tid]->m_ExecContext->WriteCsr(XCSR_MIP, CsrMipVal);
    }
}
void BaseCLINT::reset(){
    m_ThreadCnt = this->m_sim->getThreads().size();

    m_CsrRegMtime = 0;
    m_CsrRegMtimecmp.resize(m_ThreadCnt, 0);
    m_CsrRegMsip.resize(m_ThreadCnt, 0);

}

}
