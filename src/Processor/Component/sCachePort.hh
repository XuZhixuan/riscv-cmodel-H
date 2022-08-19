#pragma once

#include <functional>
#include "../Pipeline/Pipe_data.hh"
#include "../../Common/Common.hh"

namespace Emulator
{
    /**
     * @brief A simple port for timing in caches like TLB.
     */
    class sCachePort : public Trace::TraceObject
    {
    protected:
        TimeBuffer<std::pair<std::function<void(MemResp_t)>, MemResp_t>> m_sCacheRespLatch;

    public:
        sCachePort(uint64_t latency);

        ~sCachePort();

        void receiveMemReq(MemReq_t request, std::funcion<void(MemResp_t)> callback);

        void Reset();

        void Evaluate();
        
        void Advance();
    }
}
