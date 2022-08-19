#ifndef __DECODER_HH__
#define __DECODER_HH__

#include "../Processor/DynInsn.hh"
#include "../Common/Common.hh"
#include "Encoding.hh"
#include "StaticInsn.hh"
#include "rvcExpender.hh"
namespace RISCV
{

    void Decode_Func(Emulator::DynInsn &insn, Priv_level_t priv_level)
    {

        /*  handle compressed insn to init insn.UncompressedInsn:
            if rvc, decompress it; if not, assign directly
        */
        if (insn.IsRvcInsn)
        {
            insn.UncompressedInsn = crv_decompress_real(insn.CompressedInsn);
            if (insn.UncompressedInsn == 0)
            {
                insn.Excp.valid = true;
                insn.Excp.Cause = RISCV::ILLEGAL_INSTR;
                insn.Excp.Tval = insn.CompressedInsn;
                return;
            }
        }
        else
        {
            insn.UncompressedInsn = insn.CompressedInsn;
        }

        /*  StaticInsn is a class including only insn itself and functions,
            it is used to help parse DynInsn further (init IsaRs1, ...; Ts1Type, ...)
         */
        StaticInsn instr(insn.UncompressedInsn);

        /*  parse instr to init DynInsn insn.
         */
        insn.IsaRs1 = instr.rs1();
        insn.IsaRs2 = instr.rs2();
        insn.IsaRd = instr.rd();

        /*  Before handling RsType, OperandReady, ControlFlowInsn and so on, set their default value.
         */
        insn.Rs1Type = Emulator::RegType_t::NONE;
        insn.Rs2Type = Emulator::RegType_t::NONE;
        insn.RdType = Emulator::RegType_t::NONE;

        insn.Operand1Ready = false;
        insn.Operand2Ready = false;

        insn.ControlFlowInsn = false;

        insn.imm = 0;

        insn.Excp.valid = 0;
        insn.Excp.Cause = RISCV::ILLEGAL_INSTR;
        insn.Excp.Tval = 0;

        insn.Fu = Emulator::funcType_t::ALU;
        insn.SubOp = ALU_ADD;

        /*  assist to init insn.Excp  */
        bool illegal_instr = false;
        bool ecall = false;
        bool ebreak = false;

        /*  get opcode of instr and init insn according to it
         */
        switch (instr.opcode())
        {
        case OpcodeSystem:
            /** 1110011:    I-type,             |12-bit csr|5-bit rs1|3-bit funct3|5-bit rd|7-bit opcode
             *              R-type, |7-bit funct7|5-bit rs2|5-bit rs1|3-bit funct3|5-bit rd|7-bit opcode
             *  I-type: ecall, ebreak, CSRRW, ... (done)
             *  R-type: mret, sret; other privileged instructions
             */
            insn.Fu = Emulator::funcType_t::CSR; // CSR-related instruction

            /** 31:20 */
            insn.imm = instr.csr();

            /** rs1/uimm: 19:15 */
            if (!(instr.func3() & 0b011))
            /* instr.func3()==100:
             * TODO: Hypervisor VM Load/Store (32, 64) 
             */
            {
            }
            else if (instr.func3() >> 2)
            /** 14:12
             *  only CSRRWI, CSRRSI, CSRCI indicate 1xx for funct3, which means 19:15 is uimm rather than rs1 addr
             */
            {
                insn.Operand1 = instr.rs1(); // get uimm as Operand1 without accessing memory
                insn.Operand1Ready = true;   // set Operand1 ready
            }
            else
            /* ecall, ebreak; CSRRW, CSRRS, CSRRC -- handled in following switch
             * WFI, Supervisor MM instructions, Hypervisor MM instructions -- handled here
             */
            {
                insn.Rs1Type = Emulator::RegType_t::INT; // only 2 types of regs: NONE, INT
                // TODO: WFI, Supervisor MM instructions, Hypervisor MM instructions
            }

            /* 14:12--funct3: used to decide specific instruction */
            switch (instr.func3() & 0b11)
            {
            case 0b00: // funct3=x00: ecall, ebreak; mret, sret
                if (instr.rd() == 0 && instr.rs1() == 0)
                {
                    insn.Rs1Type = Emulator::RegType_t::NONE;
                    switch (instr.iimm())
                    {
                    case 0b000000000000:
                        insn.SubOp = CSR_ECALL;
                        insn.ControlFlowInsn = true;
                        ecall = true;
                        break;
                    case 0b000000000001:
                        insn.SubOp = CSR_EBREAK;
                        insn.ControlFlowInsn = true;
                        ebreak = true;
                        break;
                    case 0b001100000010:
                        insn.SubOp = CSR_MRET;
                        insn.ControlFlowInsn = true;
                        if (priv_level != Priv_level_t::PRIV_M)
                        {
                            illegal_instr = true;
                        }
                        break;
                    case 0b000100000010:
                        insn.SubOp = CSR_SRET;
                        insn.ControlFlowInsn = true;
                        if (priv_level == Priv_level_t::PRIV_U)
                        {
                            illegal_instr = true;
                        }
                        break;
                    default:
                        illegal_instr = true;
                        break;
                    }
                }
                else
                {
                    illegal_instr = true;
                }
                break;
            case 0b01:                  // funct3=x01
                insn.SubOp = CSR_CSRRW; // what about CSR_CSRRWI?
                insn.RdType = Emulator::RegType_t::INT;
                break;
            case 0b10: // funct3=x10
                if (instr.rs1() == 0)
                {
                    insn.SubOp = CSR_CSRR;
                    insn.Rs1Type = Emulator::RegType_t::NONE;
                    insn.RdType = Emulator::RegType_t::INT;
                }
                else
                {
                    insn.SubOp = CSR_CSRRS;
                    insn.RdType = Emulator::RegType_t::INT;
                }
                break;
            case 0b11: // funct3=x11, CSRRCI?
                if (instr.rs1() == 0)
                {
                    insn.SubOp = CSR_CSRR;
                    insn.Rs1Type = Emulator::RegType_t::NONE;
                    insn.RdType = Emulator::RegType_t::INT;
                }
                else
                {
                    insn.SubOp = CSR_CSRRC;
                    insn.RdType = Emulator::RegType_t::INT;
                }
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        case OpcodeMiscMem: // I-type, 0001111: FENCE(RV32I), FENCE.I(Zifencei)
            insn.Fu = Emulator::funcType_t::CSR;
            insn.imm = instr.iimm();
            if (instr.rs1() == 0 && instr.rd() == 0)
            {
                switch (instr.func3())
                {
                case 0b000:
                    insn.Fu = Emulator::funcType_t::CSR;
                    insn.SubOp = CSR_FENCE;
                    break;
                case 0b001:
                    if (instr.iimm() != 0)
                    {
                        illegal_instr = true;
                        break;
                    }
                    insn.SubOp = CSR_FENCEI;
                    insn.ControlFlowInsn = true;
                    break;
                default:
                    break;
                }
            }
            else
            {
                illegal_instr = true;
            }
            break;
        case OpcodeLoad: // 0000011, I-type: LB, LH, LW, LBU, LHU, LWU, LD
            insn.Fu = Emulator::funcType_t::LDU;
            insn.imm = instr.iimm();
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.RdType = Emulator::RegType_t::INT;
            switch (instr.func3())
            {
            case 0b000:
                insn.SubOp = LDU_LB;
                break;
            case 0b001:
                insn.SubOp = LDU_LH;
                break;
            case 0b010:
                insn.SubOp = LDU_LW;
                break;
            case 0b011:
                insn.SubOp = LDU_LD;
                break;
            case 0b100:
                insn.SubOp = LDU_LBU;
                break;
            case 0b101:
                insn.SubOp = LDU_LHU;
                break;
            case 0b110:
                insn.SubOp = LDU_LWU;
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        case OpcodeStore:
            insn.Fu = Emulator::funcType_t::STU;
            insn.imm = instr.simm();
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.Rs2Type = Emulator::RegType_t::INT;
            switch (instr.func3())
            {
            case 0b000:
                insn.SubOp = STU_SB;
                break;
            case 0b001:
                insn.SubOp = STU_SH;
                break;
            case 0b010:
                insn.SubOp = STU_SW;
                break;
            case 0b011:
                insn.SubOp = STU_SD;
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        // case OpcodeAmo:
        // insn.Fu = Emulator::funcType_t::ST;
        // 	break;
        case OpcodeBranch: // 1100011, B-type
            insn.Fu = Emulator::funcType_t::BRU;
            insn.imm = instr.sbimm();
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.Rs2Type = Emulator::RegType_t::INT;
            insn.ControlFlowInsn = true;
            switch (instr.func3())
            {
            case 0b000:
                insn.SubOp = BRU_BEQ;
                break;
            case 0b001:
                insn.SubOp = BRU_BNE;
                break;
            case 0b100:
                insn.SubOp = BRU_BLT;
                break;
            case 0b101:
                insn.SubOp = BRU_BGE;
                break;
            case 0b110:
                insn.SubOp = BRU_BLTU;
                break;
            case 0b111:
                insn.SubOp = BRU_BGEU;
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        case OpcodeJal:
            insn.Fu = Emulator::funcType_t::ALU;
            insn.SubOp = ALU_ADD;
            insn.Operand1 = insn.Pc;
            insn.Operand2 = insn.IsRvcInsn ? 2 : 4;
            insn.Operand1Ready = true;
            insn.Operand2Ready = true;
            insn.RdType = Emulator::RegType_t::INT;
            break;
        case OpcodeJalr:
            insn.Fu = Emulator::funcType_t::BRU;
            insn.SubOp = BRU_JALR;
            if (instr.func3() != 0)
            {
                illegal_instr = true;
            }
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.imm = instr.iimm();
            insn.ControlFlowInsn = true;
            insn.RdType = Emulator::RegType_t::INT;
            break;
        case OpcodeOp:
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.Rs2Type = Emulator::RegType_t::INT;
            insn.RdType = Emulator::RegType_t::INT;
            switch (instr.func7())
            {
            // imm=0000000 or 0100000, RV32I Base Instruction Set
            case 0b0000000:
                insn.Fu = Emulator::funcType_t::ALU;
                switch (instr.func3())
                {
                case 0b000:
                    insn.SubOp = ALU_ADD;
                    break;
                case 0b001:
                    insn.SubOp = ALU_SLL;
                    break;
                case 0b010:
                    insn.SubOp = ALU_SLT;
                    break;
                case 0b011:
                    insn.SubOp = ALU_SLTU;
                    break;
                case 0b100:
                    insn.SubOp = ALU_XOR;
                    break;
                case 0b101:
                    insn.SubOp = ALU_SRL;
                    break;
                case 0b110:
                    insn.SubOp = ALU_OR;
                    break;
                case 0b111:
                    insn.SubOp = ALU_AND;
                    break;
                default:
                    illegal_instr = true;
                    break;
                }
                break;
            case 0b0100000:
                insn.Fu = Emulator::funcType_t::ALU;
                switch (instr.func3())
                {
                case 0b000:
                    insn.SubOp = ALU_SUB;
                    break;
                case 0b101:
                    insn.SubOp = ALU_SRA;
                    break;
                default:
                    illegal_instr = true;
                    break;
                }
                break;
            case 0b0000001: // imm=0000001, RV32M Standard Extension
                insn.Fu = (instr.func3() >> 2) ? Emulator::funcType_t::DIV : Emulator::funcType_t::MUL;
                switch (instr.func3())
                {
                case 0b000:
                    insn.SubOp = MUL_MUL;
                    break;
                case 0b001:
                    insn.SubOp = MUL_MULH;
                    break;
                case 0b010:
                    insn.SubOp = MUL_MULHSU;
                    break;
                case 0b011:
                    insn.SubOp = MUL_MULHU;
                    break;
                case 0b100:
                    /* code */
                    insn.SubOp = DIV_DIV;
                    break;
                case 0b101:
                    /* code */
                    insn.SubOp = DIV_DIVU;
                    break;
                case 0b110:
                    /* code */
                    insn.SubOp = DIV_REM;
                    break;
                case 0b111:
                    /* code */
                    insn.SubOp = DIV_REMU;
                    break;
                default:
                    break;
                }
                break;
            default:
                illegal_instr = true;
                break;
            }

            break;
        case OpcodeOp32: // part of RV64I, RV64M Standard Extension
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.Rs2Type = Emulator::RegType_t::INT;
            insn.RdType = Emulator::RegType_t::INT;
            switch (instr.func7())
            {
            case 0b0000000:
                insn.Fu = Emulator::funcType_t::ALU;
                switch (instr.func3())
                {
                case 0b000:
                    insn.SubOp = ALU_ADDW;
                    break;
                case 0b001:
                    insn.SubOp = ALU_SLLW;
                    break;
                case 0b101:
                    insn.SubOp = ALU_SRLW;
                    break;
                default:
                    illegal_instr = true;
                    break;
                }
                break;
            case 0b0100000:
                insn.Fu = Emulator::funcType_t::ALU;
                switch (instr.func3())
                {
                case 0b000:
                    insn.SubOp = ALU_SUBW;
                    break;
                case 0b101:
                    insn.SubOp = ALU_SRAW;
                    break;
                default:
                    illegal_instr = true;
                    break;
                }
                break;
            case 0b0000001:
                insn.Fu = (instr.func3() >> 2) ? Emulator::funcType_t::DIV : Emulator::funcType_t::MUL;
                switch (instr.func3())
                {
                case 0b000:
                    insn.SubOp = MUL_MULW;
                    break;
                case 0b100:
                    insn.SubOp = DIV_DIVW;
                    break;
                case 0b101:
                    insn.SubOp = DIV_DIVUW;
                    break;
                case 0b110:
                    insn.SubOp = DIV_REMW;
                    break;
                case 0b111:
                    insn.SubOp = DIV_REMUW;
                    break;
                default:
                    break;
                }
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        case OpcodeOpImm: // RV32I, RV64I Base Instruction Set
            insn.Fu = Emulator::funcType_t::ALU;
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.RdType = Emulator::RegType_t::INT;
            insn.imm = instr.iimm();
            insn.Operand2 = instr.iimm();
            insn.Operand2Ready = true;
            switch (instr.func3())
            {
            case 0b000:
                insn.SubOp = ALU_ADD;
                break;
            case 0b001:
                insn.Operand2 = instr.shamt();
                insn.SubOp = ALU_SLL;
                if ((instr.func7() >> 1) != 0)
                {
                    illegal_instr = true;
                }
                break;
            case 0b010:
                insn.SubOp = ALU_SLT;
                break;
            case 0b011:
                insn.SubOp = ALU_SLTU;
                break;
            case 0b100:
                insn.SubOp = ALU_XOR;
                break;
            case 0b101:
                insn.Operand2 = instr.shamt();
                if ((instr.func7() >> 1) == 0b000000)
                {
                    insn.SubOp = ALU_SRL;
                }
                else if ((instr.func7() >> 1) == 0b010000)
                {
                    insn.SubOp = ALU_SRA;
                }
                else
                {
                    illegal_instr = true;
                }
                break;
            case 0b110:
                insn.SubOp = ALU_OR;
                break;
            case 0b111:
                insn.SubOp = ALU_AND;
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        case OpcodeOpImm32: // TODO: ? ADDIW, SLLIW, SRLIW, SRAIW with shamt field
            insn.Fu = Emulator::funcType_t::ALU;
            insn.Rs1Type = Emulator::RegType_t::INT;
            insn.RdType = Emulator::RegType_t::INT;
            insn.imm = instr.iimm();
            insn.Operand2 = instr.iimm();
            insn.Operand2Ready = true;
            switch (instr.func3())
            {
            case 0b000:
                insn.SubOp = ALU_ADDW;
                break;
            case 0b001:
                insn.Operand2 = instr.shamt();
                insn.SubOp = ALU_SLLW;
                if (instr.func7() != 0)
                {
                    illegal_instr = true;
                }
                break;
            case 0b101:
                insn.Operand2 = instr.shamt();
                if (instr.func7() == 0b0000000)
                {
                    insn.SubOp = ALU_SRLW;
                }
                else if (instr.func7() == 0b0100000)
                {
                    insn.SubOp = ALU_SRAW;
                }
                else
                {
                    illegal_instr = true;
                }
                break;
            default:
                illegal_instr = true;
                break;
            }
            break;
        case OpcodeAuipc:
            insn.Fu = Emulator::funcType_t::ALU;
            insn.SubOp = ALU_ADD;
            insn.Operand1 = instr.uimm();
            insn.Operand2 = insn.Pc;
            insn.Operand1Ready = true;
            insn.Operand2Ready = true;
            insn.imm = instr.uimm();
            insn.RdType = Emulator::RegType_t::INT;
            break;
        case OpcodeLui:
            insn.Fu = Emulator::funcType_t::ALU;
            insn.SubOp = ALU_ADD;
            insn.Operand1 = instr.uimm();
            insn.Operand2 = 0;
            insn.Operand1Ready = true;
            insn.Operand2Ready = true;
            insn.imm = instr.uimm();
            insn.RdType = Emulator::RegType_t::INT;
            break;
        default:
            illegal_instr = true;
            break;
        }

        insn.IsaRd = (insn.RdType == Emulator::RegType_t::NONE) ? 0 : insn.IsaRd;
        insn.IsaRs1 = (insn.Rs1Type == Emulator::RegType_t::NONE) ? 0 : insn.IsaRs1;
        insn.IsaRs2 = (insn.Rs2Type == Emulator::RegType_t::NONE) ? 0 : insn.IsaRs2;

        if (insn.IsaRd == 0 && insn.RdType == Emulator::RegType_t::INT)
        {
            insn.RdType = Emulator::RegType_t::NONE;
        }

        if (ecall)
        {
            insn.Excp.valid = true;
            insn.Excp.Tval = 0;
            switch (priv_level)
            {
            case Priv_level_t::PRIV_M:
                insn.Excp.Cause = RISCV::ENV_CALL_MMODE;
                break;
            case Priv_level_t::PRIV_S:
                insn.Excp.Cause = RISCV::ENV_CALL_SMODE;
                break;
            case Priv_level_t::PRIV_U:
                insn.Excp.Cause = RISCV::ENV_CALL_UMODE;
                break;
            default:
                illegal_instr = true;
                break;
            }
        }
        else if (ebreak)
        {
            insn.Excp.valid = true;
            insn.Excp.Cause = RISCV::BREAKPOINT;
            insn.Excp.Tval = 0;
        }

        if (illegal_instr)
        {
            insn.Excp.valid = true;
            insn.Excp.Cause = RISCV::ILLEGAL_INSTR;
            insn.Excp.Tval = insn.CompressedInsn;
        }
    }

} // namespace RISCV

#endif