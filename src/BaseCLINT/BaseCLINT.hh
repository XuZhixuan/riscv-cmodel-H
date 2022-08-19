#ifndef __BASECLINT_HH__
#define __BASECLINT_HH__ 

#include "../Processor/Processor.hh"
#include "../Memory/BaseDRAM.hh"
#include "../Common/Common.hh"
#include "../RISCV/encoding.out.h"
#include "../Simulator.hh"


typedef uint64_t mtime_t;
typedef uint64_t mtimecmp_t;
typedef uint32_t msip_t;



namespace Emulator
{

class BaseCLINT : public Trace::TraceObject
{
private:
    Simulator* m_sim;

    //regs to store csr
    mtime_t m_CsrRegMtime;
    std::vector<mtimecmp_t>     m_CsrRegMtimecmp;
    std::vector<msip_t>     m_CsrRegMsip;
    
    //regs memory mapped space

    /*
     *for example:
     *m_MsipBaseAddr	    0x0
     *m_MtimecmpBaseAddr	0x4000
     *m_MtimeBaseAddr	    0xbff8
     
     * 0000 msip hart 0
     * 0004 msip hart 1
     * 4000 mtimecmp hart 0 lo
     * 4004 mtimecmp hart 0 hi
     * 4008 mtimecmp hart 1 lo
     * 400c mtimecmp hart 1 hi
     * bff8 mtime lo
     * bffc mtime hi
     */

    Addr_t m_MsipBaseAddr;
    Addr_t m_MtimeBaseAddr;
    Addr_t m_MtimecmpBaseAddr;
    
    uint64_t m_ThreadCnt;
    uint64_t m_MtimeTickPeriod;
    uint32_t m_MtimeTickPeriodType;//0, pipeline tick; 1, cpu minstret
    std::vector<uint64_t> m_LastMinstret;
    uint64_t m_Interleave;
public:
    BaseCLINT(Simulator* sim);

    ~BaseCLINT();

    void Tick();
    void Init(const YAML::Node& Config);
    bool checkRange(uint64_t address);
    void read(uint64_t address,  char* data, uint16_t len);
    void write(uint64_t address, const char* data, const uint16_t len, const uint64_t mask);
    void reset();
    void setThreads(std::vector<Processor*>& threads);
};

} // namespace Emulator



#endif	

