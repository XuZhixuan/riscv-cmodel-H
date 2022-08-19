#include "TLB.hh"

namespace Emulator
{
    BaseTLB::BaseTLB(Processor *processor, uint16_t entry_num, uint8_t way)
        : processor(processor), entry_num(entry_num), entries(entry_num), way(way), lruSeq(0), TraceObject("TLB")
    {
        for (size_t i = 0; i < entry_num; i++)
        {
            free.push_back(&entries[i]);
        }
    }

    TLBEntryPtr BaseTLB::lookup(Addr vpn, uint16_t asid, bool hidden)
    {
        TLBEntryPtr entry = nullptr;

        for (auto &&e : entries)
        {
            if (e.vpn == vpn && e.asid == asid && e.valid)
                entry = &e;
        }

        if (!hidden)
        {
            if (entry)
                entry->lruSeq = nextSeq(); // do LRU count

            DPRINTF(TLBVerbose, "lookup(vpn=%#x, asid=%#x): %s ppn %#x\n",
                    vpn, asid, entry ? "hit" : "miss", entry ? entry->ppn : 0);
        }

        return entry;
    }

    void BaseTLB::remove(size_t idx)
    {
        entries[idx].valid = false;
        free.push_back(&entries[idx]);
    }

    void BaseTLB::evictLRU()
    {
        size_t lru = 0;
        for (size_t i = 0; i < entry_num; i++)
        {
            if (entries[i].lruSeq < entries[lru].lruSeq)
                lru = i;
        }

        remove(lru);
    }

    TLBEntryPtr BaseTLB::insert(Addr vpn, const TLBEntry &entry)
    {
        DPRINTF(TLB, "insert(vpn=%#x, asid=%#x): ppn=%#x\n", vpn, entry.asid, entry.ppn);

        TLBEntry *new_entry = lookup(vpn, entry.asid, true);
        if (new_entry)
        {
            // why could this happen ?
            return new_entry;
        }

        if (isFull())
            evictLRU();

        new_entry = free.front();
        free.pop_front();

        *new_entry = entry;
        new_entry->lruSeq = nextSeq();
        new_entry->vpn = vpn;

        return new_entry;
    }

    void BaseTLB::flushAll()
    {
        for (auto &&e : entries)
        {
            e.valid = false;
            free.push_back(&e);
        }
    }

    bool BaseTLB::TLB_LookUp(Addr vaddr, uint16_t asid, Addr *paddr)
    {
        Addr vpn = vaddr & vpn_mask;
        Addr offset = vaddr & off_mask;
        TLBEntryPtr e = lookup(vpn, asid);

        if (e)
        {
            *paddr = e->ppn | offset;
            return true;
        }
        
        return false;
    }

    bool BaseTLB::TLB_Insert(Addr vaddr, uint16_t asid, Addr paddr)
    {
        Addr vpn = vaddr & vpn_mask;
        Addr ppn = paddr & ppn_mask;

        TLBEntry e(vpn, ppn, asid, 0, true);

        insert(vpn, e);

        return true;
    }
}
