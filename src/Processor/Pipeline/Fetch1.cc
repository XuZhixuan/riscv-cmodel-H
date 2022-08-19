#include "Fetch1.hh"
#include "../Processor.hh"
#include "../../Simulator.hh"
namespace Emulator
{

Fetch1::Fetch1(
    Processor*                       processor              ,
    const uint64_t                   iCacheLatency          ,
    const uint64_t                   iCacheAlignByte        ,
    const uint64_t                   FetchByteWidth         ,
    const uint64_t                   InflightQueueSize 
) : BaseStage(processor,"Fetch1"),
    m_PcRegister("PcRegister",1),
    m_InflightQueue("FetchInflightQueue",InflightQueueSize),
    m_iCacheAlignByte(iCacheAlignByte),
    m_iCachePort(iCacheLatency,this->m_Processor->getDramPtr()),
    m_FetchByteWidth(FetchByteWidth),
    m_PredSync(FetchByteWidth/2)
{}

Fetch1::~Fetch1(){}

void 
Fetch1::Evaluate(){
    this->m_iCachePort.Evaluate();
    this->SendReq();
    
    bool SendSuccess;
    if(this->m_PcRegister.OutPort->valid){
        this->SendFetchReq(SendSuccess);
        this->Predict(this->m_PcRegister.InPort->data,this->m_PredSync);
    }
    this->GenNextFetchAddress(SendSuccess);
}

void 
Fetch1::Advance(){
    this->m_PcRegister.advance();
    this->m_iCachePort.Advance();
    this->m_FlushSyncLatch.advance();
    if(this->m_FlushSyncLatch.OutPort->valid){
        this->FlushAction();
        DPRINTF(Flush,"Flush Stage");
    }
}

void 
Fetch1::Reset(){
    this->m_MisalignValid    = false;
    this->m_MisalignHalf     = 0;
    this->m_ExcpTag          = 0;
    this->m_State            = State_t::Idle;
    this->m_iCachePort.Reset();
    this->m_InflightQueue.Reset();
    this->m_FlushSyncLatch.reset();
    this->m_PcRegister.reset();
}


void 
Fetch1::InitBootPc(Addr_t boot_address){
    this->m_PcRegister.InPort->set(boot_address);
}

void
Fetch1::GenNextFetchAddress(bool& SendSuccess){
    InsnState_t redirectTag = InsnState_t::State_Fetch1;
    if(this->m_PcRegister.OutPort->valid){
        this->m_PcRegister.InPort->set(this->m_PcRegister.OutPort->data);
    }
    if(SendSuccess){
        this->m_PcRegister.InPort->set((this->m_PcRegister.OutPort->data & ~(this->m_iCacheAlignByte - 1)) + this->m_FetchByteWidth);
    }
    for(auto RedirectPort : this->m_RedirectPortVec){
        if(RedirectPort->valid && RedirectPort->data.StageId >= redirectTag){
            this->m_PcRegister.InPort->set(RedirectPort->data.target);
            redirectTag = RedirectPort->data.StageId;
        }
    }
}

void 
Fetch1::Predict(Addr_t& Pc,std::vector<Pred_t>& insnPred){
    for(auto& pred : insnPred){
        pred.taken_valid  = false;
        pred.taken        = false;
        pred.target       = 0;
        pred.target_valid = false;
    }
}

void
Fetch1::SendFetchReq(bool& SendSuccess){
    MemReq_t            fetchReq;
    InflightQueueEntry_t NewEntry(this->m_FetchByteWidth);
    SendSuccess = false;
    /* Check Alignment */
    if(!this->m_InflightQueue.full() && this->m_State == State_t::Idle && 
        !this->m_StageOutPort->isStalled() )
    {
        NewEntry.Address    = this->m_PcRegister.OutPort->data;
        NewEntry.Killed     = false;
        NewEntry.InsnPred   = this->m_PredSync;
        fetchReq.Id.HartId  = this->m_Processor->getThreadId();
        fetchReq.Id.TransId = this->m_InflightQueue.Allocate();
        if((this->m_PcRegister.OutPort->data & 0b1) == 0 ){
            NewEntry.Busy       = true;
            fetchReq.Address    = this->m_PcRegister.OutPort->data & ~(m_FetchByteWidth - 1);
            fetchReq.Length     = this->m_FetchByteWidth;
            fetchReq.Opcode     = MemOp_t::Fetch;
            SendSuccess = true;
            this->m_InflightQueue[fetchReq.Id.TransId] = NewEntry;
            this->m_iCachePort.ReceiveFetchReq(fetchReq,std::bind(&Fetch1::ReceiveReq,this,std::placeholders::_1));
        }else{
            NewEntry.Excp.valid = true;
            NewEntry.Excp.Cause = RISCV::INSTR_ADDR_MISALIGNED;
            NewEntry.Excp.Tval  = this->m_PcRegister.OutPort->data;
            NewEntry.Busy       = false;
            this->m_InflightQueue[fetchReq.Id.TransId] = NewEntry;
            this->m_State       = State_t::HandleExcp;
            this->m_ExcpTag     = fetchReq.Id.TransId;
        }
    }
}

void 
Fetch1::FlushAction(){
    this->KillNewer(this->m_InflightQueue.getHeader());
    // this->m_iCachePort.Reset();
    // this->m_InflightQueue.Reset();
    this->m_PcRegister.OutPort->kill();
    this->m_MisalignValid    = false;
    this->m_MisalignHalf     = 0;
    this->m_ExcpTag          = 0;
    this->m_State            = State_t::Idle;
}

void 
Fetch1::KillNewer(uint64_t InflightQueueTag){
    for(auto ptr = InflightQueueTag; ptr != this->m_InflightQueue.getTail();ptr = this->m_InflightQueue.getNextPtr(ptr)){
        InflightQueueEntry_t& entry = this->m_InflightQueue[ptr];
        entry.Killed = true;
    }
}

void 
Fetch1::ReceiveReq(MemResp_t mem_resp){
    
    InflightQueueEntry_t& entry = this->m_InflightQueue[mem_resp.Id.TransId];
    entry.Busy = false;
    entry.Excp = mem_resp.Excp;
    if(!entry.Excp.valid){
        memcpy(entry.InsnByte.data(),mem_resp.Data,this->m_FetchByteWidth);
    }else{
        if(this->m_State == State_t::Idle || (this->m_State == State_t::HandleExcp && 
           this->m_InflightQueue.isOlder(mem_resp.Id.TransId,this->m_ExcpTag)))
        {
            this->m_ExcpTag = mem_resp.Id.TransId;
            this->KillNewer(this->m_ExcpTag);
        }
    }
#ifdef TRACE_ON
    std::stringstream insnByte;
    for(size_t i = 0; !mem_resp.Excp.valid && i < this->m_FetchByteWidth; i++){
        insnByte << fmt::format("{:02x} ",*(uint8_t*)(mem_resp.Data+(m_FetchByteWidth-1-i)));
    }
    DPRINTF(ICacheResp,"Address : {:#x}, Killed : {}, Excp : {} \n\tFetch Package {}",
                mem_resp.Address,entry.Killed,mem_resp.Excp.valid,insnByte.str());
#endif

}

void 
Fetch1::Predecode(InflightQueueEntry_t& frontEntry ,InsnPkg_t& insnPkg){
    bool needRedirect;

    DASSERT(!frontEntry.Killed,"Killed But send to Next Stage!");
    if(frontEntry.Excp.valid){
        auto insn = this->m_Processor->CreateInsn();
        insn->Pc   = frontEntry.Address;
        insn->Excp = frontEntry.Excp;
        this->m_State = State_t::WaitForResume;
        insnPkg.emplace_back(insn);
    }else{
        uint64_t offset  = frontEntry.Address & (this->m_iCacheAlignByte-1);
        DASSERT(!(offset && this->m_MisalignValid),"Shift Cacheline when MisAlign valid!");
        
        char* dataPtr = frontEntry.InsnByte.data() + offset;
        uint64_t numByte = m_FetchByteWidth - 
                (frontEntry.Address & (this->m_iCacheAlignByte-1)) + 
                (this->m_MisalignValid ? 2 : 0);

        uint64_t Npc  = frontEntry.Address - (this->m_MisalignValid ? 2 : 0);

        while(numByte){
            if(numByte == 2 && ((*dataPtr & 0b11) == 0b11)){
                this->m_MisalignValid = true;
                this->m_MisalignHalf  = *(uint16_t*)dataPtr;
                break;
            }
            InsnPtr_t insn          = this->m_Processor->CreateInsn();
            insn->Pc                = Npc;
            insn->Excp              = frontEntry.Excp;
            insn->IsRvcInsn         = this->m_MisalignValid ? false : ((*dataPtr & 0b11) != 0b11);
            insn->CompressedInsn    = this->m_MisalignValid ? ((*(uint16_t*)dataPtr << 16) + this->m_MisalignHalf) : 
                                      (insn->IsRvcInsn       ? (*(uint16_t*)dataPtr) : (*(uint32_t*)dataPtr));
            insnPkg.emplace_back(insn);
            this->BranchRedirect(insn,frontEntry.InsnPred[(offset >> 1)],needRedirect);
            if(needRedirect){
                break;
            }
            numByte -= (insn->IsRvcInsn ? 2 : 4);
            dataPtr += this->m_MisalignValid ? 2 : (insn->IsRvcInsn ? 2 : 4);
            Npc     += (insn->IsRvcInsn ? 2 : 4);
            offset  += (insn->IsRvcInsn ? 2 : 4);
            this->m_MisalignValid = false;
        }   
    }     
}

void
Fetch1::BranchRedirect(InsnPtr_t& insn, Pred_t& Pred, bool& needRedirect){
    needRedirect = false;
    Redirect_t RedirectReq;
    RedirectReq.StageId = InsnState_t::State_Fetch1;
    RISCV::StaticInsn instruction(insn->CompressedInsn);
    insn->Pred.Taken        = false;
    insn->Pred.Target       = insn->Pc + (insn->IsRvcInsn ? 2 : 4);
    if(instruction.opcode() == 0b1101111){ // JAL
        insn->Pred.Taken    = true;
        insn->Pred.Target   = insn->Pc + instruction.ujimm();
        if(!(Pred.taken_valid == true && Pred.taken == true && Pred.target_valid && Pred.target == (insn->Pc + instruction.ujimm()))){
            needRedirect = true;
            RedirectReq.target = insn->Pred.Target;
        }
    }else if(instruction.opcode() == 0b1100111 && instruction.func3() == 0b000){ // JALR
        if(Pred.taken_valid && Pred.target_valid){
            insn->Pred.Taken  = Pred.taken;
            insn->Pred.Target = Pred.target;
        }
    }else if(instruction.opcode() == 0b1100011){ // BRANCH
        if(Pred.taken_valid && Pred.taken){
            insn->Pred.Taken = true;
            insn->Pred.Target = insn->Pc + instruction.sbimm();
            if(!(Pred.target_valid && Pred.target == (insn->Pc + instruction.sbimm()))){
                needRedirect = true;
                RedirectReq.target = insn->Pred.Target;
            }
        }
    }else if(instruction(1,0) == 0b01){
        if(instruction(15,13) == 0b101){ // C.J
            insn->Pred.Taken    = true;
            insn->Pred.Target   = insn->Pc + instruction.rvc_j_imm();
            if(!(Pred.taken_valid == true && Pred.taken == true && Pred.target_valid && Pred.target == (insn->Pc + instruction.rvc_j_imm()))){
            needRedirect = true;
            RedirectReq.target = insn->Pred.Target;
        }
        }else if(instruction(15,13) == 0b110 || instruction(15,13) == 0b111){ // C.BRANCH
            if(Pred.taken_valid && Pred.taken){
            insn->Pred.Taken = true;
            insn->Pred.Target = insn->Pc + instruction.sbimm();
            if(!(Pred.target_valid && Pred.target == (insn->Pc + instruction.rvc_b_imm()))){
                needRedirect = true;
                RedirectReq.target = insn->Pred.Target;
            }
        }
        }
    }else{
        if(Pred.taken_valid && Pred.target_valid){
            needRedirect = true;
            RedirectReq.target = insn->Pred.Target; 
        }
    }
    if(needRedirect){
        DPRINTF(Redirect,"Pc[{:#x}] -> Predict Mismatch, Redirect to {:#x}",insn->Pc,RedirectReq.target);
        this->m_FlushSyncLatch.InPort->set(true);
        this->m_RedirectPort->set(RedirectReq);
    }
}

void
Fetch1::SendReq(){
    InflightQueueEntry_t& frontEntry = this->m_InflightQueue.front();
    if(!this->m_InflightQueue.empty() && !frontEntry.Busy){
        if(!frontEntry.Killed){
            InsnPkg_t insnPkg;
            this->Predecode(frontEntry,insnPkg);
            if(insnPkg.size()){
                this->m_StageOutPort->set(insnPkg);
            }
        }
        this->m_InflightQueue.Pop();
    }
}

void 
Fetch1::AddRedirectPort(std::shared_ptr<TimeBuffer<Redirect_t>::Port> RedirectPort){
    this->m_RedirectPortVec.emplace_back(RedirectPort);
}

std::shared_ptr<BaseStage> Create_Fetch1_Instance(
        Processor*                       processor         ,
        const YAML::Node&                StageConfig       )
{
    const uint64_t iCacheLatency        = processor->m_iCacheLatency;
    const uint64_t iCacheAlignByte      = processor->m_iCacheAlignByte ;
    const uint64_t FetchByteWidth       = StageConfig["FetchByteWidth"].as<uint64_t>() ;
    const uint64_t InflightQueueSize    = StageConfig["InflightQueueSize"].as<uint64_t>() ;
    return std::shared_ptr<BaseStage>(
        new Fetch1(processor,iCacheLatency,iCacheAlignByte,FetchByteWidth,InflightQueueSize)
    );

}

} // namespace Emulator
