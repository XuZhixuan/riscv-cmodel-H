#pragma once

#include "../../Common/Common.hh"
#include "../../Trace/TraceObject.hh"

namespace Emulator
{
    typedef uint64_t Addr;
    class Processor;

    typedef struct TLBEntry
    {
        uint64_t vpn;    // Virtual Page Number.
        uint64_t ppn;    // Physical Page Number.
        uint16_t asid;   // Address Space ID.
        uint64_t lruSeq; // sequence number to keep track of LRU.
        bool valid;      // If this entry valid.

        TLBEntry() : vpn(0), ppn(0), lruSeq(0) {}
        TLBEntry(Addr vpn, Addr ppn, uint16_t asid, uint64_t lruSeq, bool valid)
            : vpn(vpn), ppn(ppn), asid(asid), lruSeq(lruSeq), valid(valid)
        {
        }

    } * TLBEntryPtr; // TLB entry struct pointer.

    /**
     * @brief Base TLB class
     */
    class BaseTLB : public Trace::TraceObject
    {
        Processor *m_Processor;

    public:
        /**
         * @param processor pointer to processor.
         * @param entry_num num of tle entries.
         * @param way of set-associative.
         **/
        BaseTLB(Processor *processor, uint16_t entry_num = 64, uint8_t way = 1);

        ~BaseTLB();

    protected:
        uint64_t lruSeq;    // LRU sequence
        uint16_t entry_num; // number of TLB entries.
        uint8_t way;        // num of associative way.

        static const Addr vpn_mask = 0x007ffffff000;
        static const Addr ppn_mask = 0x00fffffffffff000;
        static const Addr off_mask = 0x0fff;

        Processor *processor;

        std::vector<TLBEntry> entries; // TLB entries
        std::list<TLBEntryPtr> free;   // free TLB entries

        // *** Internal APIs here ***
    protected:
        /**
         * @brief get next LRU sequence.
         *
         * @return uint64_t next LRU Sequence.
         */
        uint64_t nextSeq() { return ++lruSeq; }

        /**
         * @brief insert an new entry to TLB.
         *
         * @param vpn Virtual Page Number.
         * @param entry Reference to tne entry.
         *
         * @return TLBEntryPtr pointer to the tlb entry.
         */
        TLBEntryPtr insert(Addr vpn, const TLBEntry &entry);

        /**
         * @brief do a TLB lookup.
         *
         * @param vpn Virtual Page Number.
         * @param asid Address Space ID.
         * @param hidden determine to hide this lookup from count, default false.
         *
         * @return TLBEntryPtr pointer to the TLB entry looing for.
         */
        TLBEntryPtr lookup(Addr vpn, uint16_t asid, bool hidden = false);

        /**
         * @brief remove an entry.
         *
         * @param idx the id of entry to remove.
         */
        void remove(size_t idx);

        /**
         * @brief find and remove an entry.
         */
        void evictLRU();

        // *** APIs down here ***
    public:
        /**
         * @brief exam if the TLB is full.
         *
         * @return true if fulled.
         */
        bool isFull() { return free.empty(); }

        /**
         * @brief flush all entries in TLB.
         */
        void flushAll();

        /**
         * @brief API - Look up the physical address, only SV39 now.
         *
         * @param vaddr virtual address.
         * @param asid address space ID.
         * @param paddr pointer to physical address write to.
         *
         * @return bool true if look up hit.
         */
        bool TLB_LookUp(Addr vaddr, uint16_t asid, Addr *paddr);

        /**
         * @brief API - Insert a new TLB entry
         *
         * @param vaddr virtual address.
         * @param asid address space ID.
         * @param paddr physical address.
         *
         * @return bool true if insert successfully.
         */
        bool TLB_Insert(Addr vaddr, uint16_t asid, Addr paddr);
    };
}
