/*
CS-UY 2214
Adapted from Jeff Epstein
Starter code for E20 simulator
sim.cpp
*/


#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <regex>
#include <cstdlib>


using namespace std;


// Some helpful constant values that we'll be using.
size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;


/*
    Loads an E20 machine code file into the list
    provided by mem. We assume that mem is
    large enough to hold the values in the machine
    code file.


    @param f Open file to read from
    @param mem Array represetnting memory into which to read program
*/
void load_machine_code(ifstream &f, unsigned mem[])
{
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;
    while (getline(f, line))
    {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re))
        {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr)
        {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE)
        {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr++;
        mem[addr] = instr;
    }
}


/*
    Prints the current state of the simulator, including
    the current program counter, the current register values,
    and the first memquantity elements of memory.


    @param pc The final value of the program counter
    @param regs Final value of all registers
    @param memory Final value of memory
    @param memquantity How many words of memory to dump
*/
void print_state(unsigned pc, unsigned regs[], unsigned memory[], size_t memquantity)
{
    cout << setfill(' ');
    cout << "Final state:" << endl;
    cout << "\tpc=" << setw(5) << pc << endl;


    for (size_t reg = 0; reg < NUM_REGS; reg++)
        cout << "\t$" << reg << "=" << setw(5) << regs[reg] << endl;


    cout << setfill('0');
    bool cr = false;
    for (size_t count = 0; count < memquantity; count++)
    {
        cout << hex << setw(4) << memory[count] << " ";
        cr = true;
        if (count % 8 == 7)
        {
            cout << endl;
            cr = false;
        }
    }
    if (cr)
        cout << endl;
}

uint16_t fix_bit_length(uint16_t val) {
    return val = val & 0b1111111111111111;
}

uint16_t fix_bit_length13(uint16_t val) {
    return val = val & 0b1111111111111;
}

uint16_t convert_neg_imm(uint16_t val) {
    val = (~val & 0b0000000001111111) + 0b1;
    return val;
}

uint16_t sign_extend_imm7(uint16_t val) {
    if (val & 0b1000000) {
        return val |= 0b1111111111000000;
    }
    else {
        return val;
    }
}


