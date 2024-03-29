#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "virtual_machine.h"
#include "memory.c"
#include "util.c"
#include "instruction.c"

/* #define DEBUG_PRINT_INSTRUCTIONS */
/* #define DEBUG_PRINT_INSTRUCTION_FILE */

typedef struct {
    byte data[64];
} MemoryBank;

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("Error, no input file\n");
        return 1;
    }

    byte should_terminate = 0;
    u32 program_counter = 0;
    u32 R[REG_COUNT];

    // 3 bits per register type == 24 bytes
    byte RT[32];
    memzero(R, sizeof(u32) * 32);
    memzero(RT, 32);

    byte* memory = malloc(TOTAL_MEM_SIZE);

    u32*  instruction_memory  = (u32*) &memory[0];
    byte* data_memory         = &memory[INSTRUCTION_MEMORY_SIZE];
    byte avail_blocks[128];
    memzero(avail_blocks, 128);

    memzero(memory, TOTAL_MEM_SIZE);

    // Scope to avoid using input_file pointer again
    {
        FILE* input_file = fopen(argv[1], "r");
        if (input_file == NULL) {
            printf("Failed to open file: %s\n", argv[1]);
            free(memory);
            return 1;
        }

        unsigned int bytes_read_length = 0;

        bytes_read_length  = fread(instruction_memory, 1, INSTRUCTION_MEMORY_SIZE, input_file);
        bytes_read_length += fread(data_memory, 1, DATA_MEMORY_SIZE, input_file);

        fclose(input_file);
        if (bytes_read_length != INSTRUCTION_MEMORY_SIZE + DATA_MEMORY_SIZE) {
            printf("Binary blob in invalid format, expected 2048 bytes got %i.\n", bytes_read_length);
            free(memory);
            return 1;
        }
    }

#ifdef DEBUG_PRINT_INSTRUCTION_FILE
    printf("PC\tCode\t\tName\tRd\tR1\tR2\tImm\n");
    Instruction in;

    do {
        u32 instruction_data = instruction_memory[program_counter / 4];
        VirtualInstructionName virt_instruction = VIRTUAL_NONE;
        byte failed_memory = 0;
        byte jumped = 0;

        in = instruction_decode(instruction_data);
        if (in.name == INSTRUCTION_INVALID)
            break;

        printf("%x:\t", program_counter);
        print_int_as_hex_string(instruction_data);
        putchar('\t');
        instruction_print_summary(in, stdout);
        program_counter += 4;
    }
    while (in.name != INSTRUCTION_INVALID);
    free(memory);
    exit(0);
#endif

#ifdef DEBUG_PRINT_INSTRUCTIONS
    printf("PC\tCode\t\tName\tRd\tR1\tR2\tImm\n");
