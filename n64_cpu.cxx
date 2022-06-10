#include "n64_cpu.hxx"
#include <cstring> // memcpy
#include <cassert> // assert
#include "../include/error_factory.hxx"
#include "n64_addresses.hxx"
#define SKIPDEBUGSTUFF 1
#define TKP_INSTR_FUNC void
constexpr uint64_t LUT[] = {
    0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF, 0, 0, 0, 0xFFFFFFFFFFFFFFFF,
};

namespace TKPEmu::N64::Devices {
    CPU::CPU(CPUBus& cpubus, RCP& rcp, GLuint& text_format)  :
        gpr_regs_{},
        fpr_regs_{},
        instr_cache_(KB(16)),
        data_cache_(KB(8)),
        cpubus_(cpubus),
        rcp_(rcp),
        text_format_(text_format)
    {
    }

    void CPU::Reset() {
        pc_ = 0xBFC0'0000;
        ldi_ = false;
        clear_registers();
        cpubus_.Reset();
        if (cpubus_.IsEverythingLoaded())
            fill_pipeline();
    }

    TKP_INSTR_FUNC CPU::ERROR() {
        throw ErrorFactory::generate_exception(__func__, __LINE__, "ERROR opcode reached");
    }

    /**
     * s_SRA
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_SRA() {
        int32_t sedata = rfex_latch_.fetched_rt.D >> rfex_latch_.instruction.RType.sa;
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = sedata;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}

    /**
     * s_SLLV
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_SLLV() {
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        int64_t sedata = static_cast<int32_t>(rfex_latch_.fetched_rt.UW._0 << (rfex_latch_.fetched_rs.UD & 0b111111));
        exdc_latch_.data = sedata;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}

    /**
     * s_SRLV
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_SRLV() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        int64_t sedata = static_cast<int64_t>(static_cast<int32_t>(rfex_latch_.fetched_rt.UW._0 >> (rfex_latch_.fetched_rs.UD & 0b111111)));
        exdc_latch_.data = sedata;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::s_SRAV() {
		int32_t sedata = rfex_latch_.fetched_rt.D >> (rfex_latch_.fetched_rs.UD & 0b11111);
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = sedata;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::s_SYSCALL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_SYSCALL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_BREAK() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_BREAK opcode reached");
	}

    TKP_INSTR_FUNC CPU::s_SYNC() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_SYNC opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_MFHI() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_MFHI opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_MTHI() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_MTHI opcode reached");
	}

    TKP_INSTR_FUNC CPU::s_MFLO() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_MFLO opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DSRLV() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSRLV opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DSRAV() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSRAV opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_MULT() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_MULT opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_MULTU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_MULTU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DIV() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DIV opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DIVU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DIVU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DMULT() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DMULT opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DMULTU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DMULTU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DDIV() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DDIV opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DDIVU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DDIVU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_SUB() {
		int64_t result = 0;
		bool overflow = __builtin_sub_overflow(rfex_latch_.fetched_rs.D, rfex_latch_.fetched_rt.D, &result);
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = static_cast<int32_t>(result);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
        #if SKIPEXCEPTIONS == 0
        if (overflow) {
            // An integer overflow exception occurs if carries out of bits 30 and 31 differ (2’s
            // complement overflow). The contents of destination register rt is not modified
            // when an integer overflow exception occurs.
            exdc_latch_.write_type = WriteType::NONE;
            throw IntegerOverflowException();
        }
        #endif
	}
    
    TKP_INSTR_FUNC CPU::s_SUBU() {
        int64_t result = 0;
		bool overflow = __builtin_sub_overflow(rfex_latch_.fetched_rs.D, rfex_latch_.fetched_rt.D, &result);
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = static_cast<int32_t>(result);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    /**
     * s_OR
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_OR() {
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD | rfex_latch_.fetched_rt.UD;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::s_XOR() {
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD ^ rfex_latch_.fetched_rt.UD;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::s_DADD() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DADD opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DADDU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DADDU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DSUB() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSUB opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DSUBU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSUBU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_TGEU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_TGEU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_TLT() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_TLT opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_TLTU() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_TLTU opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_TEQ() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_TEQ opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_TNE() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_TNE opcode reached");
	}
    
    /**
     * s_DSLL
     * 
     * throws ReservedInstructionException
     */
    TKP_INSTR_FUNC CPU::s_DSLL() {
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rt.UD << rfex_latch_.instruction.RType.sa;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::s_DSRL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSRL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DSRA() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSRA opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_DSRL32() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_DSRL32 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SPECIAL() {
        (SpecialTable[rfex_latch_.instruction.RType.func])(this);
	}

    TKP_INSTR_FUNC CPU::REGIMM() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "REGIMM opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::BLEZ() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "BLEZ opcode reached");
	}
    
