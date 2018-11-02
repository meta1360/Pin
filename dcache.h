/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2018 Intel Corporation. All rights reserved.

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
/*! @file
 *  This file contains a configurable cache class
 */



#ifndef PIN_CACHE_H
#define PIN_CACHE_H

#include <iostream>     // std::cin, std::cout
#include <iomanip>      // std::get_time
#include <ctime>        // struct std::tm
#include <sstream>
#include <cassert>
#include <math.h>




#define KILO 1024
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)

const size_t WARMUP(4000000000);
const long long int EPOCH =  500000000;
long long int count_inst;
//ofstream myfile ("example.txt");
FILE * my_file;

typedef enum
{
    ACCESS_TYPE_LOAD,
    ACCESS_TYPE_STORE,
    ACCESS_TYPE_NUM
}ACCESS_TYPE;

unsigned long long int ins_count=0;

unsigned long long int current_count = 0;
unsigned long long int prev_count = 0;
unsigned long long int mem_count_before_warmup = 0;
unsigned long long int mem_count_after_warmup = 0;


typedef UINT64 CACHE_STATS; // type of cache hit/miss counters



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

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG
{
private:
    ADDRINT _tag;
    bool dirty;
    bool valid;


public:
    CACHE_TAG(ADDRINT tag = 0) { _tag = tag; dirty = false;}
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
    bool IsDirty(){return dirty;}
    bool IsValid() { return  valid;}
    void SetDirty(bool value){dirty = value;}
    void SetValid (bool value){valid=value;}
    ADDRINT GetTag(){return _tag;}
    void SetTag(ADDRINT tg){_tag = tg;}
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

        UINT32 Find(CACHE_TAG tag) { return(_tag == tag); }
        VOID Replace(CACHE_TAG tag) { _tag = tag; }
    };

