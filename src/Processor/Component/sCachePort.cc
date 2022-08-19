#pragma once

#include "sCachePort.hh"

namespace Emulator
{
    sCachePort::sCachePort(uint64_t latency)
        : TraceObject("sCachePort")
        : m_sCacheRespLatch("sCache", latency)
    {
    }

    sCachePort::receiveMemReq(MemReq_t request, std::funcion<void(MemResp_t)> callback)
    {
        //
    }

}
