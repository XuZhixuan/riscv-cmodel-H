#ifndef __FETCH1_HH__
#define __FETCH1_HH__ 

#include "BaseStage.hh"
#include "Pipe_data.hh"

#include "../Component/iCachePort.hh"
#include "../Component/LoopQueue.hh"

#include "../../Common/Common.hh"
#include "../../Memory/BaseDRAM.hh"

namespace Emulator
{
class Simulator;
class Processor;
class Fetch1 : public BaseStage
{
private:
    enum   State_t{
        Idle, HandleExcp, WaitForResume
    };
    struct Pred_t
    {
        bool    taken_valid;
        bool    taken;
        bool    target_valid;
        Addr_t  target;
    };
    struct InflightQueueEntry_t
    {
        bool                        Busy;
        bool                        Killed;
        Addr_t                      Address;
        std::vector<char>           InsnByte;
        std::vector<Pred_t>         InsnPred;
        Exception_t                 Excp;
        InflightQueueEntry_t(){};
        InflightQueueEntry_t(uint64_t ByteWidth){
            this->InsnByte.resize(ByteWidth);
            this->InsnPred.resize(ByteWidth/2);
        };
    };
private:
    /* Param */
    const uint16_t                      m_FetchByteWidth;

    const uint16_t                      m_iCacheAlignByte;

    bool                                m_MisalignValid;

    uint16_t                            m_MisalignHalf;

    /* Var */
    State_t                             m_State;

    uint64_t                            m_ExcpTag;

    TimeBuffer<Addr_t>                  m_PcRegister;

    std::vector<Pred_t>                 m_PredSync;

    /* Ptr in this Module */
    iCachePort                          m_iCachePort;

    LoopQueue<InflightQueueEntry_t>      m_InflightQueue;

    /* Ptr Out of this Module */
    std::vector<std::shared_ptr<TimeBuffer<Redirect_t>::Port>> m_RedirectPortVec;

public:
    Fetch1();

    Fetch1( 
        Processor*                       processor              ,
        const uint64_t                   iCacheLatency          ,
        const uint64_t                   iCacheAlignByte        ,
        const uint64_t                   FetchByteWidth         ,
        const uint64_t                   InflightQueueSize 
    );

    ~Fetch1();

    void Evaluate();

    void Advance();

    void InitBootPc(Addr_t boot_address);

    void Reset();

    void FlushAction();

    void KillNewer(uint64_t InflightQueueTag);

    void GenNextFetchAddress(bool& SendSuccess);

    void SendFetchReq(bool& SendSuccess);

    void ReceiveReq(MemResp_t mem_resp);

    void Predecode(InflightQueueEntry_t& frontEntry ,InsnPkg_t& insnPkg);

    void BranchRedirect(InsnPtr_t& insn, Pred_t& Pred, bool& needRedirect);

    void Predict(Addr_t& Pc, std::vector<Pred_t>& insnPred);

    void SendReq();

    void AddRedirectPort(std::shared_ptr<TimeBuffer<Redirect_t>::Port> RedirectPort);
};

std::shared_ptr<BaseStage> Create_Fetch1_Instance(
        Processor*                       processor         ,
        const YAML::Node&                StageConfig    
);  
} // namespace Emulator





#endif	