/*!
 *  @brief Cache set with round robin replacement
 */
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

        UINT32 Find(CACHE_TAG tag)
        {
            bool result = true;

            for (INT32 index = _tagsLastIndex; index >= 0; index--)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                if(_tags[index] == tag) goto end;
            }
            result = false;

            end: return result;
        }

        VOID Replace(CACHE_TAG tag)
        {
            // g++ -O3 too dumb to do CSE on following lines?!
            const UINT32 index = _nextReplaceIndex;

            _tags[index] = tag;
            // condition typically faster than modulo
            _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
        }
    };

    template <UINT32 MAX_ASSOCIATIVITY=8>
    class LRU
    {
    private:
        CACHE_TAG _tag[MAX_ASSOCIATIVITY];
        UINT32 _tagslastindex;
        int LRUNum[MAX_ASSOCIATIVITY];
    public:
        LRU(){
            for (int i=0; i<MAX_ASSOCIATIVITY; i++)
            {
                LRUNum[i] = 0;
                _tag[i].SetDirty(false);
            }

        }
        ~LRU(){
            cout << "LRU set is distructed....\n";
        }
        void SetAssociativity(UINT32 associativity)
        {
            ASSERTX(associativity <= MAX_ASSOCIATIVITY);
            _tagslastindex = associativity-1;
        }
        UINT32 getAssociativity()
        {
            return (_tagslastindex + 1);
        }
        void update_LRU_array(int way)
        {
            for (int i=0; i<=_tagslastindex; i++)
            {
                if (i == way)
                    LRUNum[i] = 0;
                else
                    LRUNum[i]++;
            }
        }
        bool Find(CACHE_TAG tag, ACCESS_TYPE access_type)
        {
            bool hit = false;
            ///int realIndex;

            // cout << dec << current_cycle() <<" : " << " FIND func " << " look for " << hex << tag << "\n" ;
            for (int index=0; index<=_tagslastindex; index++)
            {
                ///  realIndex = (int) ((LRUStack >> (4*index)) & 0x0f);
                ///   cout << _tag[index].get_tag() << "  ";
                if ((_tag[index] == tag) && (_tag[index].IsValid()))
                {
                    //hit
                    if (access_type == ACCESS_TYPE_STORE)
                    {
                        _tag[index].SetDirty(true);
                    }

                    hit = true;
                    update_LRU_array(index);
                    //    cout << " >>>>  found \n";
                    //    cout << "----------------------------------------\n";
                    return (hit);
                }
            }
            //cout << " >>> not found \n";
            //cout << "----------------------------------------\n";
            return (hit);
        }
        CACHE_TAG Replace(CACHE_TAG tag, ACCESS_TYPE access_type)
        {

            int max_way = -2;
            int index=0;
            CACHE_TAG result; // = (CACHE_TAG *) malloc (sizeof(CACHE_TAG));
            result.SetValid(false);
            result.SetDirty(false);

            for (int i=0; i<=_tagslastindex; i++)
            {
                //   cout << "[ " <<i << " " << LRUnum[i] << " ]";
                if (!_tag[i].IsValid())
                {
                    index = i;
                    break;
                }
                if (LRUNum[i] >= max_way)
                {
                    max_way = LRUNum[i];
                    index = i;
                };
                //LRUStack = LRUStack >> 2;
            }
            ///index = 5;
            assert((index >= 0) && (index <= _tagslastindex));
            //cout << "index = " << index <<"\n";

            update_LRU_array(index);

            //cout << "my index = " << index <<" ";
            // index = (LRUStack >> (4*_tagslastindex))& 0x0f;
            //cout << " other index = " << index << "\n";
            //if (_tag[index].get_tag() == 0)
            //cout << current_cycle() <<": " << tag is zero not sure if it is
            if (_tag[index].IsDirty() && _tag[index].IsValid()) {
                // assert (_tag[index].IsValid());
                result = _tag[index];
                //   if (_tag[index].get_tag() == 0)
                //       cout << dec << current_cycle() <<": " << "tag is zero and dirty in replace 0 \n";
                //   assert(_tag[index].get_tag() != 0);
            }
            _tag[index] = tag;
            //if (_tag[index].get_tag() == 0)
            //cout << "tag is zero \n";
            _tag[index].SetValid(true);


            if (access_type == ACCESS_TYPE_STORE)
            {
                _tag[index].SetDirty(true);
            }
            else
                _tag[index].SetDirty(false);
            return result;
        }
        bool SetDirty(CACHE_TAG tag, bool value)
        {
            bool found = 0;
            for (int i=0; i<=_tagslastindex; i++)
            {
                if (_tag[i] == tag)
                {
                    found = 1;
                    _tag[i].SetDirty(value);
                    update_LRU_array(i);
                }
            }
            return found;
        }

        bool SetValid(CACHE_TAG tag, bool value) {
            bool found = 0;
            for (int i = 0; i <= _tagslastindex; i++) {
                if (_tag[i] == tag) {
                    found = 1;
                    _tag[i].SetValid(value);
                }
            }
            return found;
        }


    };

}; // namespace CACHE_SET


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
    // types, constants
   /* typedef enum
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;
*/
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
    std::string get_name() {return _name;}
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
        tag = addr >> _lineShift;
        setIndex = tag & _setIndexMask;
    }

    ADDRINT RecoverAddress (ADDRINT tag)
    {
        return (tag << _lineShift);
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
          _setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{

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
            : CACHE_BASE(name, cacheSize, lineSize, associativity)
    {
        hit_penalty = hit;
        miss_penalty = miss;
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
    bool SetDirty(ADDRINT addr);



};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    // this is an "Non-Inclusive cache". it means that when a block will be taken in to the
    // higher level of cache, it will be kept in the current level as well. As a block is to be brought
    // to a level of cache, it needs to evict a cache line; if clean, the evicted cache line will be
    // nicely thrown a way; If it is dirty it needs to be written back in to the lower level of cache.
    // In a non-inclusive cache, we do not need to invalidate a cache line in lower levels, as a cache
    // line is evicted.

    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        CACHE_TAG tag;
        UINT32 setIndex;

        SplitAddress(addr, tag, setIndex);

        SET & set = _sets[setIndex];

        bool localHit = set.Find(tag, accessType);
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
        if ( (! localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE)) {
            CACHE_TAG victim = set.Replace(tag, accessType);
            ADDRINT victim_tag;
            if (victim.IsValid()) //(victim != 0)
            {
                /*if (VERBOSE)
                    cout << std::dec << current_cycle() << ": " << std::hex << victim.get_tag() << " evicted from " << get_name()  <<"\n";*/
                victim_tag = victim.GetTag();
                victim_tag = RecoverAddress(victim_tag); // ,setIndex) ;
                // cout << "recovered address is " << victim_tag  <<"\n"; // " from setIndex " << hex << setIndex << endl;
            }

            if (next_level != NULL) {
                if (victim.IsValid() && victim.IsDirty()) {

                    next_level->SetDirty(victim);

                    /*if (VERBOSE)
                        cout << std::dec <<  current_cycle() << ": " << std::hex << " victim " << victim_tag << " is stored in " << next_level->get_name() <<"\n";*/
                    ///next_level->SetDirty(victim); inclusive
                    //whether or not it is fond, we need to write it back
                    next_level->AccessSingleLine(victim_tag, ACCESS_TYPE_STORE);
                    /////next_level->AccessSingleLine(victim,)

                }
                next_level->AccessSingleLine(tag, accessType);
            }
            /*else{
                // this level is an external Memory
                // if victum is dirty there is a write otherwise Nothing!
                // for tag we need to read this block from memory whether or not it is a read request.

            }*/
        } // if local hit

        addr = (addr & notLineMask) + lineSize; // start of next cache line
    } // while
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
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];

    bool hit = set.Find(tag, accessType);

    if(hit)
        ins_count+= hit_penalty;
    else
        ins_count+= miss_penalty;

    // on miss, loads always allocate, stores optionally
    if ( (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        CACHE_TAG victim = set.Replace(tag, accessType);
        ADDRINT victim_tag = 0;
        if (victim.IsValid())
        {
            victim_tag = victim.GetTag();
            victim_tag = RecoverAddress(victim_tag);
        }
        if (next_level != NULL) {
            if (victim.IsValid() && victim.IsDirty()) {

                next_level->SetDirty(victim);
                //whether or not it is fond, we need to write it back
                next_level->AccessSingleLine(victim_tag, ACCESS_TYPE_STORE);
            }
            next_level->AccessSingleLine(addr, accessType);
        }
        else{
            // this level is an external Memory
            // if victum is dirty there is a write otherwise Nothing!
            // for tag we need to read this block from memory whether or not it is a read request.


            long long int diff;
            // write back into memory
            current_count = ins_count;
            diff = current_count - prev_count;
            prev_count = ins_count;
            if (victim.IsValid() && victim.IsDirty())
            {
                victim_tag = victim.GetTag();
                victim_tag = RecoverAddress(victim_tag);
                if (ins_count > WARMUP) {
                    ADDRINT  vic = victim_tag & 0xFFFFFFFFFFFFFFC0;
                    //cerr.flush();
                    //cerr <<" META "<< std::dec << diff << " W " << std::hex << vic << endl;
                    //cerr.flush();
                    fprintf(my_file," META %lld W %lx\n", diff, vic);
                    mem_count_after_warmup++;
                }
                else
                    mem_count_before_warmup++;
            }
            victim_tag = victim.GetTag();
            victim_tag = RecoverAddress(victim_tag);

            if (ins_count > WARMUP) {
                //uint64_t  vic = victim_tag & 0xFFFFFFFFFFFFFFC0;
                //cerr.flush();
                //cerr <<" META "<< std::dec << diff << " R " << std::hex << addr << " 26432 " << endl;
                //cerr.flush();
                fprintf(my_file," META %lld R %lx\n", diff, addr);
                mem_count_after_warmup++;
            }
            else
                mem_count_before_warmup++;
        }
    } // if local hit
    _access[accessType][hit]++;
    return hit;
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::SetDirty(ADDRINT addr)
{
    // if it gets dirty meaning it is found here this function
    // returns 1, Otherwise false will be returned.
    CACHE_TAG tag;
    UINT32 setIndex;
    ///cerr << "in CACHE::SetDirty\n";
    SplitAddress(addr, tag, setIndex);
    SET & set = _sets[setIndex];
    // if (!set.Find(tag,ACCESS_TYPE_STORE))
    //	cerr <<"!! "<< GetName() << addr<< endl;
    ///cerr << GetName() << "addr: " << addr << " tag: " << tag << "{\n";
    //assert(set.Find(tag,ACCESS_TYPE_LOAD));
    if (set.Find(tag,ACCESS_TYPE_LOAD))
    {
        //it is found, make it dirty
        set.SetDirty(tag, ACCESS_TYPE_LOAD);
        return (true);
    } else
        return (false);
    ///cerr << "}\n";
};

// define shortcuts
#define CACHE_DIRECT_MAPPED(MAX_SETS, ALLOCATION) CACHE<CACHE_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_LRU(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::LRU<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

#endif // PIN_CACHE_H
