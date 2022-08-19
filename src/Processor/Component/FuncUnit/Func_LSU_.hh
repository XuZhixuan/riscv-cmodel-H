#ifndef __FUNC_LSU_HH__
#define __FUNC_LSU_HH__

#include <vector>

#include "BaseFuncUnit.hh"

#include "../mmu.hh"

#include "../Lsq.hh"

#include "../Rcu.hh"

#include "../dCachePort.hh"

namespace Emulator
{



class Func_LSU : public BaseFuncUnit
{
private:
	
	std::shared_ptr<RISCV::ExecContext> m_ExecContext;

    std::shared_ptr<Lsq>    m_Lsq;

    std::shared_ptr<Rcu>    m_Rcu;
	
    std::shared_ptr<BaseMMU>    m_Mmu;

public:

    dCachePort              m_dCachePort;

    static std::vector<Func_LSU*> lsu_vec;
	
    Func_LSU(		
		std::shared_ptr<RISCV::ExecContext> execContext,
        std::shared_ptr<BaseMMU> mmu,
        std::shared_ptr<Rcu> rcu,
        std::shared_ptr<Lsq> lsq,
        BaseDRAM*            dram,
        const uint64_t       dCacheLatency,
        const uint16_t FuncUnitId,
        const std::string Name, 
        const uint64_t Latency, 
        const bool Pipelined,
        BaseCLINT*           clint)
    :   BaseFuncUnit(Name,FuncUnitId,{funcType_t::LDU,funcType_t::STU},Latency,Pipelined),
        m_ExecContext(execContext),m_Mmu(mmu),m_Rcu(rcu),m_Lsq(lsq),m_dCachePort(dCacheLatency,dram, clint)
    {
        lsu_vec.push_back(this);
    };
 
    ~Func_LSU(){};

    void Reset(){
        this->m_Allcated = false;
        this->m_CalcuPipe.reset();
        this->m_dCachePort.Reset();
    };

    void AddrGen(InsnPtr_t& insn){
        insn->Agu_addr  =  insn->Operand1 + insn->imm;
        insn->Agu_addr_ready = true;
        if(insn->Fu == funcType_t::LDU){

            switch (insn->SubOp)
            {
            case LDU_LH  :
                if((insn->Agu_addr & 0b1) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;
            case LDU_LW  :
                if((insn->Agu_addr & 0b11) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;
            case LDU_LD  :
                if((insn->Agu_addr & 0b111) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;
            case LDU_LHU :
                if((insn->Agu_addr & 0b1) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
				break;
            case LDU_LWU :
                if((insn->Agu_addr & 0b11) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;
			case FLDU_LW  :
                if((insn->Agu_addr & 0b11) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;				
			case FLDU_LD  :
				if((insn->Agu_addr & 0b111) != 0){
					insn->Excp.valid = true;
					insn->Excp.Cause = RISCV::ExcpCause_t::LD_ADDR_MISALIGNED;
					insn->Excp.Tval  = insn->Agu_addr;
				}
				break;
            default:
                break;
            }
        }else if (insn->Fu == funcType_t::STU){
            switch (insn->SubOp)
            {
            case STU_SH:
                if((insn->Agu_addr & 0b1) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::ST_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;
            case STU_SW:
                if((insn->Agu_addr & 0b11) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::ST_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;
            case STU_SD:
                if((insn->Agu_addr & 0b111) != 0){
                    insn->Excp.valid = true;
                    insn->Excp.Cause = RISCV::ExcpCause_t::ST_ADDR_MISALIGNED;
                    insn->Excp.Tval  = insn->Agu_addr;
                }
                break;			
			case FSTU_SW:
				if((insn->Agu_addr & 0b11) != 0){
					insn->Excp.valid = true;
					insn->Excp.Cause = RISCV::ExcpCause_t::ST_ADDR_MISALIGNED;
					insn->Excp.Tval  = insn->Agu_addr;
				}
				break;				
			case FSTU_SD:
				if((insn->Agu_addr & 0b111) != 0){
					insn->Excp.valid = true;
					insn->Excp.Cause = RISCV::ExcpCause_t::ST_ADDR_MISALIGNED;
					insn->Excp.Tval  = insn->Agu_addr;
				}
				break;
            default:
                break;
            }
        }
    };
    void DataGen(InsnPtr_t& insn){
        insn->Agu_data_ready = true;
        insn->Agu_data = insn->Operand2;
    }

    void Evaluate(){    
		bool Success;
		this->SendStoreReq(Success);
		if(!Success){
			this->SendLoadReq(Success);
		}
        if(this->m_CalcuPipe.OutPort->valid){
            this->m_Allcated = false;
            auto& insn = this->m_CalcuPipe.OutPort->data;
            if(this->m_Rcu->m_Rob[insn->RobTag].valid){
                this->AddrGen(insn);
                if(insn->Fu == funcType_t::STU){
                    this->DataGen(insn);
                }
    			/* If translate exception detected, No need to write back */
    			RISCV::AccessMode mode;
    			if (insn->Fu == funcType_t::STU){
    				mode = RISCV::AccessMode::Write;
    			}
    			else if (insn->Fu == funcType_t::LDU){
    				mode = RISCV::AccessMode::Read;
    			}
    			else{
    				/* mode should not other than Read/Write */
    				assert(false);
    			}
    			RISCV::Fault fault;
    			fault.FaultFlag = false;
    			uint64_t PhysicalAddr = this->m_Mmu->Evaluate(insn->Agu_addr, mode, insn->Pc, fault);				
    			insn->Excp.valid = fault.FaultFlag;
    			insn->Excp.Cause = fault.FaultCause;
    			insn->Excp.Tval = PhysicalAddr;
    			
    			insn->Agu_addr = PhysicalAddr;
                this->m_Rcu->AGUFastDetect(insn);
    			if (!insn->Excp.valid){
                	this->m_Lsq->WriteBack(insn);		
					if ((insn->Fu == funcType_t::LDU) && (insn->Agu_addr_ready == true)){						
						MemDependency Dep =  this->m_Lsq->loadToStoreDepCheck(insn); // Pending Completion: Must assume load address is ready					
						if (!Dep.Dependent){
							this->m_Lsq->m_LoadQueue[insn->LSQTag].hasDependancy = false;							
							this->m_Lsq->m_LoadQueue[insn->LSQTag].oldestStqTag = -1;
							if(!Success && (this->m_Lsq->m_LoadQueue[insn->LSQTag].state == loadState_t::load_WaitSend && !this->m_Lsq->m_LoadQueue[insn->LSQTag].killed)){
								MemReq_t LoadReq;
								this->m_Lsq->m_LoadQueue[insn->LSQTag].state = loadState_t::load_Inflight;
								LoadReq.Opcode	= MemOp_t::Load;
								LoadReq.Id.HartId = this->m_Lsq->m_Processor->getThreadId();;
								LoadReq.Id.TransId = insn->LSQTag;
								LoadReq.Address = insn->Agu_addr & ~(this->m_Lsq->m_dCacheAlignByte - 1);
								LoadReq.Length	= this->m_Lsq->m_Processor->m_XLEN / 8;
								LoadReq.insn	= insn;
								this->m_dCachePort.ReceiveMemReq(LoadReq,std::bind(&Func_LSU::ReceiveResponse,this,std::placeholders::_1));
							}
						}
						else{						
							this->m_Lsq->m_LoadQueue[insn->LSQTag].hasDependancy = true;
							this->m_Lsq->m_LoadQueue[insn->LSQTag].state = loadState_t::load_WaitForWakeUp;
							this->m_Lsq->m_LoadQueue[insn->LSQTag].oldestStqTag = Dep.DependentInsnID;
						}
					}
    			}
            }
        }
        this->m_dCachePort.Evaluate();
    };

    void SendStoreReq(bool& Success){
        MemReq_t storeReq;
        uint16_t stqPtr;
        storeReq.Data = new char[8];
        Success = false;
        this->m_Lsq->TryIssueStore(storeReq,Success,stqPtr);

        if(Success){
        #ifdef USING_GEM5
            if(this->m_Rcu->m_Processor->MemReqToCache(storeReq, std::bind(&Func_LSU::ReceiveResponse,this,std::placeholders::_1))){
                this->m_Lsq->SetStoreEntry_Inflight(stqPtr);
            }
        #else
            this->m_dCachePort.ReceiveMemReq(storeReq,std::bind(&Func_LSU::ReceiveResponse,this,std::placeholders::_1));
            this->m_Lsq->SetStoreEntry_Inflight(stqPtr);
        #endif
        }
        delete[] storeReq.Data;
        
    }

    void SendLoadReq(bool& Success){
        MemReq_t loadReq;
		
        Success = false;
        this->m_Lsq->TryIssueLoad(loadReq,Success);
        if(Success){
        #ifdef USING_GEM5
            this->m_Rcu->m_Processor->MemReqToCache(loadReq, std::bind(&Func_LSU::ReceiveResponse,this,std::placeholders::_1));
        #else
            this->m_dCachePort.ReceiveMemReq(loadReq,std::bind(&Func_LSU::ReceiveResponse,this,std::placeholders::_1));
        #endif
        } 
        
    }

    void ReceiveResponse(MemResp_t memResp){
        if(memResp.Opcode == MemOp_t::Load){
            auto& ldqEntry = this->m_Lsq->m_LoadQueue[memResp.Id.TransId];
            if(!ldqEntry.killed){
                ldqEntry.state = loadState_t::load_Executed;
                auto& insn     = ldqEntry.insnPtr;
                insn->Excp = memResp.Excp;
                if(!memResp.Excp.valid){
                    uint64_t offset = ldqEntry.address & (this->m_Lsq->m_dCacheAlignByte - 1);
                    switch (ldqEntry.op)
                    {
                    case LDU_LB  :
                        insn->RdResult = *(int8_t*)(memResp.Data + offset);
                        break;
                    case LDU_LH  :
                        insn->RdResult = *(int16_t*)(memResp.Data + offset);
                        break;
                    case LDU_LW  :
                        insn->RdResult = *(int32_t*)(memResp.Data + offset);
                        break;
                    case LDU_LD  :
                        insn->RdResult = *(int64_t*)(memResp.Data + offset);
                        break;
                    case LDU_LBU :
                        insn->RdResult = *(uint8_t*)(memResp.Data + offset);
                        break;
                    case LDU_LHU :
                        insn->RdResult = *(uint16_t*)(memResp.Data + offset);
                        break;
                    case LDU_LWU :
                        insn->RdResult = *(uint32_t*)(memResp.Data + offset);
                        break;
					case FLDU_LW :
						insn->RdResult = *reinterpret_cast<int32_t*>(memResp.Data + offset);
						break;
					case FLDU_LD :
						insn->RdResult = *reinterpret_cast<int64_t*>(memResp.Data + offset);
						break;
                    default:
                        break;
                    }
                }
                delete[] memResp.Data;
                bool Success = false;                
                for(auto& wbPort : this->m_wbPortVec){
                    if(!wbPort->valid){
                        wbPort->set(insn);
                        Success = true;
                        insn->State = InsnState_t::State_WriteBack;
                        break;
                    }
                }

                DASSERT(Success,"RobTag[{}],Pc[{}] -> UnAllowed WriteBack Failed");
            }
			else{
                ldqEntry.state = loadState_t::load_Executed;
            }
        }
		else if(memResp.Opcode == MemOp_t::Store){
            auto& stqEntry = this->m_Lsq->m_StoreQueue[memResp.Id.TransId];
            if(stqEntry.insnPtr->IsAmoInsn){
                for(auto& wbPort : this->m_wbPortVec){
                    if(!wbPort->valid){
                        wbPort->set(stqEntry.insnPtr);
                        stqEntry.insnPtr->State = InsnState_t::State_WriteBack;
                        break;
                    }
                }
            }
            if(!stqEntry.insnPtr->Excp.valid){
                stqEntry.state = storeState_t::store_Executed;
            }
        }
    }

    void Advance(){
        this->m_FlushLatch.advance();
        this->m_CalcuPipe.advance();
        this->m_dCachePort.Advance();
        if((this->m_Pipelined && this->m_Allcated) || this->m_FlushLatch.OutPort->valid){
            this->m_Allcated = false;
        }
    };

};


} // namespace Emulator


#endif
