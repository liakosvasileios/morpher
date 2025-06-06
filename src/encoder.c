#include <string.h>
#include <stdio.h>
#include "engine.h"
#include "isa.h"

int encode_instruction(const struct Instruction *inst, uint8_t *out) {
    int offset = 0;

    if (inst->rex) {
        EMIT(inst->rex);
    } else if ((inst->op1 >= 8 || inst->op2 >= 8) || (inst->opcode == OPCODE_MOV_REG_IMM64 || inst->opcode == OPCODE_ADD_RAX_IMM32)) {
        uint8_t rex = 0x48;
        if (inst->op1 >= 8) rex |= 0x01;
        if (inst->op2 >= 8) rex |= 0x04;
        EMIT(rex);
    }

    switch (inst->opcode) {
        case OPCODE_MOV_REG_IMM64:
            EMIT(0xB8 + (inst->op1 & 0x07));
            memcpy(&out[offset], &inst->imm, 8);
            offset += 8;
            return offset;

        case OPCODE_MOV_MEM_REG:
            EMIT(0x89);
            EMIT(ENCODE_MODRM(inst->op1, inst->op2));
            return offset;

        case OPCODE_MOV_REG_MEM:
            EMIT(0x8B);
            EMIT(ENCODE_MODRM(inst->op2, inst->op1));
            return offset;

        case OPCODE_ADD_RAX_IMM32:
            EMIT(0x05);
            memcpy(&out[offset], &inst->imm, 4);
            offset += 4;
            return offset;

        case OPCODE_SUB_RAX_IMM32:
            EMIT(0x2D);
            memcpy(&out[offset], &inst->imm, 4);
            offset += 4;
            return offset;

        case OPCODE_XOR_REG_REG:
            EMIT(0x31);
            EMIT(ENCODE_MODRM(inst->op1, inst->op2));
            return offset;

        case OPCODE_PUSH_IMM32:
            EMIT(0x68);
            memcpy(&out[offset], &inst->imm, 4);
            offset += 4;
            return offset;

        case OPCODE_XCHG_REG_REG:
            EMIT(0x87);
            EMIT(ENCODE_MODRM(inst->op1, inst->op2));
            return offset;

        case OPCODE_MOV_MEM_IMM32:
            EMIT(0xC7);
            EMIT(0x04);
            EMIT(0x24);
            memcpy(&out[offset], &inst->imm, 4);
            offset += 4;
            return offset;

        case OPCODE_XOR_REG_IMM32:
            EMIT(0x81);
            EMIT(ENCODE_MODRM(inst->op1, 6));
            memcpy(&out[offset], &inst->imm, 4);
            offset += 4;
            return offset;

        case OPCODE_ADD_REG_REG:
            EMIT(0x01);
            EMIT(ENCODE_MODRM(inst->op1, inst->op2));
            return offset;

        case OPCODE_SUB_REG_REG:
            EMIT(0x29);
            EMIT(ENCODE_MODRM(inst->op1, inst->op2));
            return offset;

        case OPCODE_TEST_REG8_REG8:
            EMIT(0x84);
            EMIT(ENCODE_MODRM(inst->op1, inst->op2));
            return offset;

        // Jcc 
        default:
            if ((inst->opcode & 0xFF00) == 0x0F00) {
                uint8_t ext = inst->opcode & 0xFF;
                if (ext >= 0x90 && ext <= 0x9F && inst->operand_type == OPERAND_REG) {
                    EMIT(0x0F);
                    EMIT(ext);
                    EMIT(0xC0 | (inst->op1 & 0x07));
                    return offset;
                } else if (ext >= 0x80 && ext <= 0x8F && inst->operand_type == OPERAND_IMM) {
                    EMIT(0x0F);
                    EMIT(ext);
                    memcpy(&out[offset], &inst->imm, 4);
                    offset += 4;
                    return offset;
                }
            }
            if (inst->opcode >= OPCODE_JCC_SHORT_MIN && inst->opcode <= OPCODE_JCC_SHORT_MAX && inst->operand_type == OPERAND_IMM) {
                EMIT(inst->opcode);
                EMIT((int8_t)inst->imm);
                return offset;
            }
            return -1;
    }
}
