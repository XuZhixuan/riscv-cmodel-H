#include "Rcu.hh"
#include "../Processor.hh"
#include "../../RISCV/encoding.out.h"

namespace Emulator
{
    
Rcu::Rcu(
    Processor*          processor,
    const bool          RenameRegister,
    const uint64_t      RobEntryCount,
    const uint64_t      IntRegCount,
    const bool          SupportRollBack,
    const uint64_t      SnapShotNum,
    const uint64_t      AllocWidth,
    const uint64_t      DeallocWidth
):  TraceObject("Rcu"),
    m_Processor(processor),
    m_RenameRegister(RenameRegister),
    m_Rob("Rob",RobEntryCount),
    m_IntRegfile("IntRegfile",IntRegCount),
    m_IntRenameTable("IntRenameTable"),
    m_IntBusylist("IntBusylist",IntRegCount),
    m_IntFreelist("m_IntFreelist",IntRegCount),
    m_Snapshot("Snapshot",SnapShotNum),
    m_SupportRollback(SupportRollBack),
    m_AllocWidth(AllocWidth),
    m_DeallocWidth(DeallocWidth)
{}

Rcu::~Rcu(){}

void
Rcu::Reset(){
    /* Reset Rob */
    this->m_Rob.Reset();
    this->m_RobState = rob_state_t::Rob_Idle;
    this->m_RollBackTag = 0;
    /* Reset Regfile */
    this->m_IntRegfile.Reset();
    /* Reset RenameTable */
    this->m_IntRenameTable.Reset(this->m_RenameRegister);
    /* Reset Busylist */
    this->m_IntBusylist.Reset(this->m_RenameRegister);
    /* Reset FreeList */
    this->m_IntFreelist.Reset();
}

void 
Rcu::Rename(InsnPkg_t& insnPkg){
    for(auto& insn : insnPkg){
        insn->PhyRs1 = this->m_RenameRegister ? this->m_IntRenameTable[insn->IsaRs1] : insn->IsaRs1;
        insn->PhyRs2 = this->m_RenameRegister ? this->m_IntRenameTable[insn->IsaRs2] : insn->IsaRs2;
        insn->LPhyRd = this->m_RenameRegister ? this->m_IntRenameTable[insn->IsaRd]  : insn->IsaRd;
    }
}

void 
Rcu::CreateRobEntry(InsnPtr_t& insn){
    Rob_entry_t newEntry;

    insn->RobTag = this->m_Rob.Allocate();

    if(insn){
        bool isNop   = (insn->Fu == funcType_t::ALU) && (insn->IsaRd == 0);
        bool isFence = (insn->Fu == funcType_t::CSR) && (insn->SubOp == CSR_FENCE);
        bool isMret  = (insn->Fu == funcType_t::CSR) && (insn->SubOp == CSR_MRET);

        newEntry.valid              = true;
        newEntry.done               = isNop | insn->Excp.valid | isFence | isMret;

        newEntry.isStable           = newEntry.done;
        newEntry.isMisPred          = false;
        newEntry.isExcp             = insn->Excp.valid;
        
        newEntry.isRvc              = insn->IsRvcInsn;
        newEntry.pc                 = insn->Pc;

        newEntry.Fu                 = insn->Fu;
        newEntry.LSQtag             = insn->LSQTag;

        newEntry.RdType             = insn->RdType;
        newEntry.isaRd              = insn->IsaRd;
        newEntry.phyRd              = insn->PhyRd;
        newEntry.LphyRd             = insn->LPhyRd;

        if(insn->Excp.valid){
            this->m_RobState        = rob_state_t::Rob_Undo;
            this->m_RollBackTag     = insn->RobTag;
            this->m_ExcpCause       = insn->Excp.Cause;
            this->m_ExcpTval        = insn->Excp.Tval;
            this->m_Processor->FlushBackWard(InsnState_t::State_Issue);
        }else{
            if(insn->ControlFlowInsn){
                this->m_RobState        = rob_state_t::Rob_Undo;
                this->m_RollBackTag     = insn->RobTag;   
                if(isMret){
                    this->m_RobState        = rob_state_t::Rob_Idle;
                }
            }
            // if(insn->Fu == funcType_t::CSR && insn->SubOp == CSR_FENCE){
                // DPRINTF(MemoryOrder,"RobTag[{}],Pc[{:#x}] -> Receive Fence, Change Memory Order",insn->RobTag,insn->Pc);
            // }
        }
    }else{
        newEntry.valid      = false;
        newEntry.isStable   = true;
        newEntry.done       = true;
        newEntry.isExcp     = false;
        newEntry.isMisPred  = false;
    }

    newEntry.insnPtr = insn; // For Trace Usage, Keep the Pointer

    this->m_Rob[insn->RobTag] = newEntry;
}

void
Rcu::Allocate(InsnPkg_t& insnPkg, uint64_t allocCount){
    this->Rename(insnPkg);
    for(size_t i = 0; i < allocCount; i++){
        InsnPtr_t insn = insnPkg[i];
        if(insn){
            if(this->m_RenameRegister && insn->RdType == RegType_t::INT && insn->IsaRd != 0){
                insn->PhyRd = this->m_IntFreelist.pop();
                this->m_IntBusylist[insn->PhyRd].allocated = true;
                this->m_IntBusylist[insn->PhyRd].done      = false;
                this->m_IntBusylist[insn->PhyRd].forwarding = false;
                this->m_IntRenameTable[insn->IsaRd]        = insn->PhyRd;
            }
        }
    }
    this->ResovleDependancy(insnPkg);
    for(size_t i = 0; i < allocCount; i++){
        InsnPtr_t insn = insnPkg[i];
        this->CreateRobEntry(insn);
    }
}

void 
Rcu::TryAllocate(InsnPkg_t& insnPkg, uint64_t& SuccessCount){
    
    SuccessCount = 0;
    uint64_t allocRobCount = 0;
    uint64_t allocLrqCount = 0;
    uint64_t allocSrqCount = 0;
    uint64_t allocRegCount = 0;
    for(auto insn : insnPkg){
        if(insn){
            if(this->m_Rob.getAvailEntryCount() > allocRobCount){
                allocRobCount++;
            }else{
                break;
            }
            if(this->m_RenameRegister && insn->RdType == RegType_t::INT && insn->IsaRd != 0){
                if((this->m_IntFreelist.getAvailEntryCount() > allocRegCount)){
                    allocRegCount++;
                }else{
                    break;
                }
            }
        }
        SuccessCount++;
    }
};


void 
Rcu::ResovleDependancy(InsnPkg_t& insnPkg){
    for(size_t i = 0 ; i < insnPkg.size(); i++){
        InsnPtr_t& insn = insnPkg[i];
        if(insn && insn->RdType == RegType_t::INT && insn->IsaRd != 0){
            for(size_t j = i + 1; j < insnPkg.size(); j++){
                InsnPtr_t& laterInsn = insnPkg[j];
                if(laterInsn){
                    if(insn->RdType == laterInsn->Rs1Type && insn->IsaRd == laterInsn->IsaRs1){
                        laterInsn->PhyRs1 = insn->PhyRd;
                    }
                    if(insn->RdType == laterInsn->Rs2Type && insn->IsaRd == laterInsn->IsaRs2){
                        laterInsn->PhyRs2 = insn->PhyRd;
                    } 
                    if(insn->RdType == laterInsn->RdType  && insn->IsaRd == laterInsn->IsaRd){
                        laterInsn->LPhyRd = insn->PhyRd;
                    } 
                }
            }
        }
    }
}


bool 
Rcu::ReadyForCommit(uint64_t RobTag){
    uint64_t RobPtr = this->m_Rob.getHeader();
    if(this->m_RobState == rob_state_t::Rob_Idle ||  this->m_Rob.isOlder(RobTag,this->m_RollBackTag)){
        for(size_t i = 0; i < this->m_DeallocWidth && i < this->m_Rob.getUsage(); i++){
            if(RobPtr == RobTag){
                return true;
            }else{
                auto RobEntry = this->m_Rob[RobPtr];
                if( !RobEntry.isStable && (RobEntry.Fu == funcType_t::LDU || 
                    RobEntry.Fu == funcType_t::STU || RobEntry.Fu == funcType_t::BRU))
                {
                    return false;
                }
                RobPtr = this->m_Rob.getNextPtr(RobPtr);
            }
        }
    }else{
        if(this->m_RobState == rob_state_t::Rob_WaitForResume && this->m_RollBackTag == RobTag){
            return true;
        }
    }
    return false;
}

void 
Rcu::Forwarding(InsnPtr_t& insn){
    this->m_IntRegfile[insn->PhyRd] = insn->RdResult;
    this->m_IntBusylist[insn->PhyRd].done = true;
}

void 
Rcu::WriteBack(InsnPtr_t& insn, bool& needRedirect){
    needRedirect = false;
    if(!this->m_Rob.empty() && (this->m_Rob.isOlder(insn->RobTag,this->m_Rob.getLastest()) || insn->RobTag == this->m_Rob.getLastest())){
        this->m_Rob[insn->RobTag].done = true;
        this->m_Rob[insn->RobTag].isStable = true;
        if(!insn->Excp.valid){
            if(insn->Fu == funcType_t::BRU){
                if(insn->BruMisPred){
                    this->m_Rob[insn->RobTag].isMisPred = true;  
                    if(this->m_RobState == rob_state_t::Rob_Idle || 
                        (this->m_Rob.isOlder(insn->RobTag,this->m_RollBackTag) ||
                        insn->RobTag == this->m_RollBackTag)
                    ){
                        this->m_RobState = rob_state_t::Rob_Undo;
                        this->m_RollBackTag = insn->RobTag;
                        this->m_Processor->FlushBackWard(InsnState_t::State_Issue);
                        needRedirect = true;
                        DPRINTF(Redirect,"RobTag[{}],Pc[{:#x}] -> MisPredict, Redirect to [{:#x}]",
                            insn->RobTag,
                            insn->Pc,
                            insn->BruTarget
                        );
                    }
                }else{
                    if(!this->m_Processor->m_doSpeculation && this->m_RobState == rob_state_t::Rob_WaitForResume && 
                        insn->RobTag == this->m_RollBackTag)
                    {
                        this->m_RobState = rob_state_t::Rob_Idle;
                        DPRINTF(WriteBack,"RobTag[{}],Pc[{:#x}] -> Resovled Branch, Resume Executing]",
                            insn->RobTag,
                            insn->Pc
                        ); 
                    }else{
                        DPRINTF(WriteBack,"RobTag[{}],Pc[{:#x}] -> Resovled Branch, Predirect Successfully]",
                            insn->RobTag,
                            insn->Pc
                        ); 
                    }
                }
            }
            if(insn->RdType == RegType_t::INT && insn->IsaRd != 0){
                this->m_IntRegfile[insn->PhyRd] = insn->RdResult;
                this->m_IntBusylist[insn->PhyRd].done = true;
                this->m_IntBusylist[insn->PhyRd].forwarding = false;
                DPRINTF(WriteBack,"RobTag[{}],Pc[{:#x}] -> Write Back Result[Rd[{}],PRd[{}],Result[{:#x}]",
                    insn->RobTag,
                    insn->Pc,
                    insn->IsaRd,
                    insn->PhyRd,
                    insn->RdResult
                );
            }
        }else{
            this->m_Rob[insn->RobTag].isExcp = true;
            if(this->m_RobState == rob_state_t::Rob_Idle || 
                this->m_Rob.isOlder(insn->RobTag,this->m_RollBackTag)
            )
            {
                this->m_RobState = rob_state_t::Rob_Undo;
                this->m_RollBackTag = insn->RobTag;
                this->m_ExcpCause   = insn->Excp.Cause;
                this->m_ExcpTval    = insn->Excp.Tval;
            }
        }
    }
}

void 
Rcu::AGUFastDetect(InsnPtr_t& insn){
    auto lsq = this->m_Processor->getLsqPtr();
    this->m_Rob[insn->RobTag].isStable = true;
    DPRINTF(WriteBack,"RobTag[{}],Pc[{:#x}] -> Scan AGU result, Exception [{}]",
                    insn->RobTag,
                    insn->Pc,
                    insn->Excp.valid
    );
    if(insn->Fu == funcType_t::STU && insn->Agu_addr_ready && insn->Agu_data_ready)
    {
        this->m_Rob[insn->RobTag].done = true;
    }
    if(insn->Excp.valid){
        this->m_Rob[insn->RobTag].done   = true;
        this->m_Rob[insn->RobTag].isExcp = true;
        if(this->m_RobState != rob_state_t::Rob_Idle || 
            this->m_Rob.isOlder(insn->RobTag,this->m_RollBackTag)
        )
        {
            this->m_RobState = rob_state_t::Rob_Undo;
            this->m_RollBackTag = insn->RobTag;
            this->m_ExcpCause   = insn->Excp.Cause;
            this->m_ExcpTval    = insn->Excp.Tval;
        }
    }
}




void 
Rcu::ReleaseResource(uint16_t robTag){
    auto& entry = this->m_Rob[robTag];
    if(entry.RdType == RegType_t::INT && entry.phyRd != 0){
        this->m_IntFreelist.push(entry.phyRd);
        this->m_IntBusylist[entry.phyRd].forwarding = false;
        this->m_IntBusylist[entry.phyRd].done       = false;
        if(this->m_RenameRegister){
            this->m_IntBusylist[entry.phyRd].allocated  = false;
            this->m_IntRenameTable[entry.isaRd] = entry.LphyRd;
            DPRINTF(RollBack,"RobTag[{}],Pc[{:#x}], Free phyRegister : Rd[{}], PRd[{}], LPRd[{}]",
                robTag,
                entry.pc,
                entry.isaRd,
                entry.phyRd,
                entry.LphyRd
            );
        }
    }
    if(entry.Fu == funcType_t::LDU){
        this->m_Processor->getLsqPtr()->KillLoadEntry(entry.LSQtag);
    }else if(entry.Fu == funcType_t::STU){
        this->m_Processor->getLsqPtr()->KillStoreEntry(entry.LSQtag);
    }
}

void 
Rcu::RollBack(){
    uint16_t RobPtr = this->m_Rob.getLastest();
    for(size_t i = 0 ; i < this->m_AllocWidth && 
        this->m_Rob.isOlder(this->m_RollBackTag,RobPtr) || 
        RobPtr == this->m_RollBackTag;i++)
    {
        Rob_entry_t entry = this->m_Rob[RobPtr];
        if(entry.valid){
            if(RobPtr == this->m_RollBackTag){
                this->m_RobState = rob_state_t::Rob_WaitForResume;
                if(entry.isExcp){
                    this->ReleaseResource(RobPtr);
                }
                DPRINTF(RollBack,"RobTag[{}],Pc[{:#x}] -> RollBack Finish, Wait For Resume",RobPtr,entry.pc);
                break;
            }
            entry.valid = false;
            this->ReleaseResource(RobPtr);
            DPRINTF(RollBack,"RobTag[{}],Pc[{:#x}]",RobPtr,entry.pc);
        }
        this->m_Rob.RollBack();
        RobPtr = this->m_Rob.getLastest();
    }
}

void 
Rcu::Evaulate(){
    if(this->m_RobState == rob_state_t::Rob_Undo){
        this->RollBack();
    }
    if(this->m_RobState == rob_state_t::Rob_FlushBackend){
        this->m_RobState = rob_state_t::Rob_Idle;
    }
}


void 
Rcu::CommitInsn(InsnPkg_t& insnPkg, Redirect_t& redirectReq, bool& needRedirect){
    needRedirect = false;
    for(size_t i = 0; i < insnPkg.size(); i++){
        uint16_t robPtr = this->m_Rob.getHeader();
        auto& robEntry = this->m_Rob[robPtr];
        if(robEntry.valid){
            DASSERT(robEntry.done,"Commit Insn When not Ready!")
            if(!robEntry.isExcp){
                if(robEntry.RdType == RegType_t::INT && robEntry.LphyRd != 0){
                    this->m_Processor->m_ExecContext->WriteIntReg(robEntry.isaRd,this->m_IntRegfile[robEntry.phyRd]);
                    if(this->m_RenameRegister){
                        this->m_IntFreelist.push(robEntry.LphyRd);
                        this->m_IntBusylist[robEntry.LphyRd].done = false;
                        this->m_IntBusylist[robEntry.LphyRd].allocated = false;
                        DPRINTF(Commit,"RobTag[{}],Pc[{:#x}] -> Deallocate Last PRd[{}]",robPtr,robEntry.pc,robEntry.LphyRd);
                    }
                }
                if(robEntry.Fu == funcType_t::LDU){
                    DPRINTF(Commit,"RobTag[{}],Pc[{:#x}] -> Commit a Load LSQTag[{}]", 
                    robPtr,robEntry.pc,robEntry.LSQtag
                    );
                    this->m_Processor->getLsqPtr()->CommitLoadEntry(robEntry.LSQtag);

                }else if(robEntry.Fu == funcType_t::STU){
                    DPRINTF(Commit,"RobTag[{}],Pc[{:#x}] -> Commit a Store LSQTag[{}]: Address[{:#x}] Data[{:#x}]",
                        robPtr,robEntry.pc,robEntry.LSQtag,robEntry.insnPtr->Agu_addr,robEntry.insnPtr->Agu_data);
                    this->m_Processor->getLsqPtr()->CommitStoreEntry(robEntry.LSQtag);
                }
            }
            if(this->m_RobState == rob_state_t::Rob_WaitForResume && robPtr == this->m_RollBackTag){
                this->m_RobState = rob_state_t::Rob_FlushBackend;
                this->m_Processor->FlushForward(InsnState_t::State_Dispatch);
                if(robEntry.isExcp){
                    needRedirect = true;
                    redirectReq.StageId = InsnState_t::State_Commit;
                    this->m_Processor->m_ExecContext->WriteCsr(CSR_MEPC,robEntry.pc);
                    this->m_Processor->m_ExecContext->WriteCsr(CSR_MCAUSE,this->m_ExcpCause);
                    this->m_Processor->m_ExecContext->WriteCsr(CSR_MTVAL,this->m_ExcpTval);
                    this->m_Processor->m_ExecContext->ReadCsr(CSR_MTVEC,redirectReq.target);
                    this->m_Processor->FlushBackWard(InsnState_t::State_Issue);
                    DPRINTF(Commit,"RobTag[{}],Pc[{:#x}] -> Commit an Exception, Redirect to {:#x}!",robPtr,robEntry.pc,redirectReq.target);
                }
                DPRINTF(Commit,"RobTag[{}],Pc[{:#x}] -> Resume Execution!",robPtr,robEntry.pc);
            }
            this->m_Processor->m_ExecContext->InstretInc();
        }
        this->m_Rob.Pop();                        
    }

}



} // namespace Emulator
