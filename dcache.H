/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
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
//

/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_CACHE_H
#define PIN_CACHE_H


#define KILO 1024
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)

#define my_WARMUP 1000000000

typedef UINT64 CACHE_STATS; // type of cache hit/miss counters

#include <sstream>
#include <cassert>
#include <cmath>

#include <math.h>
//#include <unordered_map>

unsigned long long int ins_count=0;
unsigned long long int mem_count = 0;
unsigned  long long int mem_count_before_warmup = 0;
unsigned long long int current_count = 0;
unsigned long long int prev_count = 0;

//std::unordered_map<uint64_t, int> pageTableFreq;
//std::unordered_map<uint64_t, int>::iterator it_ptf;

//const int PAGESIZE = (8*KILO);

#define NUM_MICRO_PAGE 4
#define NUM_MEM_INDEX (32*KILO)
//#define PAGE_SIZE (4*KILO)
#define EPOCH 500000000

// types, constants
typedef enum
{
    ACCESS_TYPE_LOAD,
    ACCESS_TYPE_STORE,
    ACCESS_TYPE_NUM
} ACCESS_TYPE;

/*! RMR (rodric@gmail.com)
 *   - temporary work around because decstr()
 *     casts 64 bit ints to 32 bit ones
 */
static string mydecstr(UINT64 v, UINT32 w)
{
    ostringstream o;
    o.width(w);
    o << v;
    string str(o.str());
    return str;
}

/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0) return -1;

    if (n & 0xffff0000) { p += 16; n >>= 16; }
    if (n & 0x0000ff00)	{ p +=  8; n >>=  8; }
    if (n & 0x000000f0) { p +=  4; n >>=  4; }
    if (n & 0x0000000c) { p +=  2; n >>=  2; }
    if (n & 0x00000002) { p +=  1; }

    return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}


typedef struct page
{
    int ID;
    unsigned long long lastAcess;
    short counter[NUM_MICRO_PAGE];
    struct page *next;
} page_t;

class Memory
{
private:
    page_t *pages[NUM_MEM_INDEX];
    int _shiftMicroPage;
    int _shiftPage;
    int _shiftIndex;
    int total_num_page_accessed;
    int num_page_accessed;
    int total_num_access;
    int num_access;
public:

    Memory();
    void Access(ADDRINT addr, ACCESS_TYPE accessType);
    void PrintStat();
    void resetCounter();
};

Memory::Memory()
{
    for(int i=0;i<NUM_MEM_INDEX;i++)
    {
        pages[i]=NULL;
    }
    _shiftPage  = FloorLog2(PAGE_SIZE);
    _shiftIndex = FloorLog2(NUM_MEM_INDEX);
    _shiftMicroPage = FloorLog2(NUM_MICRO_PAGE);
    num_page_accessed=0;
}

void Memory::Access(ADDRINT addr, ACCESS_TYPE accessType)
{
    num_access++;
    total_num_access++;

    int index = (addr >> _shiftPage) & (NUM_MEM_INDEX-1);
    int ID = addr >> (_shiftIndex+_shiftPage);

    page_t *iter=pages[index];
    page_t *lastPage=pages[index];
    bool done=false;
    while(iter!=NULL)
    {
        if(iter->ID==ID)
        {
            //now what the fuck
            int counterIndex=(addr>> (_shiftIndex + _shiftPage -_shiftMicroPage) ) & (NUM_MICRO_PAGE-1);
            iter->counter[counterIndex]++;
            if((iter->lastAcess/EPOCH)!=(ins_count/EPOCH))
            {
                num_page_accessed++;
            }
            iter->lastAcess=ins_count;
            done=true;
            break;
        }
        lastPage=iter;
        iter=iter->next;
    }
    if(!done)
    {
        page_t *newPage=new page_t();
        if(lastPage)
            lastPage->next=newPage;
        else
            pages[index]=newPage;
        newPage->next=NULL;
        newPage->ID=ID;
        newPage->lastAcess=ins_count;
        for(int i=0;i<NUM_MICRO_PAGE;i++)
            newPage->counter[i]=0;
        num_page_accessed++;
        total_num_page_accessed++;
        // now what the fuck
        int counterIndex=(addr>> (_shiftIndex + _shiftPage -_shiftMicroPage) ) & (NUM_MICRO_PAGE-1);
        newPage->counter[counterIndex]++;
    }
}

void Memory::PrintStat()
{
    cerr << "total_num_page_accessed: " << total_num_page_accessed << endl;
    cerr << "num_page_accessed: " << num_page_accessed << endl;
    //cerr << "total_num_access: " << total_num_access << endl;
    cerr << "num_access: " << num_access << endl;
}

