/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redist ributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
//
// @ORIGINAL_AUTHOR: Artur Klauser
// @EXTENDED: Rodric Rabbah (rodric@gmail.com)
//

/*! @file
 *  This file contains an ISA-portable cache simulator
 *  data cache hierarchies
 */


#include "pin.H"

#include <iostream>
#include <fstream>

#include "dcache.H"
#include "pin_profile.H"

std::ofstream outFile;
VOID Fini(int code, VOID * v);
/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
                            "o", "dcache.out", "specify dcache file name");
KNOB<BOOL>   KnobTrackLoads(KNOB_MODE_WRITEONCE,    "pintool",
                            "tl", "0", "track individual loads -- increases profiling time");
KNOB<BOOL>   KnobTrackStores(KNOB_MODE_WRITEONCE,   "pintool",
                             "ts", "0", "track individual stores -- increases profiling time");
KNOB<UINT32> KnobThresholdHit(KNOB_MODE_WRITEONCE , "pintool",
                              "rh", "100", "only report memops with hit count above threshold");
KNOB<UINT32> KnobThresholdMiss(KNOB_MODE_WRITEONCE, "pintool",
                               "rm","100", "only report memops with miss count above threshold");
KNOB<UINT32> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool",
                           "c","32", "cache size in kilobytes");
KNOB<UINT32> KnobLineSize(KNOB_MODE_WRITEONCE, "pintool",
                          "b","32", "cache block size in bytes");
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
                               "a","4", "cache associativity (1 for direct mapped)");

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr <<
         "This tool represents a cache simulator.\n"
         "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

// wrap configuation constants into their own name space to avoid name clashes

#define WARMUP 40000000000
namespace DL1
{
    const UINT32 max_sets = 8*KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 256; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    //typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
}

DL1::CACHE*  dl1 = NULL;
DL1::CACHE*  l2  = NULL;
//DL1::CACHE*  l3  = NULL;

typedef enum
{
    COUNTER_MISS = 0,
    COUNTER_HIT = 1,
    COUNTER_NUM
} COUNTER;



typedef  COUNTER_ARRAY<UINT64, COUNTER_NUM> COUNTER_HIT_MISS;


// holds the counters with misses and hits
// conceptually this is an array indexed by instruction address
COMPRESSOR_COUNTER<ADDRINT, UINT32, COUNTER_HIT_MISS> profile;

/* ===================================================================== */

VOID docount()
{
    ins_count++;
    if( ( (ins_count%EPOCH)==0 ) & (ins_count>WARMUP) )
    {
        cerr << "$$$$ " << ins_count << flush << endl;
        cerr.flush();
      //  mainMemory->PrintStat();
      //  mainMemory->resetCounter();
    }
   /* if(ins_count>= 100000000000)
    {


        outFile << "PIN:MEMLATENCIES 1.0. 0x0\n";

        outFile <<
                "#\n"
                "# DCACHE stats\n"
                "#\n";

        outFile << dl1->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

        if( KnobTrackLoads || KnobTrackStores ) {
            outFile <<
                    "#\n"
                    "# LOAD stats\n"
                    "#\n";
            outFile << profile.StringLong();
        }

        outFile << l2->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

        if( KnobTrackLoads || KnobTrackStores ) {
            outFile <<
                    "#\n"
                    "# LOAD stats\n"
                    "#\n";

            outFile << profile.StringLong();
        }

        outFile << l3->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

        if( KnobTrackLoads || KnobTrackStores ) {
            outFile <<
                    "#\n"
                    "# LOAD   stats\n"
                    "#\n";

            outFile << profile.StringLong();
        }
        outFile.close();
        //Fini(0,0);
        cout << "cycle:" << ins_count << endl;
        char c;
        cin.get(c);
    }*/

}

VOID LoadMulti(ADDRINT addr, UINT32 size, UINT32 instId)
{
    // first level D-cache
    if(ins_count>WARMUP)
    {
        const BOOL dl1Hit = dl1->Access(addr, size, ACCESS_TYPE_LOAD);

        const COUNTER counter = dl1Hit ? COUNTER_HIT : COUNTER_MISS;
        profile[instId][counter]++;
    }
}

/* ===================================================================== */

VOID StoreMulti(ADDRINT addr, UINT32 size, UINT32 instId)
{
    // first level D-cache
    if(ins_count>WARMUP)
    {
        const BOOL dl1Hit = dl1->Access(addr, size, ACCESS_TYPE_STORE);

        const COUNTER counter = dl1Hit ? COUNTER_HIT : COUNTER_MISS;
        profile[instId][counter]++;
    }
}

/* ===================================================================== */

VOID LoadSingle(ADDRINT addr, UINT32 instId)
{
    // @todo we may access several cache lines for
    // first level D-cache
    if(ins_count>WARMUP)
    {
        const BOOL dl1Hit = dl1->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

        const COUNTER counter = dl1Hit ? COUNTER_HIT : COUNTER_MISS;
        profile[instId][counter]++;
    }
}
/* ===================================================================== */

VOID StoreSingle(ADDRINT addr, UINT32 instId)
{
    // @todo we may access several cache lines for
    // first level D-cache
    if(ins_count>WARMUP)
    {
        const BOOL dl1Hit = dl1->AccessSingleLine(addr, ACCESS_TYPE_STORE);

        const COUNTER counter = dl1Hit ? COUNTER_HIT : COUNTER_MISS;
        profile[instId][counter]++;
    }
}

/* ===================================================================== */

