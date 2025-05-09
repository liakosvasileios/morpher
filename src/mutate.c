#include "mutate.h"

/*
    Supported instrutions:
        mov reg, 0 => xor reg, reg
        mov r/m64, r64 => mov r64, r/m64
        add reg, imm64 => sub reg, -imm64
        sub reg, imm64 => add reg, -imm64
        xor reg, reg => mov reg, 0
        push imm32 => sub rsp, 8 ; mov [rsp], imm32
        xchg reg, reg2 => xor reg, reg2 ; xor reg2, reg ; xor reg, reg2
        **new**
*/

void mutate_opcode(struct Instruction *inst) {

    // mov reg, 0 => xor reg, reg
    if (inst->opcode == 0xB8 && inst->imm == 0 && CHANCE(PERC)) {
        inst->opcode = 0x31;    // xor reg, reg
        inst->operand_type = OPERAND_REG | OPERAND_REG;
        inst->op2 = inst->op1;
        inst->rex |= 0x08;      // ensure 64-bit mode
    }

    // Mutate mov r/m64, r64 -> mov r64, r/m64
    else if (inst->opcode == 0x89 && inst->operand_type == (OPERAND_MEM | OPERAND_REG)) {
        if (CHANCE(PERC)) {
            inst->opcode = 0x8B;    // mov r64, r/m64
            inst->operand_type = OPERAND_REG | OPERAND_MEM;
            
            // Swap operands
            uint8_t tmp = inst->op1;
            inst->op1 = inst->op2;
            inst->op2 = tmp;
            
            // Handle REX bits for swapped operands
            if (inst->rex) {
                uint8_t rex_r = (inst->rex & 0x04); // Extract REX.R
                uint8_t rex_b = (inst->rex & 0x01); // Extract REX.B
                
                // Swap REX.R and REX.B bits
                inst->rex = (inst->rex & 0xFA) | (rex_b << 2) | (rex_r >> 2);
            }
        }
    }

    // ADD RAX, imm -> SUB RAX, -imm
    else if (inst->opcode == 0x05 && inst->operand_type == (OPERAND_REG | OPERAND_IMM) && CHANCE(PERC)) {
        inst->opcode = 0x2D;  // SUB EAX, imm32
        inst->imm = -inst->imm;
        // No need to change REX prefix - both use RAX
    }

    // SUB reg, imm -> add reg, -imm
    else if (inst->opcode == 0x2D && inst->operand_type == (OPERAND_REG | OPERAND_IMM) && CHANCE(PERC)) {
        inst->opcode = 0x05;  // ADD EAX, imm32
        inst->imm = -inst->imm;
    }
    
    // xor reg, reg -> mov reg, 0
    else if (inst->opcode == 0x31 && inst->operand_type == (OPERAND_REG | OPERAND_REG) && CHANCE(PERC)) {
        inst->opcode = 0xB8;    // mov reg, imm32
        inst->operand_type = OPERAND_REG | OPERAND_IMM;
        inst->imm = 0;
    }

    // push imm32 -> sub rsp, 8 ; mov [rsp], imm32
    

    // xchg reg, reg -> xor swap trick
   

    // Optional: operand flip (only if both are regs)
    if ((inst->operand_type == (OPERAND_REG | OPERAND_REG)) && CHANCE(PERC)) {
        uint8_t tmp = inst->op1;
        inst->op1 = inst->op2;
        inst->op2 = tmp;
        
        // Swap REX.R and REX.B bits if both are extended registers
        if (inst->rex && (inst->op1 >= 8 || inst->op2 >= 8)) {
            uint8_t rex_r = (inst->rex & 0x04); // Extract REX.R
            uint8_t rex_b = (inst->rex & 0x01); // Extract REX.B
            
            // Swap REX.R and REX.B bits
            inst->rex = (inst->rex & 0xFA) | (rex_b << 2) | (rex_r >> 2);
        }
    }
}

int mutate_multi(const struct Instruction *input, struct Instruction *out_list, int max_count) {
    if (max_count < 3) return 0;    // We may need up to 3 slots

    // push imm32 => sub rsp, 8; mov [rsp], imm32
    if (input->opcode == 0x68 && input->operand_type == OPERAND_IMM && CHANCE(PERC)) {
        // sub rsp, 8
        struct Instruction sub_inst;
        memset(&sub_inst, 0, sizeof(sub_inst));
        sub_inst.opcode = 0x2D;     // sub reg, imm32
        sub_inst.operand_type = OPERAND_REG | OPERAND_IMM;
        sub_inst.op1 = RSP_REG;     // rsp
        sub_inst.imm = 8;           // 8 bytes
        sub_inst.rex = 0x48;

        // mov [rsp], imm32
        struct Instruction mov_inst;
        memset(&mov_inst, 0, sizeof(mov_inst));
        mov_inst.opcode = 0xC7;         // IS mov [reg], imm32 HANDLED BY ENCODE/DECODE????? IF NOT ADD THIS BEFORE YOU RUN
        mov_inst.operand_type = OPERAND_MEM | OPERAND_IMM;
        mov_inst.op1 = RSP_REG;         // rsp
        mov_inst.imm = input->imm;
        mov_inst.rex = 0x48;

        out_list[0] = sub_inst;
        out_list[1] = mov_inst;
        return 2;
    }

    // xchg reg, reg -> xor swap trick
    if (input->opcode == 0x87 && input->operand_type == (OPERAND_REG | OPERAND_REG) && CHANCE(PERC)) {
        struct Instruction xor1, xor2, xor3;
        memset(&xor1, 0, sizeof(xor1));
        memset(&xor2, 0, sizeof(xor2));
        memset(&xor3, 0, sizeof(xor3));

        xor1.opcode = xor2.opcode = xor3.opcode = 0x31;     // xor reg, reg 
        xor1.operand_type = xor2.operand_type = xor3.operand_type = OPERAND_REG | OPERAND_REG;

        xor1.op1 = input->op1;
        xor1.op2 = input->op2;

        xor2.op1 = input->op2;
        xor2.op2 = input->op1;
        
        xor3.op1 = input->op1;
        xor3.op2 = input->op2;

        xor1.rex = xor2.rex = xor3.rex = 0x48;

        out_list[0] = xor1;
        out_list[1] = xor2;
        out_list[2] = xor3;

        return 3;

    }

    // Unsupported/Invalid Instruction
    return 0;   
}