void Memory::resetCounter()
{
    num_page_accessed=0;
    num_access=0;
}
Memory *mainMemory;

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG
{
private:
    ADDRINT _tag;
    bool dirty;
public:
    CACHE_TAG(ADDRINT tag = 0) { _tag = tag; dirty=false; }
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
    bool IsDirty(){return dirty;}
    void SetDirty(bool value){dirty=value;}
    ADDRINT GetTag() {return _tag;}
    void SetTag(ADDRINT addr) {_tag=addr;}
};


/*!
 * Everything related to cache sets
 */
namespace CACHE_SET
{

/*!
 *  @brief Cache set direct mapped
 */
    class DIRECT_MAPPED
    {
    private:
        CACHE_TAG _tag;

    public:
        DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

        VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
        UINT32 GetAssociativity(UINT32 associativity) { return 1; }

        UINT32 Find(CACHE_TAG tag, ACCESS_TYPE access_type)
        {
            if(_tag == tag)
            {
                if(access_type == ACCESS_TYPE_STORE)
                    _tag.SetDirty(true);
                return true;
            }
            return false;
        }
        ADDRINT Replace(CACHE_TAG tag, ACCESS_TYPE access_type)
        {
            ADDRINT result=0;
            if(_tag.IsDirty())
                result=_tag;
            _tag = tag;
            if(access_type == ACCESS_TYPE_STORE)
                _tag.SetDirty(true);
            else
                _tag.SetDirty(false);
            return result;
        }

        bool SetDirty(CACHE_TAG tag)
        {
            bool result;
            if(_tag == tag)
            {
                _tag.SetDirty(true);
                goto end;
            }

            assert(false);
            //result = false;

            end: return result;
        }
    };

/*!
 *  @brief Cache set with round robin replacement
 */
    template <UINT32 MAX_ASSOCIATIVITY = 4>
    class LRU
    {
    private:
        CACHE_TAG _tags[MAX_ASSOCIATIVITY];
        UINT32 _tagsLastIndex;
        // UINT32 _nextReplaceIndex;
        unsigned long long int LRUStack;

    public:
        LRU()
        {
            LRUStack=0xfedcba9876543210;
        }

        VOID SetAssociativity(UINT32 associativity)
        {
            ASSERTX(associativity <= MAX_ASSOCIATIVITY);
            _tagsLastIndex = associativity - 1;
        }

        UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

        VOID updateLRUStack(int way)
        {
            if(way==0)
                return;
            //cerr << "way: " << way << endl;
            ///cerr << "before update :" << hex << LRUStack << dec << endl;
            unsigned long long int MRU = (LRUStack>> (4*way) ) & 0x0f;
            unsigned long long int temp = ( (1<<(4*(way-1)))-1 ) & LRUStack;
            temp = temp <<4;
            temp |= MRU;
            LRUStack = (LRUStack & ~((1<<(4*way+1))-1) ) | temp;

            ///cerr << "after update :" << hex << LRUStack << dec << endl;
        }

        UINT32 Find(CACHE_TAG tag, ACCESS_TYPE access_type)
        {

            bool result = true;
            int realIndex;
            //cerr << "_tagsLastIndex= " << _tagsLastIndex<< endl;
            for (UINT32 index = 0; index <= _tagsLastIndex; index++)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                realIndex = (int) ((LRUStack >> (4*index)) & 0x0f);
                ///cerr << "	Find: " << _tags[realIndex].GetTag()<< endl;
                if(_tags[realIndex] == tag)
                {
                    ///cerr << "	bingo " << _tags[realIndex].GetTag() << " = " << tag.GetTag() << endl;
                    if(access_type == ACCESS_TYPE_STORE)
                        _tags[realIndex].SetDirty(true);
                    updateLRUStack(index);
                    goto end;
                }
            }
            result = false;

            end: return result;
        }

        ADDRINT Replace(CACHE_TAG tag, ACCESS_TYPE access_type)
        {
            //cerr << "----- " << hex << LRUStack << dec << endl;
            ADDRINT result=0;
            int index = (LRUStack >> (4*_tagsLastIndex))& 0x0f;
            updateLRUStack(_tagsLastIndex);
            if(_tags[index].IsDirty())
            {
                result = _tags[index].GetTag();
                ///cerr << "In Replace result: " << hex << result << endl;
            }
            _tags[index] = tag;
            if(access_type == ACCESS_TYPE_STORE)
                _tags[index].SetDirty(true);
            else
                _tags[index].SetDirty(false);
            //cerr << "way: " << index << endl;
            return result;
        }

        bool SetDirty(CACHE_TAG tag)
        {
            bool result;
            int realIndex;
            for (INT32 index = _tagsLastIndex; index >= 0; index--)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                realIndex = (int) ((LRUStack >> (4*index)) & 0x0f);
                if(_tags[realIndex] == tag)
                {
                    _tags[realIndex].SetDirty(true);
                    updateLRUStack(index);
                    result=true;
                    goto end;
                }
            }

            //assert(false);
            //result = false;

            end: return result;
        }
    };

    template <UINT32 MAX_ASSOCIATIVITY = 4>
    class ROUND_ROBIN
    {
    private:
        CACHE_TAG _tags[MAX_ASSOCIATIVITY];
        UINT32 _tagsLastIndex;
        UINT32 _nextReplaceIndex;

    public:
        ROUND_ROBIN(UINT32 associativity = MAX_ASSOCIATIVITY)
                : _tagsLastIndex(associativity - 1)
        {
            ASSERTX(associativity <= MAX_ASSOCIATIVITY);
            _nextReplaceIndex = _tagsLastIndex;

            for (INT32 index = _tagsLastIndex; index >= 0; index--)
            {
                _tags[index] = CACHE_TAG(0);
            }
        }

        VOID SetAssociativity(UINT32 associativity)
        {
            ASSERTX(associativity <= MAX_ASSOCIATIVITY);
            _tagsLastIndex = associativity - 1;
            _nextReplaceIndex = _tagsLastIndex;
        }
        UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

        UINT32 Find(CACHE_TAG tag, ACCESS_TYPE access_type)
        {
            bool result = true;

            for (INT32 index = _tagsLastIndex; index >= 0; index--)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                if(_tags[index] == tag)
                {
                    if(access_type == ACCESS_TYPE_STORE)
                        _tags[index].SetDirty(true);
                    goto end;
                }
            }
            result = false;

            end: return result;
        }

        ADDRINT Replace(CACHE_TAG tag, ACCESS_TYPE access_type)
        {
            // g++ -O3 too dumb to do CSE on following lines?!
            const UINT32 index = _nextReplaceIndex;
            ADDRINT result=0;
            if(_tags[index].IsDirty())
                result= _tags[index];
            _tags[index] = tag;
            if(access_type == ACCESS_TYPE_STORE)
                _tags[index].SetDirty(true);
            else
                _tags[index].SetDirty(false);
            // condition typically faster than modulo
            _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
            return result;
        }

        bool SetDirty(CACHE_TAG tag)
        {
            bool result;
            for (INT32 index = _tagsLastIndex; index >= 0; index--)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                if(_tags[index] == tag)
                {
                    _tags[index].SetDirty(true);
                    goto end;
                }
            }
            assert(false);
            //result = false;

            end: return result;
        }
    };

} // namespace CACHE_SET

namespace CACHE_ALLOC
{
    typedef enum
    {
        STORE_ALLOCATE,
        STORE_NO_ALLOCATE
    } STORE_ALLOCATION;
}

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class CACHE_BASE
{
public:

    typedef enum
    {
        CACHE_TYPE_ICACHE,
        CACHE_TYPE_DCACHE,
        CACHE_TYPE_NUM
    } CACHE_TYPE;

protected:
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

private:    // input params
    const std::string _name;
    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;

    // computed params
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;
    const UINT32 _indexShift;

    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

protected:

    UINT32 NumSets() const { return _setIndexMask + 1; }

public:
    // constructors/destructors
    CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity);

    // accessors
    UINT32 CacheSize() const { return _cacheSize; }
    UINT32 LineSize() const { return _lineSize; }
    UINT32 Associativity() const { return _associativity; }
    //
    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true];}
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false];}
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType);}
    CACHE_STATS Hits() const { return SumAccess(true);}
    CACHE_STATS Misses() const { return SumAccess(false);}
    CACHE_STATS Accesses() const { return Hits() + Misses();}

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex) const
    {
        tag.SetTag(addr >> _lineShift);
        setIndex = tag.GetTag() & _setIndexMask;
    }

    ADDRINT RecoverAddress(const ADDRINT addr, UINT32 setIndex)
    {
        return (addr<< _lineShift)  ;
    }

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 & lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tag, setIndex);
    }

    string StatsLong(string prefix = "", CACHE_TYPE = CACHE_TYPE_DCACHE) const;
    string GetName() {return _name;}
};