VOID LoadMultiFast(ADDRINT addr, UINT32 size)
{
    if(ins_count>WARMUP)
        dl1->Access(addr, size, ACCESS_TYPE_LOAD);
}

/* ===================================================================== */

VOID StoreMultiFast(ADDRINT addr, UINT32 size)
{
    if(ins_count>WARMUP)
        dl1->Access(addr, size, ACCESS_TYPE_STORE);
}

/* ===================================================================== */

VOID LoadSingleFast(ADDRINT addr)
{
    if(ins_count>WARMUP)
        dl1->AccessSingleLine(addr, ACCESS_TYPE_LOAD);
}

/* ===================================================================== */

VOID StoreSingleFast(ADDRINT addr)
{
    if(ins_count>WARMUP)
        dl1->AccessSingleLine(addr, ACCESS_TYPE_STORE);
}



/* ===================================================================== */

VOID Instruction(INS ins, void * v)
{
    //if (mem_count > 1000000000)
    //    PIN_ExitThread(-1);

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);

    //if(ins_count%1000000==0)
    //cerr << "ins_addr: " << INS_Address(ins) << endl;

    if (INS_IsMemoryRead(ins))
    {
        // map sparse INS addresses to dense IDs
        const ADDRINT iaddr = INS_Address(ins);
        const UINT32 instId = profile.Map(iaddr);

        const UINT32 size = INS_MemoryReadSize(ins);
        const BOOL   single = (size <= 4);

        if( KnobTrackLoads )
        {
            if( single )
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE, (AFUNPTR) LoadSingle,
                        IARG_MEMORYREAD_EA,
                        IARG_UINT32, instId,
                        IARG_END);
            }
            else
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) LoadMulti,
                        IARG_MEMORYREAD_EA,
                        IARG_MEMORYREAD_SIZE,
                        IARG_UINT32, instId,
                        IARG_END);
            }

        }
        else
        {
            if( single )
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) LoadSingleFast,
                        IARG_MEMORYREAD_EA,
                        IARG_END);

            }
            else
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) LoadMultiFast,
                        IARG_MEMORYREAD_EA,
                        IARG_MEMORYREAD_SIZE,
                        IARG_END);
            }
        }
    }

    if ( INS_IsMemoryWrite(ins) )
    {
        // map sparse INS addresses to dense IDs
        const ADDRINT iaddr = INS_Address(ins);
        const UINT32 instId = profile.Map(iaddr);

        const UINT32 size = INS_MemoryWriteSize(ins);

        const BOOL   single = (size <= 4);

        if( KnobTrackStores )
        {
            if( single )
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) StoreSingle,
                        IARG_MEMORYWRITE_EA,
                        IARG_UINT32, instId,
                        IARG_END);
            }
            else
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) StoreMulti,
                        IARG_MEMORYWRITE_EA,
                        IARG_MEMORYWRITE_SIZE,
                        IARG_UINT32, instId,
                        IARG_END);
            }

        }
        else
        {
            if( single )
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) StoreSingleFast,
                        IARG_MEMORYWRITE_EA,
                        IARG_END);

            }
            else
            {
                INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) StoreMultiFast,
                        IARG_MEMORYWRITE_EA,
                        IARG_MEMORYWRITE_SIZE,
                        IARG_END);
            }
        }

    }

}

/* ================ ====== =============================================== */

VOID Fini(int code, VOID * v)
{
    // print D-cache profile
    // @todo what does this print
     cerr <<  "Count " << ins_count  << " mem count before warmup = "
                                       << mem_count_before_warmup << " mem count after warm up = " << mem_count<< endl;


    outFile << "PIN:MEMLATENCIES 1.0. 0x0\n";

    outFile <<
            "#\n"
            "# DCACHE stats\n"
            "#\n";

    outFile << dl1->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

    if( KnobTrackLoads || KnobTrackStores ) {
        outFile <<
                "#\n"
                "# LOAD stats\n"
                "#\n";

        outFile << profile.StringLong();
    }

    outFile << l2->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

    if( KnobTrackLoads || KnobTrackStores ) {
        outFile <<
                "#\n"
                "# LOAD stats\n"
                "#\n";

        outFile << profile.StringLong();
    }

    //outFile << l3->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

    if( KnobTrackLoads || KnobTrackStores ) {
        outFile <<
                "#\n"
                "# LOAD stats\n"
                "#\n";

        outFile << profile.StringLong();
    }
    outFile.close();

}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    outFile.open(KnobOutputFile.Value().c_str());

    dl1 = new DL1::CACHE("L1 ",  32*KILO, 64, 4,1,4); //(KnobCacheSize.Value() * KILO,KnobLineSize.Value(),KnobAssociativity.Value());
    l2  = new DL1::CACHE("L2 ", 1024 * KILO, 64, 8,4,150);
    //l3  = new DL1::CACHE("L3 ", 2048 * KILO, 64, 8,0,0);
    mainMemory = new Memory();
    dl1->setNextLevel(l2);
    l2->setNextLevel(NULL); ///l2->setNextLevel(l3);
    //l3->setNextLevel(NULL);

    profile.SetKeyName("iaddr         ");
    profile.SetCounterName("dcache:miss      dcache:hit");

    COUNTER_HIT_MISS threshold;

    threshold[COUNTER_HIT] = KnobThresholdHit.Value();
    threshold[COUNTER_MISS] = KnobThresholdMiss.Value();

    profile.SetThreshold( threshold );

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns

    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */