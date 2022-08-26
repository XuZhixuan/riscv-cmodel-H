#include "dCachePort.hh"
#include "../../CLINT/BaseCLINT.hh"
namespace Emulator
{

dCachePort::dCachePort(uint64_t Latency, BaseDRAM* dram, BaseCLINT* clint)
    : TraceObject("dCachePort"),
      m_dCacheRespLatch("dCache",Latency),
      m_baseDRAM(dram),
      m_baseCLINT(clint)
{

}

dCachePort::~dCachePort(){}

bool
dCachePort::ReservationValidGet(Addr_t addr){
	if(m_baseDRAM->g_reservationSet.addr==addr){
		m_baseDRAM->g_reservationSet.addr = 0;
		return true;
	}
	else{
		//assert(0);
    	return false;
	}
	assert(0);
	return false;
}

bool
dCachePort::ReservationValidSet(Addr_t addr, bool valid){
	m_baseDRAM->g_reservationSet.addr = addr;
	return true;
}

bool
dCachePort::AmoIsSc(InsnPtr_t insn){
    RISCV::StaticInsn instr(insn->UncompressedInsn);
    if(instr.func5() == 0b00011)
        return true;
    return false;
}

void
dCachePort::AmoOpCal(MemReq_t &mem_req, char* storeVal, uint64_t& op1, uint64_t& op2){
    RISCV::StatInsn instr(mem_req.insn->UncompressedInsn);
	uint64_t opResult;
    switch(instr.func3()){
    case 0b10://RV32A
    {
		uint64_t srs1 = (((int64_t)((op1 >> mem_req.offset*8) << 32)) >> 32);
		uint64_t urs1 = (((uint64_t)((op1 >> mem_req.offset*8) << 32)) >> 32);
        switch(instr.func5()){
        case 0b00001: //AMOSWAP
            opResult = op2;
            break;
        case 0b00000: //AMOADD
            opResult = (int64_t)srs1 + (int64_t)op2;
            break;
        case 0b00100: //AMOXOR
			opResult = op1 ^ op2;            
            break;
        case 0b01100: //AMOAND
			opResult = op1 & op2;            
            break;
        case 0b01000: //AMOOR
			opResult = op1 | op2;            
            break;
        case 0b10000: //AMOMIN
			opResult = ((int64_t)srs1) < (int64_t)op2 ? ((int64_t)srs1) : op2;
            break;
        case 0b10100: //AMOMAX
			opResult = ((int64_t)srs1) > (int64_t)op2 ? ((int64_t)srs1) : op2;
            break;
        case 0b11000: //AMOMINU
			opResult = urs1 < op2 ? urs1 : op2;
            break;
        case 0b11100: //AMOMAXU 
			opResult = urs1 > op2 ? urs1 : op2;
            break;
        default:
            assert(false);
            break;
        }
		mem_req.insn->RdResult = (int64_t)srs1;
        break;
    }
    case 0b11://RV64A
    {
        switch(instr.func5()){
        case 0b00001: //AMOSWAP
            opResult = op2;
            break;
        case 0b00000: //AMOADD
            opResult = (int64_t)op1 + (int64_t)op2;
            break;
        case 0b00100: //AMOXOR
            opResult = (int64_t)op1 ^ (int64_t)op2;
            break;
        case 0b01100: //AMOAND
            opResult = (int64_t)op1 & (int64_t)op2;
            break;
        case 0b01000: //AMOOR
            opResult = (int64_t)op1 | (int64_t)op2;
            break;
        case 0b10000: //AMOMIN
            opResult = (int64_t)op1 < (int64_t)op2 ?  (int64_t)op1 :  (int64_t)op2;
            break;
        case 0b10100: //AMOMAX
            opResult = (int64_t)op1 > (int64_t)op2 ?  (int64_t)op1 :  (int64_t)op2;
            break;
        case 0b11000: //AMOMINU
            opResult = (uint64_t)op1 < (uint64_t)op2 ? op1 : op2;
            break;
        case 0b11100: //AMOMAXU 
            opResult = (uint64_t)op1 > (uint64_t)op2 ? op1 : op2;
            break;
        default:
            assert(false);
            break;
        }
		mem_req.insn->RdResult = (int64_t)(op1);
        break;
    }
    default:
        assert(false);
        break;
    }
	opResult <<= (mem_req.offset*8);	
    (*((uint64_t *)storeVal)) = opResult;    
}

void 
dCachePort::ReceiveMemReq(MemReq_t mem_req,std::function<void(MemResp_t)> CallBackFunc){
    MemResp_t resp;
	resp.Type       = mem_req.Type;
    resp.Id         = mem_req.Id;
    resp.Address    = mem_req.Address;
    resp.Opcode     = mem_req.Opcode;
    resp.Length     = mem_req.Length;
	resp.Data		= new char[resp.Length];
    if(mem_req.Opcode == MemOp_t::Load){
        if(this->m_baseDRAM->checkRange(mem_req.Address)){
            resp.Excp.valid = false;
            resp.Excp.Cause = 0;
            resp.Excp.Tval  = 0;
			this->m_baseDRAM->read(mem_req.Address,resp.Data,mem_req.Length);
			#if 1
            if(mem_req.insn->IsAmoInsn){
				ReservationValidSet(mem_req.Address, true);
            }
			#endif
        }else if(this->m_baseCLINT->checkRange(mem_req.Address)){
            resp.Excp.valid = false;
            resp.Excp.Cause = 0;
            resp.Excp.Tval  = 0;
			this->m_baseCLINT->read(mem_req.Address,resp.Data,mem_req.Length);
			#if 1
            if(mem_req.insn->IsAmoInsn){
				ReservationValidSet(mem_req.Address, true);
            }
			#endif
		}else if (this->m_baseDRAM->checkIORange(mem_req.Address)){
			resp.Excp.valid = false;
			resp.Excp.Cause = 0;
			resp.Excp.Tval	= 0;
			this->m_baseDRAM->readIO(mem_req.Address,resp.Data,mem_req.Length);
			#if 1
            if(mem_req.insn->IsAmoInsn){
				ReservationValidSet(mem_req.Address, true);
            }
			#endif		
		}else if (this->m_baseDRAM->checkHoleRange(mem_req.Address)){
			resp.Excp.valid = false;
			resp.Excp.Cause = 0;
			resp.Excp.Tval	= 0;
			this->m_baseDRAM->readHole(mem_req.Address,resp.Data,mem_req.Length);
			#if 1
			if(mem_req.insn->IsAmoInsn){
				ReservationValidSet(mem_req.Address, true);
			}
			#endif
        }else{
            resp.Excp.valid = true;
            resp.Excp.Cause = RISCV::LD_ACCESS_FAULT;
            resp.Excp.Tval  = mem_req.Address;
        }
        this->m_dCacheRespLatch.InPort->set({CallBackFunc,resp});
        DPRINTFF(DCacheReq,"Load Pc[{:#x}], address {:#x} Excp {}",mem_req.insn->Pc, mem_req.Address,resp.Excp.valid);
    }
	else if(mem_req.Opcode == MemOp_t::Store){
        if(this->m_baseDRAM->checkRange(mem_req.Address)){
            resp.Excp.valid = false;
            resp.Excp.Cause = 0;
            resp.Excp.Tval  = 0;
            if(mem_req.insn->IsAmoInsn){
                if(AmoIsSc(mem_req.insn)){
					if(ReservationValidGet(mem_req.Address)){
                    	mem_req.insn->RdResult = 0;
                    	this->m_baseDRAM->write(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
					}else{
                        mem_req.insn->RdResult = 1;
						//fprintf(stderr, "SC FAILED!\n");
                    }
                }
				else {
                    uint64_t rs1_val;
					uint64_t rs2_val = mem_req.insn->Operand2;
					char* store_val = new char[mem_req.Length];
                    this->m_baseDRAM->read(mem_req.Address,(char*)&rs1_val,mem_req.Length);
                    AmoOpCal(mem_req, store_val, rs1_val, rs2_val);
					mem_req.Data = store_val;
                    this->m_baseDRAM->write(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
                    delete[] store_val;
                }
            }
			else{
            	this->m_baseDRAM->write(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
            }            
        }
		else if(this->m_baseCLINT->checkRange(mem_req.Address)){
            if(mem_req.insn->IsAmoInsn){
                if(AmoIsSc(mem_req.insn)){
                    if(ReservationValidGet(mem_req.Address)){
                    	mem_req.insn->RdResult = 0;
                    	this->m_baseCLINT->write(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
                    }else{
                        mem_req.insn->RdResult = 1;						
						//fprintf(stderr, "CLINT SC FAILED!\n");
                    }
                } else {
                    uint64_t rs1_val;
					uint64_t rs2_val = mem_req.insn->Operand2;
					char* store_val = new char[mem_req.Length];
                    this->m_baseCLINT->read(mem_req.Address,(char*)&rs1_val,mem_req.Length);
                    AmoOpCal(mem_req, store_val, rs1_val, rs2_val);					
					mem_req.Data = store_val;
                    this->m_baseCLINT->write(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
                    delete[] store_val;
                }
            }
			else{
            	this->m_baseCLINT->write(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
            }        
		}		
		else if (this->m_baseDRAM->checkIORange(mem_req.Address)){
            resp.Excp.valid = false;
            resp.Excp.Cause = 0;
            resp.Excp.Tval  = 0;
            if(mem_req.insn->IsAmoInsn){
                if(AmoIsSc(mem_req.insn)){
					if(ReservationValidGet(mem_req.Address)){
                    	mem_req.insn->RdResult = 0;
                    	this->m_baseDRAM->writeIO(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
					}else{
                        mem_req.insn->RdResult = 1;
						//fprintf(stderr, "SC FAILED!\n");						
                    }
                } else {
                    uint64_t rs1_val;
					uint64_t rs2_val = mem_req.insn->Operand2;
					char* store_val = new char[mem_req.Length];					
                    this->m_baseDRAM->readIO(mem_req.Address,(char*)&rs1_val,mem_req.Length);
                    AmoOpCal(mem_req, store_val, rs1_val, rs2_val);
					mem_req.Data = store_val;
                    this->m_baseDRAM->writeIO(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
                    delete[] store_val;
                }
            }
			else{
            	this->m_baseDRAM->writeIO(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
            }     
		}		
		else if (this->m_baseDRAM->checkHoleRange(mem_req.Address)){
            resp.Excp.valid = false;
            resp.Excp.Cause = 0;
            resp.Excp.Tval  = 0;
            if(mem_req.insn->IsAmoInsn){
                if(AmoIsSc(mem_req.insn)){
					if(ReservationValidGet(mem_req.Address)){
	                    mem_req.insn->RdResult = 0;
	                    this->m_baseDRAM->writeHole(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
					}else{
                        mem_req.insn->RdResult = 1;
						//fprintf(stderr, "SC FAILED!\n");
                    }
                } else {
                    uint64_t rs1_val;
					uint64_t rs2_val = mem_req.insn->Operand2;
					char* store_val = new char[mem_req.Length];					
                    this->m_baseDRAM->readHole(mem_req.Address,(char*)&rs1_val,mem_req.Length);
                    AmoOpCal(mem_req, store_val, rs1_val, rs2_val);
					mem_req.Data = store_val;
                    this->m_baseDRAM->writeHole(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
                    delete[] store_val;
                }
            }
			else{
            	this->m_baseDRAM->writeHole(mem_req.Address,mem_req.Data,mem_req.Length,mem_req.ByteMask);
            }     
		}
		else{
            resp.Excp.valid = true;
            resp.Excp.Cause = RISCV::ST_ACCESS_FAULT;
            resp.Excp.Tval  = mem_req.Address;
        }
        this->m_dCacheRespLatch.InPort->set({CallBackFunc,resp});
        DPRINTFF(DCacheReq,"Store Pc[{:#x}], address {:#x} Excp {}",mem_req.insn->Pc, mem_req.Address,resp.Excp.valid);
    }   
}

MemResp_t
dCachePort::ReceivePTWReq(MemReq_t mem_req, std::function<void(MemResp_t)> CallBackfunc,uint64_t& _pteBits)
{
    MemResp_t resp;
    resp.Id         = mem_req.Id;
    resp.Address    = mem_req.Address;
    resp.Opcode     = mem_req.Opcode;
    resp.Length     = mem_req.Length;
	
	resp.Data		= new char[resp.Length];
	if(this->m_baseDRAM->checkRange(mem_req.Address)){
		resp.Excp.valid = false;
		resp.Excp.Cause = 0;
		resp.Excp.Tval	= 0;
		this->m_baseDRAM->read(mem_req.Address,resp.Data,mem_req.Length);
	}else{
		/* PTW should not out of bound */
		assert(false); // temp solution
		resp.Excp.valid = true;
		resp.Excp.Cause = RISCV::ExcpCause_t::LOAD_PAGE_FAULT;
		resp.Excp.Tval	= mem_req.Address;
	}
    _pteBits = *(uint64_t*)(resp.Data);
    delete[] resp.Data;
	return resp;
}

void 
dCachePort::Reset(){
    this->m_dCacheRespLatch.reset();
}   

void 
dCachePort::Evaluate(){
    if(this->m_dCacheRespLatch.OutPort->valid){
        this->m_dCacheRespLatch.OutPort->data.first(this->m_dCacheRespLatch.OutPort->data.second);
    }
}

void 
dCachePort::Advance(){
    this->m_dCacheRespLatch.advance();
}
    
} // namespace Emulator