CACHE_BASE::CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity)
        : _name(name),
          _cacheSize(cacheSize),
          _lineSize(lineSize),
          _associativity(associativity),
          _lineShift(FloorLog2(lineSize)),
          _setIndexMask((cacheSize / (associativity * lineSize)) - 1),
          _indexShift(FloorLog2(cacheSize / (associativity * lineSize)))
{
    cout << _name << "_lineShift: " << _lineShift << " _indexShift: " << _indexShift  << " _setIndexMask: " << _setIndexMask<< endl;
    ASSERTX(IsPower2(_lineSize));
    ASSERTX(IsPower2(_setIndexMask + 1));

    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

/*!
 *  @brief Stats output method
 */

string CACHE_BASE::StatsLong(string prefix, CACHE_TYPE cache_type) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 12;

    string out;

    out += prefix + _name + ":" + "\n";

    if (cache_type != CACHE_TYPE_ICACHE) {
        for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++)
        {
            const ACCESS_TYPE accessType = ACCESS_TYPE(i);

            std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

            out += prefix + ljstr(type + "-Hits:      ", headerWidth)
                   + mydecstr(Hits(accessType), numberWidth)  +
                   "  " +fltstr(100.0 * Hits(accessType) / Accesses(accessType), 2, 6) + "%\n";

            out += prefix + ljstr(type + "-Misses:    ", headerWidth)
                   + mydecstr(Misses(accessType), numberWidth) +
                   "  " +fltstr(100.0 * Misses(accessType) / Accesses(accessType), 2, 6) + "%\n";

            out += prefix + ljstr(type + "-Accesses:  ", headerWidth)
                   + mydecstr(Accesses(accessType), numberWidth) +
                   "  " +fltstr(100.0 * Accesses(accessType) / Accesses(accessType), 2, 6) + "%\n";

            out += prefix + "\n";
        }
    }

    out += prefix + ljstr("Total-Hits:      ", headerWidth)
           + mydecstr(Hits(), numberWidth) +
           "  " +fltstr(100.0 * Hits() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Misses:    ", headerWidth)
           + mydecstr(Misses(), numberWidth) +
           "  " +fltstr(100.0 * Misses() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Accesses:  ", headerWidth)
           + mydecstr(Accesses(), numberWidth) +
           "  " +fltstr(100.0 * Accesses() / Accesses(), 2, 6) + "%\n";
    out += "\n";

    return out;
}


/*!
 *  @brief Templated cache class with specific cache set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of cache sets.
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
class CACHE : public CACHE_BASE
{
private:
    SET _sets[MAX_SETS];
    CACHE* next_level;
    int hit_penalty;
    int miss_penalty;
public:
    // constructors/destructors
    CACHE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, int hit, int miss)
            :CACHE_BASE(name, cacheSize, lineSize, associativity)//:hit_penalty(hit):miss_penalty(miss)
    {
        hit_penalty=hit;
        miss_penalty=miss;
        ASSERTX(NumSets() <= MAX_SETS);

        for (UINT32 i = 0; i < NumSets(); i++)
        {
            _sets[i].SetAssociativity(associativity);
        }
    }
    void setNextLevel(CACHE* nextLevel){next_level=nextLevel;}
    // modifiers
    /// Cache access from addr to addr+size-1
    bool Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);
    /// Cache access at addr that does not span cache lines
    bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);
    VOID SetDirty(ADDRINT addr)
    {
        CACHE_TAG tag;
        UINT32 setIndex;
        ///cerr << "in CACHE::SetDirty\n";
        SplitAddress(addr, tag, setIndex);
        SET & set = _sets[setIndex];
        // if (!set.Find(tag,ACCESS_TYPE_STORE))
        //	cerr <<"!! "<< GetName() << addr<< endl;
        ///cerr << GetName() << "addr: " << addr << " tag: " << tag << "{\n";
        assert(set.Find(tag,ACCESS_TYPE_LOAD));
        ///cerr << "}\n";
        set.SetDirty(tag);
    }





};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    ///cerr <<  GetName() << "Access(" << hex << addr <<","<< accessType << ")\n";
    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        CACHE_TAG tag;
        UINT32 setIndex;

        SplitAddress(addr, tag, setIndex);
        ///cerr << GetName() << " ("<< addr << ","<< tag <<","<< setIndex<< ")\n" ;
        SET & set = _sets[setIndex];

        bool localHit = set.Find(tag,accessType);
        allHit &= localHit;
        if(localHit)
        {
            ins_count+= hit_penalty;
        }
        else
        {
            ins_count+= miss_penalty;
        }
        // on miss, loads always allocate, stores optionally
        if ( (! localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        {
            ADDRINT victim =set.Replace(tag,accessType);
            ///cerr << GetName() << " Victim is:  " << victim << endl;
            if (victim != 0)
            {
                victim = RecoverAddress(victim ,setIndex) ;
                ///cerr << "recovered address is " << victim  << " from setIndex " << hex << setIndex << endl;
            }
            if(next_level!=NULL)
            {
                if(victim!=0)
                    next_level->SetDirty(victim);
                next_level->AccessSingleLine(addr,accessType);
                //else
                //	cout << "victim is clean\n";
            }
            else
            {
                mainMemory->Access(addr,accessType);
                if(victim!=0) {
                    mainMemory->Access(victim, ACCESS_TYPE_STORE);

                }
            }

        }

        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);

    _access[accessType][allHit]++;

    return allHit;
}

/*!
 *  @return true if accessed cache line hits
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType)
{
    ///cerr << GetName() << "AccessSingleLine(" << hex << addr <<","<< accessType << ")\n";

    CACHE_TAG tag;

    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);
    ///cerr << "("<< addr << ","<< tag <<","<< setIndex<< ")\n" ;
    SET & set = _sets[setIndex];

    bool hit = set.Find(tag,accessType);

    if(hit)
        ins_count+= hit_penalty;
    else
        ins_count+= miss_penalty;

    // on miss, loads always allocate, stores optionally
    if ( (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        ADDRINT victim = set.Replace(tag,accessType);
        ///cerr << GetName() <<  " Victim is:  " << victim << endl;
        if (victim != 0)
            victim = RecoverAddress(victim ,setIndex) ;
        if(next_level!=NULL)
        {
            //cout <<"ggggg\n";
            if(victim!=0)
                next_level->SetDirty(victim);
            next_level->AccessSingleLine(addr,accessType);
        }
        else
        {
            if (ins_count > my_WARMUP)
                mem_count++;
            else {
                mem_count_before_warmup++;
                if (ins_count%1 == 0)
                    cerr << " mem_count before warm up = "<< mem_count_before_warmup << " ins_count = " << ins_count<<"\n";
            }
            if (ins_count == my_WARMUP)
                cerr << "warm up done with # memory accesses= " << mem_count_before_warmup <<"\n";
            if ((mem_count % 10000000 == 0) && (mem_count != 0))
                cerr << "mem = " << mem_count <<"\n";
            mainMemory->Access(addr,accessType);
            current_count = ins_count;
            //cout <<"kkkkkk\n";
            if (ins_count > my_WARMUP) {
                if (accessType == ACCESS_TYPE_LOAD)
                    cout << "META " << std::dec << current_count - prev_count << " R " << std::hex
                         << (addr & 0xFFFFFFFFFFFFFFC0) << " 26432 " << "\n";
                else
                    cout << "META " << std::dec << current_count - prev_count << " W " << std::hex
                         << (addr & 0xFFFFFFFFFFFFFFC0) << "\n";

                // page number should be counted
                /*uint64_t page_ID = (addr >> (int)log2((double)PAGESIZE));
                it_ptf = pageTableFreq.find(page_ID);
                if (it_ptf == pageTableFreq.end())
                {
                    // not found
                    pageTableFreq.insert(std::make_pair(page_ID, 1));
                }
                else
                    it_ptf->second++;
*/

            }
            prev_count = ins_count;
            if(victim!=0) {
                if (ins_count > my_WARMUP) {

                    unsigned long long int diff = (current_count == prev_count) ? 1 : current_count - prev_count;

                    cout << "META " << std::dec << diff << " W " << std::hex
                         << (victim & 0xFFFFFFFFFFFFFFC0) << "\n";
                    // page number should be counted
                    /*uint64_t page_ID = (addr >> (int)log2((double)PAGESIZE));
                    it_ptf = pageTableFreq.find(page_ID);
                    if (it_ptf == pageTableFreq.end())
                    {
                        // not found
                        pageTableFreq.insert(std::make_pair(page_ID, 1));
                    }
                    else
                        it_ptf->second++;*/
                }
                prev_count = ins_count;
                mainMemory->Access(victim, ACCESS_TYPE_STORE);
            }
            //else
            //		cout << "victim is clean\n";
        }
        //cerr << "back to higher level \n";
    }
    //next->accessSingleLine(addr,accessType);
    _access[accessType][hit]++;

    return hit;
}

// define shortcuts
#define CACHE_DIRECT_MAPPED(MAX_SETS, ALLOCATION) CACHE<CACHE_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_LRU(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::LRU<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

#endif // PIN_CACHE_H
