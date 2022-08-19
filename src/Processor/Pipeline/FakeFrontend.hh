#ifndef __FAKEFRONTEND_HH__
#define __FAKEFRONTEND_HH__

#include "Pipe_data.hh"
#include "BaseStage.hh"

namespace Emulator
{
    class FakeFrontend : public BaseStage
    {
    public:
        FakeFrontend()
        {
            // Initilize it just like the default model stage
        }
        ~FakeFrontend() {}

        void Set_StageOutPort(const InsnPkg_t &insn_pkg)
        {
            this->m_StageOutPort->set(insn_pkg);
        }

        void ConnectWith(...) {}
    };

}

#endif