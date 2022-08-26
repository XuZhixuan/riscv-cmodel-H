#pragma once

#include "../../Common/Common.hh"
#include "../../Trace/Logging.hh"
#include "../Pipeline/Pipe_data.hh"

namespace Emulator
{
    /**
     * @brief Base cache port class contains time buffer and basic functions.
     */
    class BaseCachePort : public Trace::TraceObject
    {
    private:
        TimeBuffer<std::pair<std::function<void(MemResp_t)>, MemResp_t>> m_CacheRespLatch;

    public:
        /**
         * @brief Constructor of Base Cache Port
         *
         * @param latency latency of the port
         */
        BaseCachePort(std::string name, uint64_t latency)
            : TraceObject(name), m_CacheRespLatch(name, latency)
        {
        }

        ~BaseCachePort();

        /**
         * @brief Receive memory request.
         *
         * @param request memory request.
         * @param callback callback function for response.
         */
        virtual void recvMemReq(MemReq_t request, std::function<void(MemResp_t)> callback) = 0;

        /**
         * @brief basic simulation function
         */
        void reset() { m_CacheRespLatch.reset(); }

        /**
         * @brief basic simulation function
         */
        void evaluate()
        {
            if (m_CacheRespLatch.OutPort->valid)
                m_CacheRespLatch.OutPort->data.first(m_CacheRespLatch.OutPort->data.second);
        }

        /**
         * @brief basic simulation function
         */
        void advance() { m_CacheRespLatch.advance(); }
    };
}