/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char* argv[])
{
    /*
        Parse the command-line arguments
    */
    char* filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    for (int i = 1; i < argc; i++)
    {
        string arg(argv[i]);
        if (arg.rfind("-", 0) == 0)
        {
            if (arg == "-h" || arg == "--help") {
                do_help = true;
            }
            else {
                arg_error = true;
            }
        }
        else
        {
            if (filename == nullptr) {
                filename = argv[i];
            }
            else {
                arg_error = true;
            }
        }
    }
    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr)
    {
        cerr << "usage " << argv[0] << " [-h] filename" << endl << endl;
        cerr << "Simulate E20 machine" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl << endl;
        cerr << "optional arguments:" << endl;
        cerr << "  -h, --help  show this help message and exit" << endl;
        return 1;
    }
    
    ifstream f(filename);
    if (!f.is_open())
    {
        cerr << "Can't open file " << filename << endl;
        return 1;
    }
    // TODO: your code here. Load f and parse using load_machine_code
    unsigned memory[MEM_SIZE] = { 0 };
    unsigned regs[NUM_REGS] = { 0 };
    unsigned pc = 0b0000000000000000;
    bool stop = false;
    load_machine_code(f, memory);
    f.close();
    // TODO: your code here. Do simulation.
    uint16_t op = 0b0000000000000000;
    uint16_t imm = 0b0000000000000000;
    uint16_t regA = 0b0000000000000000;
    uint16_t regB = 0b0000000000000000;
    uint16_t regDst = 0b0000000000000000;
    while (stop == false)
    {
        //get the instruction bits and shift to the right 13 to obtain the opcode
        uint16_t instruction = memory[pc];
        op = (instruction & 0b1110000000000000) >> 13;
        //3 registers instructions
        if (op == 0)
        {
            //get the immediate value, regA, regB, and regDst from the instruction
            imm = instruction & 0b0000000000001111;
            regA = (instruction & 0b0001110000000000) >> 10;
            regB = (instruction & 0b0000001110000000) >> 7;
            regDst = (instruction & 0b0000000001110000) >> 4;
            //add
            if (imm == 0)
            {
                //add two registers and store value into destination
                regs[regDst] = regs[regA] + regs[regB];
                pc += 1;
            }
            //sub
            else if (imm == 1)
            {
                //subtract two registers and store value into destination
                regs[regDst] = regs[regA] - regs[regB];
                pc += 1;
            }
            //or
            else if (imm == 2)
            {
                //or two registers and store value in desination
                regs[regDst] = regs[regA] | regs[regB];
                pc += 1;
            }
            //and
            else if (imm == 3)
            {
                //and two registers and store value in destination
                regs[regDst] = regs[regA] & regs[regB];
                pc += 1;
            }
            //slt
            else if (imm == 4)
            {
                //set regDst to 1 if regA < regB and 0 otherwise
                if (regs[regA] < regs[regB])
                    {regs[regDst] = 1;}
                else
                    {regs[regDst] = 0;}
                pc += 1;
            }
            //jr
            else if (imm == 8)
            {
                //if the immediate value is the same as the pc then it will result in a infinite loop so program is stopped if true
                //and jumps to value in regA otherwise
                if (regs[regA] == pc)
                {
                    stop = true;
                }
                else
                {
                    pc = regs[regA];
                }
            }
        }
        //0 register instructions
        else if (op == 2 || op == 3)
        {
            //setup immmediate value from instructions
            imm = instruction & 0b0001111111111111;
            //j
            if (op == 2)
            {
                // if immediate value is equal to pc it results in infinite loop so program is stopped and jumps to imm otherwise
                if (imm == pc)
                {
                    stop = true;
                }
                else
                {
                    pc = imm;
                }
            }
            //jal
            else if (op == 3)
            {
                //sets value of register 7 to current memory address + 1 and jumps to immediate value
                //if it does not result in an infinite loop
                regs[7] = pc + 1;
                if (imm == pc)
                {
                    stop = true;
                }
                else
                {
                    pc = imm;
                }
            }
        }
        //2 registers instructions
        else
        {
            //setup imm, and register numbers from instruction
            imm = instruction & 0b0000000001111111;
            regA = (instruction & 0b0001110000000000) >> 10;
            regB = (instruction & 0b0000001110000000) >> 7;
            //sign extends immediate value for calculations
            uint16_t ext_imm = sign_extend_imm7(imm);
            //addi
            if (op == 1)
            {
                //adds source reg and imm into destination
                regs[regB] = regs[regA] + ext_imm;
                regs[regB] = fix_bit_length(regs[regB]);
                pc += 1;
            }
            //lw
            else if (op == 4)
            {
                //stores the value at the memory location at immediate plus addr reg into destination
                regs[regB] = memory[fix_bit_length13(ext_imm + regs[regA])];
                pc += 1;
            }
            //sw 
            else if (op == 5)
            {
                //stores value in source register into memory cell located from sum of immediate and value at addr register
                memory[fix_bit_length13(ext_imm + regs[regA])] = regs[regB];
                pc += 1;
            }
            //jeq
            else if (op == 6)
            {
                //checks if two registers are equal and jumps if true, increments by one otherwise
                if (regs[regA] == regs[regB])
                {
                    if ((pc + ext_imm + 0b1) % 128 == pc)
                    {
                        stop = true;
                    }
                    else
                    {
                        pc = (pc + ext_imm + 0b1)%128;
                    }
                }
                else {
                    pc += 1;
                }
                
            }
            //slti
            else if (op == 7)
            {
                //checks if register value is less than immediate value and set destination to 1 or 0 accordingly
                regs[regB] = (regs[regA] < ext_imm) ? 1:0;
                pc += 1;
            }
        }
        
        //ensure pc is within range
        pc &= 0b1111111111111;

        //ensure all register values are greater than 0 and less than reg_size, and wraparound if otherwise
        for (unsigned i : regs) {
            i &= 0b1111111111111111;
        }

        //make sure register 0 is 0
        regs[0] = 0b0000000000000000;
    }
    // TODO: your code here. print the final state of the simulator before ending, using print_state
    print_state(pc, regs, memory, 128);
    return 0;
}

//ra0Eequ6ucie6Jei0koh6phishohm9