#endif

        u64 executed_instructions_count = 0;
    while(program_counter < INSTRUCTION_MEMORY_SIZE) {
        Instruction in;

        u32 instruction_data = instruction_memory[program_counter / 4];
        VirtualInstructionName virt_instruction = VIRTUAL_NONE;
        byte failed_memory = 0;
        byte jumped = 0;

        in = instruction_decode(instruction_data);

#ifdef DEBUG_PRINT_INSTRUCTIONS
        printf("%x:\t", program_counter);
        print_int_as_hex_string(instruction_data);
        putchar('\t');
        instruction_print_summary(in, stdout);
#endif


        switch (in.name) {
            case INSTRUCTION_COUNT: break;
            case INSTRUCTION_INVALID:
                printf("Instruction Not Implemented: ");
                print_int_as_hex_string(instruction_data);
                printf("\n");
                should_terminate = 1;

                break;
            case ADD:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) + R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case ADDI:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) + in.imm;
                break;
            case SUB:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) - R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case LUI:
                R[in.rd] = (in.imm >> 12);
                break;
            case XOR:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) ^ R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case XORI:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) ^ in.imm;
                break;
            case OR:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) | R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case ORI:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) | in.imm;
                break;
            case AND:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) & R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case ANDI:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) & in.imm;
                break;

            case SLL:
                R[in.rd] = (u32)R[in.rs1] << (u32)R[in.rs2];
                break;
            case SRL:
                R[in.rd] = (u32)R[in.rs1] >> (u32)R[in.rs2];
                break;
            case SRA:
                R[in.rd] = (R[in.rs1] >> R[in.rs2]) | (R[in.rs1] << (32 - R[in.rs2]));
                break;
            case LB:
                {
                char val = 0;
                failed_memory &= get_mem_char(memory, avail_blocks, R[in.rs1] + in.imm, &val);
                R[in.rd] = val;
                /* RT[in.rd] = R_I8_FLAG; */
                }
                break;
            case LH:
                {
                i16 val = 0;
                failed_memory &= get_mem_i16(memory, avail_blocks, R[in.rs1] + in.imm, &val);
                R[in.rd] = val;
                /* RT[in.rd] = R_I16_FLAG; */
                }
                break;
            case LW:
                {
                i32 val = 0;

                failed_memory &= get_mem_i32(memory, avail_blocks, R[in.rs1] + in.imm, &val);
                R[in.rd] = val;
                /* RT[in.rd] = R_I32_FLAG; */
                }
                break;
            case LBU:
                {
                byte val = 0;
                failed_memory &= get_mem_byte(memory, avail_blocks, R[in.rs1] + in.imm, &val);
                R[in.rd] = val;
                /* RT[in.rd] = R_U8_FLAG; */
                }
                break;
            case LHU:
                {
                u16 val = 0;
                failed_memory &= get_mem_u16(memory, avail_blocks, R[in.rs1] + in.imm, &val);
                R[in.rd] = val;
                /* RT[in.rd] = R_U16_FLAG; */
                }
                break;
            case SB:
                {
                failed_memory &= set_mem_byte(memory, avail_blocks, R[in.rs1] + in.imm, R[in.rs2], &virt_instruction);
                }
                break;
            case SH:
                failed_memory &= set_mem_u16( memory, avail_blocks, R[in.rs1] + in.imm, R[in.rs2], &virt_instruction);
                break;
            case SW:
                failed_memory &= set_mem_u32( memory, avail_blocks, R[in.rs1] + in.imm, R[in.rs2], &virt_instruction);
                break;
            case SLT:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) < R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case SLTI:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) < (i32)in.imm;
                break;
            case SLTU:
                R[in.rd] = (unsigned) R_CAST(R[in.rs1], RT[in.rs1]) < (unsigned) R_CAST(R[in.rs2], RT[in.rs2]);
                break;
            case SLTIU:
                R[in.rd] = R_CAST(R[in.rs1], RT[in.rs1]) < (i32)in.imm;
                break;
            case BEQ:
                if (R[in.rs1] == R[in.rs2]) {
                    program_counter += in.imm;
                    jumped = 1;
                }
                break;
            case BNE:
                if (R[in.rs1] != R[in.rs2]) {
                    program_counter += in.imm;
                    jumped = 1;
                }
                break;
            case BLT:
                if ((signed)R[in.rs1] < (signed)R[in.rs2]) {
                    program_counter += in.imm;
                    jumped = 1;
                }
                break;
            case BLTU:
                if ((unsigned)R[in.rs1] < (unsigned)R[in.rs2]) {
                    program_counter += in.imm;
                    jumped = 1;
                }
                break;
            case BGE:
                if ((signed)R[in.rs1] >= (signed)R[in.rs2]) {
                    program_counter += in.imm;
                    jumped = 1;
                }
                break;
            case BGEU:
                if ((unsigned)R[in.rs1] >= (unsigned)R[in.rs2]) {
                    program_counter += in.imm;
                    jumped = 1;
                }
                break;
            case JAL:
                R[in.rd] = program_counter + 4;
                program_counter += in.imm;
                jumped = 1;
                break;
            case JALR:
                R[in.rd] = program_counter + 4;
                program_counter = R[in.rs1] + in.imm;
                jumped = 1;
                break;
        }


        if (should_terminate || failed_memory)
            break;

        /* if (executed_instructions_count >= 10) */
        /*     break; */

        if (virt_instruction != VIRTUAL_NONE) {
            switch(virt_instruction) {
                case VIRTUAL_NONE: break;
                case VIRTUAL_COUNT: break;
                case VIRTUAL_INVALID: break;
                case VIRTUAL_PRINT_CHAR:
                    putchar((byte)memory[0x800]);
                    break;
                case VIRTUAL_PRINT_SINT:
                    printf("%i", *((i32*)&memory[0x804]));
                    break;
                case VIRTUAL_PRINT_UINT:
                    printf("%x", *((u32*)&memory[0x808]));
                    break;
                case VIRTUAL_HALT:
                    printf("CPU Halt Requested\n");
                    free(memory);
                    exit(0);
                    break;
                case VIRTUAL_READ_CHARACTER:
                    // implemented in memory.c
                    break;
                case VIRTUAL_READ_SINT:
                    // implemented in memory.c
                    break;
                case VIRTUAL_DUMP_PC:
                    printf("%x\n", program_counter);
                    break;
                case VIRTUAL_DUMP_REG:
                    register_dump(program_counter, R);
                    break;
                case VIRTUAL_DUMP_MEM_W:
                    printf("%x\n", *((u32*)&memory[0x828]));
                    break;
                case VIRTUAL_HEAP_ALLOC:
                    {
                    u32 size = *((u32*)&memory[0x830]);
                    u32 min_blocks = (size / 64) + 1;
                    byte found_block = 0;
                
                    if (min_blocks > MAX_BANKS) {
                        printf("Ran out of dynamic memory!");
                        free(memory);
                        exit(0);
                    }

                    for (u32 i = 0; i < MAX_BANKS; i++) {
                        u32 free_count = 0;
                        for (u32 j = i; j < min_blocks; j++) {
                            if (avail_blocks[j] == 0) {
                                free_count += 1;
                            }
                        }

                        if (free_count >= min_blocks) {
                            found_block = 1;
                            for (u32 j = i; j < min_blocks; j++) {
                                if (j == i) {
                                    R[28] = 0xb700 + j * 64;
                                }

                                avail_blocks[j] = 1;
                            }
                            break;
                        }
                    }

                    if (!found_block) {
                        printf("Ran out of dynamic memory!");
                        free(memory);
                        exit(0);
                    }
                    
                    }
                    break;
                case VIRTUAL_HEAP_FREE:
                    {
                        i32 address = *((u32*)&memory[0x834]);
                        i32 block = (address - (NON_DYNAMIC_SIZE)) / 64;
                        byte found_block = 0;

                        if (address >= NON_DYNAMIC_SIZE && address < TOTAL_MEM_SIZE) {
                            if (address % 4 == 0 || address % 64 == 0) {
                                if (avail_blocks[block] == 1) {
                                    avail_blocks[block] = 0;
                                    found_block = 1;
                                }
                            }

                        }

                        if (!found_block) {
                            printf("Illegal Operation: ");
                            print_int_as_hex_string(instruction_data);
                            printf("\n");
                            register_dump(program_counter, R);
                            free(memory);
                            exit(0);
                        }
                    }

                    break;
            }
        }

        R[0] = 0;
        executed_instructions_count = executed_instructions_count + 1;

        if (!jumped) {
            program_counter += 4;
        }
    }

    register_dump(program_counter, R);
    // OS will do it anyways, but if it counts as a "memory leak" I get a ZERO
    free(memory);
    return 0;
}