    /**
     * s_SLTI
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::SLTI() {
        int64_t seimm = static_cast<int16_t>(rfex_latch_.instruction.IType.immediate);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.D < seimm;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::XORI() {
		exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD ^ rfex_latch_.instruction.IType.immediate;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::COP1() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "COP1 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::COP2() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "COP2 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::BGTZL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "BGTZL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::DADDIU() {
		int64_t seimm = static_cast<int16_t>(rfex_latch_.instruction.IType.immediate);
        int64_t result = 0;
        bool overflow = __builtin_add_overflow(rfex_latch_.fetched_rs.D, seimm, &result);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = result;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::LDL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LDL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LDR() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LDR opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LWL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LWL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LWR() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LWR opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SB() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SB opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SWL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SWL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SDL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SDL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::CACHE() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "CACHE opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LWC1() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LWC1 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LWC2() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LWC2 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LLD() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LLD opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LDC1() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LDC1 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LDC2() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LDC2 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SC() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SC opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SWC1() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SWC1 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SWC2() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SWC2 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SCD() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SCD opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SDC1() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SDC1 opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SDC2() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SDC2 opcode reached");
	}

    TKP_INSTR_FUNC CPU::SLTIU() {
        int64_t seimm = static_cast<int16_t>(rfex_latch_.instruction.IType.immediate);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD < seimm;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
	}
    
    TKP_INSTR_FUNC CPU::SDR() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SDR opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::SWR() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "SWR opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::LL() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "LL opcode reached");
	}
    
    TKP_INSTR_FUNC CPU::s_MTLO() {
		throw ErrorFactory::generate_exception(__func__, __LINE__, "s_MTLO opcode reached");
	}
    
    /**
     * ADDI, ADDIU
     * 
     * ADDI throws Integer overflow exception
     */
    TKP_INSTR_FUNC CPU::ADDIU() {
        int32_t seimm = static_cast<int16_t>(rfex_latch_.instruction.IType.immediate);
        int32_t result = 0;
        bool overflow = __builtin_add_overflow(rfex_latch_.fetched_rs.W._0, seimm, &result);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = result;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    TKP_INSTR_FUNC CPU::ADDI() {
        int32_t seimm = static_cast<int16_t>(rfex_latch_.instruction.IType.immediate);
        int32_t result = 0;
        bool overflow = __builtin_add_overflow(rfex_latch_.fetched_rs.W._0, seimm, &result);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = result;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
        #if SKIPEXCEPTIONS == 0
        if (overflow) {
            // An integer overflow exception occurs if carries out of bits 30 and 31 differ (2’s
            // complement overflow). The contents of destination register rt is not modified
            // when an integer overflow exception occurs.
            exdc_latch_.write_type = WriteType::NONE;
            throw IntegerOverflowException();
        }
        #endif
    }
    /**
     * DADDI
     * 
     * throws IntegerOverflowException
     *        ReservedInstructionException
     */
    TKP_INSTR_FUNC CPU::DADDI() {
        int64_t seimm = static_cast<int16_t>(rfex_latch_.instruction.IType.immediate);
        int64_t result = 0;
        bool overflow = __builtin_add_overflow(rfex_latch_.fetched_rs.D, seimm, &result);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = result;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
        #if SKIPEXCEPTIONS == 0
        if (overflow) {
            // An integer overflow exception occurs if carries out of bits 30 and 31 differ (2’s
            // complement overflow). The contents of destination register rt is not modified
            // when an integer overflow exception occurs.
            exdc_latch_.write_type = WriteType::NONE;
            throw IntegerOverflowException();
        }
        #endif
    }
    /**
     * J, JAL
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::JAL() {
        gpr_regs_[31].UD = pc_; // By the time this instruction is executed, pc is already incremented by 8
                                    // so there's no need to increment here
        J();
    }
    TKP_INSTR_FUNC CPU::J() {
        auto jump_addr = rfex_latch_.instruction.JType.target;
        // combine first 3 bits of pc and jump_addr shifted left by 2
        exdc_latch_.data = (pc_ & 0xF000'0000) | (jump_addr << 2);
        exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		bypass_register();
    }
    /**
     * LUI
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::LUI() {
        int32_t imm = rfex_latch_.instruction.IType.immediate << 16;
        uint64_t seimm = static_cast<int64_t>(imm);
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = seimm;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * COP0
     */
    TKP_INSTR_FUNC CPU::COP0() {
        execute_cp0_instruction(rfex_latch_.instruction);
    }
    /**
     * ORI
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::ORI() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD | rfex_latch_.instruction.IType.immediate;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_AND
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_AND() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD & rfex_latch_.fetched_rt.UD;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * SD
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        TLB modification exception
     *        Bus error exception
     *        Address error exception
     *        Reserved instruction exception (32-bit User or Supervisor mode)
     */
    TKP_INSTR_FUNC CPU::SD() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        auto write_vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        auto paddr_s = translate_vaddr(write_vaddr);
        exdc_latch_.paddr = paddr_s.paddr;
        exdc_latch_.cached = paddr_s.cached;
        exdc_latch_.data = rfex_latch_.fetched_rt.UD;
        exdc_latch_.write_type = WriteType::MMU;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
        #if SKIPEXCEPTIONS == 0
        if ((exdc_latch_.vaddr & 0b111) != 0) { 
            // From manual:
            // If either of the loworder two bits of the address are not zero, an address error exception occurs.
            throw InstructionAddressErrorException();
        }
        if (!mode64_ && opmode_ != OperatingMode::Kernel) {
            // From manual:
            // This operation is defined for the VR4300 operating in 64-bit mode and in 32-bit
            // Kernel mode. Execution of this instruction in 32-bit User or Supervisor mode
            // causes a reserved instruction exception.
            throw ReservedInstructionException();
        }
        #endif
    }
    /**
     * SW
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        TLB modification exception
     *        Bus error exception
     *        Address error exception
     */
    TKP_INSTR_FUNC CPU::SW() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        auto write_vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        auto paddr_s = translate_vaddr(write_vaddr);
        exdc_latch_.paddr = paddr_s.paddr;
        exdc_latch_.cached = paddr_s.cached;
        exdc_latch_.data = rfex_latch_.fetched_rt.UW._0;
        exdc_latch_.write_type = WriteType::MMU;
        exdc_latch_.access_type = AccessType::UWORD;
        #if SKIPEXCEPTIONS == 0
        if ((exdc_latch_.dest & 0b11) != 0) {
            // From manual:
            // If either of the loworder two bits of the address are not zero, an address error exception occurs.
            throw InstructionAddressErrorException();
        }
        #endif
    }
    /**
     * SH
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        TLB modification exception
     *        Bus error exception
     *        Address error exception
     */
    TKP_INSTR_FUNC CPU::SH() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        auto write_vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        auto paddr_s = translate_vaddr(write_vaddr);
        exdc_latch_.paddr = paddr_s.paddr;
        exdc_latch_.cached = paddr_s.cached;
        exdc_latch_.data = rfex_latch_.fetched_rt.UH._0;
        exdc_latch_.write_type = WriteType::MMU;
        exdc_latch_.access_type = AccessType::UHALFWORD;
        #if SKIPEXCEPTIONS == 0
        if ((exdc_latch_.dest & 0b11) != 0) {
            // From manual:
            // If either of the loworder two bits of the address are not zero, an address error exception occurs.
            throw InstructionAddressErrorException();
        }
        #endif
    }
    /**
     * LB, LBU
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        Bus error exception
     *        Address error exception (?)
     */
    TKP_INSTR_FUNC CPU::LB() {
        LBU();
    }
    TKP_INSTR_FUNC CPU::LBU() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        exdc_latch_.write_type = WriteType::LATEREGISTER;
        exdc_latch_.access_type = AccessType::UBYTE;
        exdc_latch_.fetched_rt_i = rfex_latch_.instruction.IType.rt;
        detect_ldi();
    }
    /**
     * LD
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        Bus error exception
     *        Address error exception
     */
    TKP_INSTR_FUNC CPU::LD() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        exdc_latch_.write_type = WriteType::LATEREGISTER;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
        exdc_latch_.fetched_rt_i = rfex_latch_.instruction.IType.rt;
        detect_ldi();
        #if SKIPEXCEPTIONS == 0
        if ((exdc_latch_.vaddr & 0b111) != 0) { 
            // From manual:
            // If either of the loworder two bits of the address are not zero, an address error exception occurs.
            throw InstructionAddressErrorException();
        }
        if (!mode64_ && opmode_ != OperatingMode::Kernel) {
            // From manual:
            // This operation is defined for the VR4300 operating in 64-bit mode and in 32-bit
            // Kernel mode. Execution of this instruction in 32-bit User or Supervisor mode
            // causes a reserved instruction exception.
            throw ReservedInstructionException();
        }
        #endif
    }
    /**
     * LH, LHU
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        Bus error exception
     *        Address error exception
     */
    TKP_INSTR_FUNC CPU::LHU() {
        LH();
    }
    TKP_INSTR_FUNC CPU::LH() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        exdc_latch_.write_type = WriteType::LATEREGISTER;
        exdc_latch_.access_type = AccessType::UHALFWORD;
        exdc_latch_.fetched_rt_i = rfex_latch_.instruction.IType.rt;
        exdc_latch_.fetched_rs_i = rfex_latch_.instruction.IType.rs;
        #if SKIPEXCEPTIONS == 0
        if ((exdc_latch_.vaddr & 0b1) != 0) {
            // From manual:
            // If the least-significant bit of the address is not zero, an address error exception occurs.
            throw InstructionAddressErrorException();
        }
        #endif
    }
    /**
     * LW, LWU
     * 
     * throws TLB miss exception
     *        TLB invalid exception
     *        Bus error exception
     *        Address error exception
     */
    TKP_INSTR_FUNC CPU::LWU() {
        LW();
    }
    TKP_INSTR_FUNC CPU::LW() {
        int16_t offset = rfex_latch_.instruction.IType.immediate;
        int32_t seoffset = offset;
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.vaddr = seoffset + rfex_latch_.fetched_rs.UW._0;
        exdc_latch_.write_type = WriteType::LATEREGISTER;
        exdc_latch_.access_type = AccessType::UWORD;
        exdc_latch_.fetched_rt_i = rfex_latch_.instruction.IType.rt;
        detect_ldi();
        #if SKIPEXCEPTIONS == 0
        if ((exdc_latch_.vaddr & 0b11) != 0) {
            // From manual:
            // If either of the loworder two bits of the address are not zero, an address error exception occurs.
            throw InstructionAddressErrorException();
        }
        #endif
    }
    /**
     * ANDI
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::ANDI() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.IType.rt].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD & rfex_latch_.instruction.IType.immediate;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * BEQ
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::BEQ() {
        int16_t offset = rfex_latch_.instruction.IType.immediate << 2;
        int32_t seoffset = offset;
        if (rfex_latch_.fetched_rs.UD == rfex_latch_.fetched_rt.UD) {
            exdc_latch_.data = pc_ - 4 + seoffset;
            exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
            exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		bypass_register();
        }
    }
    /**
     * BEQL
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::BEQL() {
            int16_t offset = rfex_latch_.instruction.IType.immediate << 2;
        int32_t seoffset = offset;
        if (rfex_latch_.fetched_rs.UD == rfex_latch_.fetched_rt.UD) {
            exdc_latch_.data = pc_ - 4 + seoffset;
            exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
            exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		bypass_register();
        } else {
            // Discard delay slot instruction
            icrf_latch_.instruction.Full = 0;
        }
    }
    /**
     * BNE
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::BNE() {
        int16_t offset = rfex_latch_.instruction.IType.immediate << 2;
        int32_t seoffset = offset;
        if (rfex_latch_.fetched_rs.UD != rfex_latch_.fetched_rt.UD) {
            exdc_latch_.data = pc_ - 4 + seoffset;
            exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
            exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		    bypass_register();
        }
    }
    /**
     * BNEL
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::BNEL() {
        int16_t offset = rfex_latch_.instruction.IType.immediate << 2;
        int32_t seoffset = offset;
        if (rfex_latch_.fetched_rs.UD != rfex_latch_.fetched_rt.UD) {
            exdc_latch_.data = pc_ - 4 + seoffset;
            exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
            exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		    bypass_register();
        } else {
            // Discard delay slot instruction
            icrf_latch_.instruction.Full = 0;
        }
    }
    /**
     * BLEZL
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::BLEZL() {
        int16_t offset = rfex_latch_.instruction.IType.immediate << 2;
        int32_t seoffset = offset;
        if (rfex_latch_.fetched_rs.D <= 0) {
            exdc_latch_.data = pc_ - 4 + seoffset;
            exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
            exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		bypass_register();
        } else {
            // Discard delay slot instruction
            icrf_latch_.instruction.Full = 0;
        }
    }
    /**
     * BGTZ
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::BGTZ() {
        int16_t offset = rfex_latch_.instruction.IType.immediate << 2;
        int32_t seoffset = offset;
        if (rfex_latch_.fetched_rs.D > 0) {
            exdc_latch_.data = pc_ - 4 + seoffset;
            exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
            exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		    bypass_register();
        }
    }
    /**
     * TGE
     * 
     * throws TrapException
     */
    TKP_INSTR_FUNC CPU::s_TGE() {
        if (rfex_latch_.fetched_rs.UD >= rfex_latch_.fetched_rt.UD)
            throw ErrorFactory::generate_exception(__func__, __LINE__, "TrapException was thrown");
    }
    /**
     * s_ADD, s_ADDU
     * 
     * s_ADD throws IntegerOverflowException
     */
    TKP_INSTR_FUNC CPU::s_ADD() {
        int32_t result = 0;
        bool overflow = __builtin_add_overflow(rfex_latch_.fetched_rt.W._0, rfex_latch_.fetched_rs.W._0, &result);
        int64_t seresult = result;
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = seresult;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
        #if SKIPEXCEPTIONS == 0
        if (overflow) {
            // An integer overflow exception occurs if carries out of bits 30 and 31 differ (2’s
            // complement overflow). The contents of destination register rd is not modified
            // when an integer overflow exception occurs.
            exdc_latch_.write_type = WriteType::NONE;
            throw IntegerOverflowException();
        }
        #endif
    }
    TKP_INSTR_FUNC CPU::s_ADDU() {
        int32_t result = 0;
        bool overflow = __builtin_add_overflow(rfex_latch_.fetched_rt.W._0, rfex_latch_.fetched_rs.W._0, &result);
        int64_t seresult = result;
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = seresult;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_JR, s_JALR
     * 
     * throws Address error exception
     */
    TKP_INSTR_FUNC CPU::s_JALR() {
        auto reg = (rfex_latch_.instruction.RType.rd == 0) ? 31 : rfex_latch_.instruction.RType.rd;
        gpr_regs_[reg].UD = pc_; // By the time this instruction is executed, pc is already incremented by 8
                                    // so there's no need to increment here
        #if SKIPEXCEPTIONS == 0
        // From manual:
        // Register numbers rs and rd should not be equal, because such an instruction does
        // not have the same effect when re-executed. If they are equal, the contents of rs
        // are destroyed by storing link address. However, if an attempt is made to execute
        // this instruction, an exception will not occur, and the result of executing such an
        // instruction is undefined.
        assert(rfex_latch_.instruction.RType.rd != rfex_latch_.instruction.RType.rs);
        #endif
        s_JR();
    }
    TKP_INSTR_FUNC CPU::s_JR() {
        auto jump_addr = rfex_latch_.fetched_rs.UD;
        exdc_latch_.data = jump_addr;
        exdc_latch_.dest = reinterpret_cast<uint8_t*>(&pc_);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD_DIRECT;
		bypass_register();
        #if SKIPEXCEPTIONS == 0
        if ((jump_addr & 0b11) != 0) {
            // From manual:
            // Since instructions must be word-aligned, a Jump Register instruction must
            // specify a target register (rs) which contains an address whose low-order two bits
            // are zero. If these low-order two bits are not zero, an address exception will occur
            // when the jump target instruction is fetched.
            // TODO: when the jump target instruction is *fetched*. Does it matter that the exc is thrown here?
            throw InstructionAddressErrorException();
        }
        #endif
    }
    /**
     * s_DSLL32
     * 
     * throws Reserved instruction exception
     */
    TKP_INSTR_FUNC CPU::s_DSLL32() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rt.UD << (rfex_latch_.instruction.RType.sa + 32);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_DSLLV
     * 
     * throws Reserved instruction exception
     */
    TKP_INSTR_FUNC CPU::s_DSLLV() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rt.UD << (rfex_latch_.fetched_rs.UD & 0b111111);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_SLL
     * 
     * throws Reserved instruction exception
     */
    TKP_INSTR_FUNC CPU::s_SLL() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        int32_t sedata = rfex_latch_.fetched_rt.UW._0 << rfex_latch_.instruction.RType.sa;
        exdc_latch_.data = static_cast<int64_t>(sedata);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_SRL
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_SRL() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        int64_t sedata = static_cast<int64_t>(static_cast<int32_t>(rfex_latch_.fetched_rt.UW._0 >> rfex_latch_.instruction.RType.sa));
        exdc_latch_.data = sedata;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_DSRA32
     * 
     * throws Reserved instruction exception
     */
    TKP_INSTR_FUNC CPU::s_DSRA32() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        int64_t sedata = rfex_latch_.fetched_rt.D >> (rfex_latch_.instruction.RType.sa + 32);
        exdc_latch_.data = sedata;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_SLT
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_SLT() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.D < rfex_latch_.fetched_rt.D;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_SLTU
     * 
     * doesn't throw 
     */
    TKP_INSTR_FUNC CPU::s_SLTU() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = rfex_latch_.fetched_rs.UD < rfex_latch_.fetched_rt.UD;
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * s_NOR
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::s_NOR() {
        exdc_latch_.dest = &gpr_regs_[rfex_latch_.instruction.RType.rd].UB._0;
        exdc_latch_.data = ~(rfex_latch_.fetched_rs.UD | rfex_latch_.fetched_rt.UD);
        exdc_latch_.access_type = AccessType::UDOUBLEWORD;
		bypass_register();
    }
    /**
     * NOP
     * 
     * doesn't throw
     */
    TKP_INSTR_FUNC CPU::NOP() {
        return;
    }

    CPU::PipelineStageRet CPU::IC(PipelineStageArgs) {
        // Fetch the current process instruction
        auto paddr_s = translate_vaddr(pc_);
        icrf_latch_.instruction.Full = cpubus_.fetch_instruction_uncached(paddr_s.paddr);
        pc_ += 4;
    }

    CPU::PipelineStageRet CPU::RF(PipelineStageArgs) {
        // Fetch the registers
        // Double negation makes the target 0 or 1 based on if instr is 0 or not
        // This makes it pick from the NOP table if it's a NOP
        rfex_latch_.instruction_target = !!(icrf_latch_.instruction.Full);
        rfex_latch_.instruction_type = !!(icrf_latch_.instruction.Full) * icrf_latch_.instruction.IType.op;
        rfex_latch_.fetched_rs_i = icrf_latch_.instruction.RType.rs;
        rfex_latch_.fetched_rt_i = icrf_latch_.instruction.RType.rt;
        rfex_latch_.fetched_rs.UD = gpr_regs_[icrf_latch_.instruction.RType.rs].UD;
        rfex_latch_.fetched_rt.UD = gpr_regs_[icrf_latch_.instruction.RType.rt].UD;
        rfex_latch_.instruction = icrf_latch_.instruction;
    }

    CPU::PipelineStageRet CPU::EX(PipelineStageArgs) {
        execute_instruction();
    }

    CPU::PipelineStageRet CPU::DC(PipelineStageArgs) {
        dcwb_latch_.data = 0;
        dcwb_latch_.access_type = exdc_latch_.access_type;
        dcwb_latch_.dest = exdc_latch_.dest;
        dcwb_latch_.write_type = exdc_latch_.write_type;
        dcwb_latch_.cached = exdc_latch_.cached;
        dcwb_latch_.paddr = exdc_latch_.paddr;
        if (exdc_latch_.write_type == WriteType::LATEREGISTER) {
            auto paddr_s = translate_vaddr(exdc_latch_.vaddr);
            uint32_t paddr = paddr_s.paddr;
            dcwb_latch_.cached = paddr_s.cached;
            load_memory(dcwb_latch_.cached, paddr, dcwb_latch_.data, dcwb_latch_.access_type);
            // Result is cast to uint64_t in order to zero extend
            dcwb_latch_.access_type = AccessType::UDOUBLEWORD;
            if (ldi_) {
                // Write early so RF fetches the correct data
                // TODO: technically not accurate behavior but the results are as expected
                store_register(dcwb_latch_.dest, dcwb_latch_.data, dcwb_latch_.access_type);
                dcwb_latch_.write_type = WriteType::NONE;
                ldi_ = false;
            }
        } else {
            dcwb_latch_.data = exdc_latch_.data;
        }
    }

    CPU::PipelineStageRet CPU::WB(PipelineStageArgs) {
        switch(dcwb_latch_.write_type) {
            case WriteType::MMU: {
                store_memory(dcwb_latch_.cached, dcwb_latch_.paddr, dcwb_latch_.data, dcwb_latch_.access_type);
                break;
            }
            case WriteType::LATEREGISTER: {
                store_register(dcwb_latch_.dest, dcwb_latch_.data, dcwb_latch_.access_type);
                break;
            }
            default: {
                break;
            }
        }
    }

    uint32_t CPU::translate_kseg0(uint32_t vaddr) noexcept {
        return vaddr - KSEG0_START;
    }

    uint32_t CPU::translate_kseg1(uint32_t vaddr) noexcept {
        return vaddr - KSEG1_START;
    }

    uint32_t CPU::translate_kuseg(uint32_t vaddr) noexcept {
        throw NotImplementedException(__func__);
    }

    TranslatedAddress CPU::translate_vaddr(uint32_t addr) {
        // Fast vaddr translation
        // TODO: broken if uses kuseg
        return { addr - KSEG0_START - ((addr >> 29) & 1) * 0x2000'0000, false };
    }
    void CPU::store_register(uint8_t* dest8, uint64_t data, int size) {
        // std::memcpy(dest, &data, size);
        uint64_t* dest = (uint64_t*)(dest8);
        uint64_t mask = LUT[size];
        *dest = (*dest & ~mask) | (data & mask);
    }
    void CPU::invalidate_hwio(uint32_t addr, uint64_t data) {
        if (data != 0)
        switch (addr) {
            case PI_WR_LEN_REG: {
                std::memcpy(&cpubus_.rdram_[__builtin_bswap32(cpubus_.pi_dram_addr_)], cpubus_.redirect_paddress(__builtin_bswap32(cpubus_.pi_cart_addr_)), data);
                break;
            }
            case VI_CTRL_REG: {
                auto format = data & 0b11;
                if (format == 0b10)
                    text_format_ = GL_RGB5;
                else if (format == 0b11)
                    text_format_ = GL_RGBA;
                break;
            }
            case VI_ORIGIN_REG: {
                rcp_.framebuffer_ptr_ = cpubus_.redirect_paddress(data & 0xFFFFFF);
                break;
            }
        }
    }
    void CPU::store_memory(bool cached, uint32_t paddr, uint64_t& data, int size) {
        invalidate_hwio(paddr, data);
        // if (!cached) {
        uint8_t* loc = cpubus_.redirect_paddress(paddr);
        uint64_t temp = __builtin_bswap64(data);
        temp >>= 8 * (AccessType::UDOUBLEWORD - size);
        std::memcpy(loc, &temp, size);
        // } else {
        //     // currently not implemented
        // }
    }
    void CPU::load_memory(bool cached, uint32_t paddr, uint64_t& data, int size) {
        // if (!cached) {
        uint8_t* loc = cpubus_.redirect_paddress(paddr);
        uint64_t temp = 0;
        std::memcpy(&temp, loc, size);
        temp = __builtin_bswap64(temp);
        temp >>= 8 * (AccessType::UDOUBLEWORD - size);
        data = temp;
        // } else {
        //     // currently not implemented
        // }
    }
    
    void CPU::clear_registers() {
        for (auto& reg : gpr_regs_) {
            reg.UD = 0;
        }
        for (auto& reg : fpr_regs_) {
            reg = 0.0;
        }
    }

    // TODO: probably safe to remove
    void CPU::fill_pipeline() {
        icrf_latch_ = {};
        rfex_latch_ = {};
        exdc_latch_ = {};
        dcwb_latch_ = {};
        IC();
        
        RF();
        IC();

        EX();
        RF();
        IC();

        DC();
        EX();
        RF();
        IC();
    }

    void CPU::update_pipeline() {
        gpr_regs_[0].UD = 0;
        WB();
        gpr_regs_[0].UD = 0;
        DC();
        gpr_regs_[0].UD = 0;
        EX();
        if (!ldi_) {
            gpr_regs_[0].UD = 0;
            RF();
            gpr_regs_[0].UD = 0;
            IC();
        }
        ++cp0_regs_[CP0_COUNT].UD;
        if (cp0_regs_[CP0_COUNT].UW._0 == cp0_regs_[CP0_COMPARE].UW._0) [[unlikely]] {
            // interrupt
        }
    }

    void CPU::execute_instruction() {
        (TableTable[rfex_latch_.instruction_target][rfex_latch_.instruction_type])(this);
    }

    void CPU::bypass_register() {
        store_register(exdc_latch_.dest, exdc_latch_.data, exdc_latch_.access_type);
        exdc_latch_.write_type = WriteType::NONE;
    }

    void CPU::detect_ldi() {
        ldi_ = (rfex_latch_.fetched_rt_i == icrf_latch_.instruction.RType.rt || rfex_latch_.fetched_rs_i == icrf_latch_.instruction.RType.rt);
        // Insert NOP so next EX doesn't re-execute the load in case ldi = true
        rfex_latch_.instruction_target = 0;
        rfex_latch_.instruction_type = 0;
    }

    void CPU::execute_cp0_instruction(const Instruction& instr) {
        uint32_t func = instr.RType.rs;
        switch (func) {
            /**
             * MTC0
             * 
             * throws Coprocessor unusable exception
             */
            case 0b00100: {
                int64_t sedata = gpr_regs_[instr.RType.rd].W._0;
                exdc_latch_.dest = &cp0_regs_[instr.RType.rt].UB._0;
                exdc_latch_.data = sedata;
                exdc_latch_.access_type = AccessType::UDOUBLEWORD;
                bypass_register();
                break;
            }
            /**
             * MFC0
             * 
             * throws Coprocessor unusable exception
             */
            case 0b00000: {
                int64_t sedata = cp0_regs_[instr.RType.rd].W._0;
                exdc_latch_.dest = &gpr_regs_[instr.RType.rt].UB._0;
                exdc_latch_.data = sedata;
                exdc_latch_.access_type = AccessType::UDOUBLEWORD;
                bypass_register();
                break;
            }
        }
    }
}