/*
CS-UY 2214
Adapted from Jeff Epstein
Starter code for E20 cache Simulator
simcache.cpp
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
    Prints out the correctly-formatted configuration of a cache.

    @param cache_name The name of the cache. "L1" or "L2"

    @param size The total size of the cache, measured in memory cells.
        Excludes metadata

    @param assoc The associativity of the cache. One of [1,2,4,8,16]

    @param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])

    @param num_rows The number of rows in the given cache.
*/
void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows) {
    cout << "Cache " << cache_name << " has size " << size <<
        ", associativity " << assoc << ", blocksize " << blocksize <<
        ", rows " << num_rows << endl;
}

/*
    Prints out a correctly-formatted log entry.

    @param cache_name The name of the cache where the event
        occurred. "L1" or "L2"

    @param status The kind of cache event. "SW", "HIT", or
        "MISS"

    @param pc The program counter of the memory
        access instruction

    @param addr The memory address being accessed.

    @param row The cache row or set number where the data
        is stored.
*/
void print_log_entry(const string &cache_name, const string &status, int pc, int addr, int row) {
    cout << left << setw(8) << cache_name + " " + status <<  right <<
        " pc:" << setw(5) << pc <<
        "\taddr:" << setw(5) << addr <<
        "\trow:" << setw(4) << row << endl;
}

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

//setup node class for doubly linked list
class Node {
    public:
        Node(int data) : index(data) {
            prev = nullptr;
            next = nullptr;
        }

        void setNext(Node* node) {next = node;}

        Node* getNext() {return next;}

        void setPrev(Node* node) {prev = node;}

        Node* getPrev() {return prev;}

        int getIndex() {return index;}

    private:
        int index;
        Node* prev;
        Node* next;

};

class Cache {
    private:
        class Row {
            public:
                Row(const int associativity, const int blocksize) : size(associativity) {
                    //initialize the cache to hold all 0's according to associativity and blocksize
                    for (int i=0; i < size; ++i) {
                        vector<uint16_t> temp;
                        for (int j=0; j < blocksize; ++j) {
                            temp.push_back(0);
                        }
                    values.push_back(temp);
                    tags.push_back(0);
                    valid.push_back(0);
                    }

                    //Set up doubly linked list for LRU
                    //create head and tail node and assign to row
                    Node* head = new Node(-1);
                    Node* tail = new Node(-1);
                    head_node = head;
                    tail_node = tail;
                    Node* temp_prev = head;
                    Node* temp;
                    //add nodes with the index to LRU associativity
                    for (int i=0; i < size; ++i) {
                        temp = new Node(i);
                        temp_prev->setNext(temp);
                        temp->setPrev(temp_prev);
                        temp_prev = temp;
                        if (i == size-1) {
                            temp->setNext(tail);
                            tail->setPrev(temp);
                        }
                    }
                }

                //get the LRU index and set that node to the end and return the index
                int getLRU() {
                    Node* LRU = head_node->getNext();
                    Node* new_recent = LRU->getNext();
                    
                    //set node after LRU to new LRU
                    head_node->setNext(new_recent);
                    new_recent->setPrev(head_node);

                    Node* prev_old = tail_node->getPrev();
                    //put LRU at the end
                    prev_old->setNext(LRU);
                    LRU->setPrev(prev_old);

                    tail_node->setPrev(LRU);
                    LRU->setNext(tail_node);
                    return LRU->getIndex();
                }

                //move the index used to the end of the LRU
                void pushToTail(int index) {
                    Node* push = head_node->getNext();
                    Node* before;
                    Node* after;
                    Node* prev_tail;
                    Node* temp;
                    for (int i=0; i < size; ++i) {
                        temp = push->getNext();
                        //check if the current node holds the index and move it to back if it does
                        if (push->getIndex() == index) {
                            //connect nodes before and after the node that needs to be pushed
                            before = push->getPrev();
                            after = push->getNext();
                            before->setNext(after);
                            after->setPrev(before);

                            //connect the push node to the end
                            prev_tail = tail_node->getPrev();
                            prev_tail->setNext(push);
                            push->setPrev(prev_tail);
                            push->setNext(tail_node);
                            tail_node->setPrev(push);
                        }
                        push = temp;
                    }
                }

                //return the index of the associativity in the row that the tag is in
                int inTags(int tag) {
                    for (int i=0; i < size; ++i) {
                        if (tags[i] == tag && valid[i] == 1) {
                            return i;
                        }
                    }
                    return -1;
                }

                uint16_t getVal(int assoc, int index) {
                    return values[assoc][index];
                }

                vector<uint16_t> getBlock(int assoc) {
                    return values[assoc];
                }

                void setRow(int assoc, int tag, vector<uint16_t> vals) {
                    valid[assoc] = 1;
                    tags[assoc] = tag;
                    values[assoc] = vals;
                }

                void setRowVal(int assoc, int index, uint16_t val) {
                    values[assoc][index] = val;
                }
            
            private:
                //variables for Row class
                const int size;
                vector<int> valid;
                vector<int> tags;
                vector<vector<uint16_t>> values;
                Node* head_node;
                Node* tail_node;
        };
        
    public:
        Cache(const int Size, const int associativity, const int BlockSize, const string Name) :
        total_size(Size), blocksize(BlockSize), name(Name)
        {
            //initialize the rows for the cache
            for (int i=0; i < (total_size/(associativity*blocksize)); ++i) {
                rows.push_back(new Row(associativity, blocksize));
            }
            print_cache_config(name, total_size, associativity, blocksize, rows.size());
        }
        //variabels for Cache
        int total_size;
        int blocksize;
        vector<Row*> rows;
        string name;


        //return val from cache or memory for single cache
        uint16_t getVal(uint16_t addr, unsigned mem[], unsigned pc) {
            //obtain relevant numbers
            int blockid = addr/blocksize;
            int row = blockid % rows.size();
            int tag = blockid / rows.size();
            int assoc = rows[row]->inTags(tag);
            int index = addr % blocksize;
            
            if (assoc != -1) {
                //if hit then move the accessed associativity to the end of LRU
                //and return val
                rows[row]->pushToTail(assoc);
                print_log_entry(name, "HIT", pc, addr, row);
                return rows[row]->getVal(assoc, index);
            }
            else {
                //if miss then write block from memory into row and return val
                vector<uint16_t> vals;
                for (int i = blockid*blocksize; i < ((blockid+1)*blocksize); ++i) {vals.push_back(mem[i]);}
                int LRU = rows[row]->getLRU();
                rows[row]->setRow(LRU, tag, vals);
                print_log_entry(name, "MISS", pc, addr, row);
                return rows[row]->getVal(LRU, index);
            }
        }

        //return val from cache or memory for double cache
        uint16_t doubleCacheGetVal(uint16_t addr, unsigned mem[], Cache L2, unsigned pc) {
            //obtain relevant numbers for both caches
            int L1blockid = addr/blocksize;
            int L1row = L1blockid % rows.size();
            int L1tag = L1blockid / rows.size();
            int L1index = addr % blocksize;
            int assoc = rows[L1row]->inTags(L1tag);

            int L2blockid = addr/L2.blocksize;
            int L2row = L2blockid % L2.rows.size();
            int L2tag = L2blockid / L2.rows.size();
            int L2index = addr % L2.blocksize;
            int L2assoc = L2.rows[L2row]->inTags(L2tag);

            if (assoc != -1) {
                //if hit in L1 then push L1 associativity to end of LRU
                //print log and return val
                rows[L1row]->pushToTail(assoc);
                print_log_entry(name, "HIT", pc, addr, L1row);
                return rows[L1row]->getVal(assoc, L1index);
            }
            else if (assoc == -1 && L2assoc != -1) {
                //if miss L1 hit L2
                //move vals from L2 into L1 and move L2 associativity to end of LRU
                vector<uint16_t> vals;
                for (int i = L1blockid*blocksize; i < ((L1blockid+1)*blocksize); ++i) {vals.push_back(mem[i]);}
                L2.rows[L2row]->pushToTail(L2assoc);
                //write vals into L1
                int LRU = rows[L1row]->getLRU();
                rows[L1row]->setRow(LRU, L1tag, vals);
                //print log and return val
                print_log_entry(name, "MISS", pc, addr, L1row);
                print_log_entry(L2.name, "HIT", pc, addr, L2row);
                return rows[L1row]->getVal(LRU, L1index);
            }
            else if (assoc == -1 && L2assoc == -1) {
                //if both caches miss copy data from memory into both caches
                vector<uint16_t> L1vals;
                vector<uint16_t> L2vals;
                for (int i = L1blockid*blocksize; i < ((L1blockid+1)*blocksize); ++i) {L1vals.push_back(mem[i]);}
                for (int i = L2blockid*L2.blocksize; i < ((L2blockid+1)*L2.blocksize); ++i) {L2vals.push_back(mem[i]);}
                //get LRU and put data into row for L1 and L2
                int L2_LRU = L2.rows[L2row]->getLRU();
                L2.rows[L2row]->setRow(L2_LRU, L2tag, L2vals);
                int LRU = rows[L1row]->getLRU();
                rows[L1row]->setRow(LRU, L1tag, L1vals);
                //print log and return val
                print_log_entry(name, "MISS", pc, addr, L1row);
                print_log_entry(L2.name, "MISS", pc, addr, L2row);
                return rows[L1row]->getVal(LRU, L1index);
            }
            return 0;
        }

