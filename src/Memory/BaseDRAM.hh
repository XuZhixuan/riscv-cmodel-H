#ifndef __BASEDRAM_HH__
#define __BASEDRAM_HH__

#include <stdint.h>
#include <iostream>
#include <cstring>
#include "../Trace/Logging.hh"

namespace Emulator
{

    class BaseDRAM : public Trace::TraceObject
    {

        struct ReservationEntry
        {
            uint64_t addr;
            bool valid;
        };

    private:
        uint64_t m_base;
        uint64_t m_length;
        char *m_ram;
        uint64_t m_ioBase;
        char *m_io;
        uint64_t m_holeEnd;
        char *m_hole;

    public:
        ReservationEntry g_reservationSet;

    public:
        BaseDRAM();
        BaseDRAM(const uint64_t base, const uint64_t length);
        ~BaseDRAM();
        const uint64_t &size();
        bool checkRange(uint64_t address);
        void read(uint64_t address, char **data, uint64_t len);
        void read_c(uint64_t address, char *data, uint64_t len);
        uint64_t readDouble(uint64_t address);
        char readByte(uint64_t address);
        void write(uint64_t address, const char *data, const uint64_t len);
        void writeDouble(uint64_t address, const uint64_t data);
        void write(uint64_t address, const char *data, const uint64_t len, const uint64_t mask);

        void readIO(uint64_t address, char *data, uint64_t len);
        void writeIO(uint64_t address, const char *data, const uint64_t len, const uint64_t mask);
        bool checkIORange(uint64_t address);

        void readHole(uint64_t address, char *data, uint64_t len);
        void writeHole(uint64_t address, const char *data, const uint64_t len, const uint64_t mask);
        bool checkHoleRange(uint64_t address);
    };

} // namespace Emulator

#endif