        //takes val and writes it into the cache and memory at given address for single cache
        void setVal(uint16_t addr, uint16_t val, unsigned mem[], unsigned pc) {
            //obtain relevant numbers
            int blockid = addr/blocksize;
            int row = blockid % rows.size();
            int tag = blockid / rows.size();
            int index = addr % blocksize;
            int assoc = rows[row]->inTags(tag);

            if (assoc != -1) {
                //if hit, then change val in cache and move associativity to the end of LRU
                rows[row]->setRowVal(assoc, index, val);
                rows[row]->pushToTail(assoc);
            }
            else {
                //if miss, copy vals from memory into cache and write val to cache
                vector<uint16_t> vals;
                for (int i = blockid*blocksize; i < ((blockid+1)*blocksize); ++i) {vals.push_back(mem[i]);}
                int LRU = rows[row]->getLRU();
                rows[row]->setRow(LRU, tag, vals);
                rows[row]->setRowVal(LRU, index, val);
            }
            //write to memory and print log
            mem[addr] = val;
            print_log_entry(name, "SW", pc, addr, row);
        }

        //takes val and writes into cache and memory at given address for double cache
        void doubleCacheSetVal(uint16_t addr, uint16_t val, unsigned mem[], Cache L2, unsigned pc) {
            //setup relevant numbers
            int L1blockid = addr/blocksize;
            int L1row = L1blockid % rows.size();
            int L1tag = L1blockid / rows.size();
            int L1index = addr % blocksize;
            int assoc = rows[L1row]->inTags(L1tag);

            int L2blockid = addr/L2.blocksize;
            int L2row = L2blockid % L2.rows.size();
            int L2tag = L2blockid / L2.rows.size();
            int L2index = addr % L2.blocksize;
            int L2assoc = L2.rows[L2row]->inTags(L2tag);

            if (assoc != -1 && L2assoc != -1) {
                //if both hit write to caches and push both associativities to tail
                rows[L1row]->setRowVal(assoc, L1index, val);
                rows[L1row]->pushToTail(assoc);
                L2.rows[L2row]->setRowVal(L2assoc, L2index, val);
                L2.rows[L2row]->pushToTail(L2assoc);
            }
            else if (assoc == -1 && L2assoc != -1) {
                //if L1 miss and L2 hit, copy vals from L2 to L1
                vector<uint16_t> vals = L2.rows[L2row]->getBlock(L2assoc);
                L2.rows[L2row]->pushToTail(L2assoc);
                int LRU = rows[L1row]->getLRU();
                rows[L1row]->setRow(LRU, L1tag, vals);
                //write val to both caches
                rows[L1row]->setRowVal(assoc, L1index, val);
                L2.rows[L2row]->setRowVal(L2assoc, L2index, val);
            }
            else if (assoc != -1 && L2assoc == -1) {
                //if L1 hits and L2 misses, write to L1
                rows[L1row]->setRowVal(assoc, L1index, val);
                //copy vals from L1 to L2
                vector<uint16_t> vals = rows[L1row]->getBlock(assoc);
                rows[L1row]->pushToTail(assoc);
                int L2_LRU = L2.rows[L2row]->getLRU();
                L2.rows[L2row]->setRow(L2_LRU, L2tag, vals);
            }
            else if (assoc == -1 && L2assoc == -1) {
                //if both miss copy vals from memory to caches
                vector<uint16_t> L1vals;
                vector<uint16_t> L2vals;
                for (int i = L1blockid*blocksize; i < ((L1blockid+1)*blocksize); ++i) {L1vals.push_back(mem[i]);}
                for (int i = L2blockid*L2.blocksize; i < ((L2blockid+1)*L2.blocksize); ++i) {L2vals.push_back(mem[i]);}
                int L2_LRU = L2.rows[L2row]->getLRU();
                L2.rows[L2row]->setRow(L2_LRU, L2tag, L2vals);
                int LRU = rows[L1row]->getLRU();
                rows[L1row]->setRow(LRU, L1tag, L1vals);
                //write val to both caches
                rows[L1row]->setRowVal(LRU, L1index, val);
                L2.rows[L2row]->setRowVal(L2_LRU, L2index, val);
            }
            //write to memory and print log
            mem[addr] = val;
            print_log_entry(name, "SW", pc, addr, L1row);
            print_log_entry(L2.name, "SW", pc, addr, L2row);
        }
};







/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char *argv[]) {
    /*
        Parse the command-line arguments
    */
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    string cache_config;
    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-",0)==0) {
            if (arg== "-h" || arg == "--help")
                do_help = true;
            else if (arg=="--cache") {
                i++;
                if (i>=argc)
                    arg_error = true;
                else
                    cache_config = argv[i];
            }
            else
                arg_error = true;
        } else {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }
    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl << endl;
        cerr << "Simulate E20 cache" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
        cerr << "optional arguments:"<<endl;
        cerr << "  -h, --help  show this help message and exit"<<endl;
        cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one"<<endl;
        cerr << "                 cache) or"<<endl;
        cerr << "                 size,associativity,blocksize,size,associativity,blocksize"<<endl;
        cerr << "                 (for two caches)"<<endl;
        return 1;
    }

    ifstream f(filename);
    if (!f.is_open())
    {
        cerr << "Can't open file " << filename << endl;
        return 1;
    }

     //Setup Memory, registers, program counter, and stop
    unsigned memory[MEM_SIZE] = { 0 };
    unsigned regs[NUM_REGS] = { 0 };
    unsigned pc = 0b0000000000000000;
    bool stop = false;
    //load the machine code into memory
    load_machine_code(f, memory);
    f.close();
    // Setup registers to hold pieces of the instruction
    uint16_t op = 0b0000000000000000;
    uint16_t imm = 0b0000000000000000;
    uint16_t regA = 0b0000000000000000;
    uint16_t regB = 0b0000000000000000;
    uint16_t regDst = 0b0000000000000000;

    /* parse cache config */
    if (cache_config.size() > 0) {
        vector<int> parts;
        size_t pos;
        size_t lastpos = 0;
        while ((pos = cache_config.find(",", lastpos)) != string::npos) {
            parts.push_back(stoi(cache_config.substr(lastpos,pos)));
            lastpos = pos + 1;
        }
        parts.push_back(stoi(cache_config.substr(lastpos)));
        if (parts.size() == 3) {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            Cache L1(L1size, L1assoc, L1blocksize, "L1");
            while (stop == false) {
                //get the instruction bits and shift to the right 13 to obtain the opcode
                uint16_t instruction = memory[pc];
                op = (instruction & 0b1110000000000000) >> 13;
                //3 registers instructions
                if (op == 0) {
                    //get the immediate value, regA, regB, and regDst from the instruction
                    imm = instruction & 0b0000000000001111;
                    regA = (instruction & 0b0001110000000000) >> 10;
                    regB = (instruction & 0b0000001110000000) >> 7;
                    regDst = (instruction & 0b0000000001110000) >> 4;
                    //add
                    if (imm == 0) {
                        //add two registers and store value into destination
                        regs[regDst] = regs[regA] + regs[regB];
                        pc += 1;
                    }
                    //sub
                    else if (imm == 1) {
                        //subtract two registers and store value into destination
                        regs[regDst] = regs[regA] - regs[regB];
                        pc += 1;
                    }
                    //or
                    else if (imm == 2) {
                        //or two registers and store value in desination
                        regs[regDst] = regs[regA] | regs[regB];
                        pc += 1;
                    }
                    //and
                    else if (imm == 3) {
                        //and two registers and store value in destination
                        regs[regDst] = regs[regA] & regs[regB];
                        pc += 1;
                    }
                    //slt
                    else if (imm == 4) {
                        //set regDst to 1 if regA < regB and 0 otherwise
                        if (regs[regA] < regs[regB])
                            {regs[regDst] = 1;}
                        else
                            {regs[regDst] = 0;}
                        pc += 1;
                    }
                    //jr
                    else if (imm == 8) {
                        //if the immediate value is the same as the pc then it will result in a infinite loop so program is stopped if true
                        //and jumps to value in regA otherwise
                        if (regs[regA] == pc) {
                            stop = true;
                        }
                        else {
                            pc = regs[regA];
                        }
                    }
                }
                //0 register instructions
                else if (op == 2 || op == 3) {
                    //setup immmediate value from instructions
                    imm = instruction & 0b0001111111111111;
                    //j
                    if (op == 2) {
                        // if immediate value is equal to pc it results in infinite loop so program is stopped and jumps to imm otherwise
                        if (imm == pc) {
                            stop = true;
                        }
                        else {
                            pc = imm;
                        }
                    }
                    //jal
                    else if (op == 3) {
                        //sets value of register 7 to current memory address + 1 and jumps to immediate value
                        //if it does not result in an infinite loop
                        regs[7] = pc + 1;
                        if (imm == pc) {
                            stop = true;
                        }
                        else {
                            pc = imm;
                        }
                    }
                }
                //2 registers instructions
                else {
                    //setup imm, and register numbers from instruction
                    imm = instruction & 0b0000000001111111;
                    regA = (instruction & 0b0001110000000000) >> 10;
                    regB = (instruction & 0b0000001110000000) >> 7;
                    //sign extends immediate value for calculations
                    uint16_t ext_imm = sign_extend_imm7(imm);
                    //addi
                    if (op == 1) {
                        //adds source reg and imm into destination
                        regs[regB] = regs[regA] + ext_imm;
                        regs[regB] = fix_bit_length(regs[regB]);
                        pc += 1;
                    }
                    //lw
                    else if (op == 4) {
                        //stores the value at the memory location at immediate plus addr reg into destination
                        //regs[regB] = memory[fix_bit_length13(ext_imm + regs[regA])];
                        uint16_t addr = ext_imm + regs[regA];
                        regs[regB] = L1.getVal(addr, memory, pc);
                        pc += 1;
                    }
                    //sw 
                    else if (op == 5) {
                        //stores value in source register into memory cell located from sum of immediate and value at addr register
                        //memory[fix_bit_length13(ext_imm + regs[regA])] = regs[regB];
                        uint16_t addr = ext_imm + regs[regA];
                        L1.setVal(addr, regs[regB], memory, pc);
                        pc += 1;
                    }
                    //jeq
                    else if (op == 6) {
                        //checks if two registers are equal and jumps if true, increments by one otherwise
                        if (regs[regA] == regs[regB]) {
                            if ((pc + ext_imm + 0b1) % 128 == pc) {
                                stop = true;
                            }
                            else {
                                pc = (pc + ext_imm + 0b1)%128;
                            }
                        }
                        else {
                            pc += 1;
                        }
                        
                    }
                    //slti
                    else if (op == 7) {
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
            
            return 0;
                } else if (parts.size() == 6) {
                    int L1size = parts[0];
                    int L1assoc = parts[1];
                    int L1blocksize = parts[2];
                    int L2size = parts[3];
                    int L2assoc = parts[4];
                    int L2blocksize = parts[5];

                    //Initialize the two caches
                    Cache L1(L1size, L1assoc, L1blocksize, "L1");
                    Cache L2(L2size, L2assoc, L2blocksize, "L2");

                    while (stop == false) {
                        //get the instruction bits and shift to the right 13 to obtain the opcode
                        uint16_t instruction = memory[pc];
                        op = (instruction & 0b1110000000000000) >> 13;
                        //3 registers instructions
                        if (op == 0) {
                            //get the immediate value, regA, regB, and regDst from the instruction
                            imm = instruction & 0b0000000000001111;
                            regA = (instruction & 0b0001110000000000) >> 10;
                            regB = (instruction & 0b0000001110000000) >> 7;
                            regDst = (instruction & 0b0000000001110000) >> 4;
                            //add
                            if (imm == 0) {
                                //add two registers and store value into destination
                                regs[regDst] = regs[regA] + regs[regB];
                                pc += 1;
                            }
                            //sub
                            else if (imm == 1) {
                                //subtract two registers and store value into destination
                                regs[regDst] = regs[regA] - regs[regB];
                                pc += 1;
                            }
                            //or
                            else if (imm == 2) {
                                //or two registers and store value in desination
                                regs[regDst] = regs[regA] | regs[regB];
                                pc += 1;
                            }
                            //and
                            else if (imm == 3) {
                                //and two registers and store value in destination
                                regs[regDst] = regs[regA] & regs[regB];
                                pc += 1;
                            }
                            //slt
                            else if (imm == 4) {
                                //set regDst to 1 if regA < regB and 0 otherwise
                                if (regs[regA] < regs[regB])
                                    {regs[regDst] = 1;}
                                else
                                    {regs[regDst] = 0;}
                                pc += 1;
                            }
                            //jr
                            else if (imm == 8) {
                                //if the immediate value is the same as the pc then it will result in a infinite loop so program is stopped if true
                                //and jumps to value in regA otherwise
                                if (regs[regA] == pc) {
                                    stop = true;
                                }
                                else {
                                    pc = regs[regA];
                                }
                            }
                        }
                        //0 register instructions
                        else if (op == 2 || op == 3) {
                            //setup immmediate value from instructions
                            imm = instruction & 0b0001111111111111;
                            //j
                            if (op == 2) {
                                // if immediate value is equal to pc it results in infinite loop so program is stopped and jumps to imm otherwise
                                if (imm == pc) {
                                    stop = true;
                                }
                                else {
                                    pc = imm;
                                }
                            }
                            //jal
                            else if (op == 3) {
                                //sets value of register 7 to current memory address + 1 and jumps to immediate value
                                //if it does not result in an infinite loop
                                regs[7] = pc + 1;
                                if (imm == pc) {
                                    stop = true;
                                }
                                else {
                                    pc = imm;
                                }
                            }
                        }
                        //2 registers instructions
                        else {
                            //setup imm, and register numbers from instruction
                            imm = instruction & 0b0000000001111111;
                            regA = (instruction & 0b0001110000000000) >> 10;
                            regB = (instruction & 0b0000001110000000) >> 7;
                            //sign extends immediate value for calculations
                            uint16_t ext_imm = sign_extend_imm7(imm);
                            //addi
                            if (op == 1) {
                                //adds source reg and imm into destination
                                regs[regB] = regs[regA] + ext_imm;
                                regs[regB] = fix_bit_length(regs[regB]);
                                pc += 1;
                            }
                            //lw
                            else if (op == 4) {
                                //stores the value at the memory location at immediate plus addr reg into destination
                                //regs[regB] = memory[fix_bit_length13(ext_imm + regs[regA])];
                                uint16_t addr = ext_imm + regs[regA];
                                regs[regB] = L1.doubleCacheGetVal(addr, memory, L2, pc);
                                pc += 1;
                            }
                            //sw 
                            else if (op == 5) {
                                //stores value in source register into memory cell located from sum of immediate and value at addr register
                                //memory[fix_bit_length13(ext_imm + regs[regA])] = regs[regB];
                                uint16_t addr = ext_imm + regs[regA];
                                L1.doubleCacheSetVal(addr, regs[regB], memory, L2, pc);
                                pc += 1;
                            }
                            //jeq
                            else if (op == 6) {
                                //checks if two registers are equal and jumps if true, increments by one otherwise
                                if (regs[regA] == regs[regB]) {
                                    if ((pc + ext_imm + 0b1) % 128 == pc) {
                                        stop = true;
                                    }
                                    else {
                                        pc = (pc + ext_imm + 0b1)%128;
                                    }
                                }
                                else {
                                    pc += 1;
                                }
                                
                            }
                            //slti
                            else if (op == 7) {
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
                } else {
                    cerr << "Invalid cache config"  << endl;
                    return 1;
                }
            }

    return 0;
}
//ra0Eequ6ucie6Jei0koh6phishohm9
