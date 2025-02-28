/*
 * Copyright 2019 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#ifndef __ARCH_ARM_FASTMODEL_GIC_GIC_HH__
#define __ARCH_ARM_FASTMODEL_GIC_GIC_HH__

#include <amba_pv.h>

#include "arch/arm/fastmodel/amba_ports.hh"
#include "dev/arm/base_gic.hh"
#include "params/FastModelGIC.hh"
#include "params/SCFastModelGIC.hh"
#include "scx_evs_GIC.h"
#include "systemc/ext/core/sc_module_name.hh"
#include "systemc/sc_port_wrapper.hh"

namespace FastModel
{

// The fast model exports a class called scx_evs_GIC which represents
// the subsystem described in LISA+. This class specializes it to export gem5
// ports and interface with its peer gem5 GIC. The gem5 GIC inherits from the
// gem5 BaseGic class and implements its API, while this class actually does
// the work.
class SCGIC : public scx_evs_GIC
{
  public:
    SCGIC(const SCFastModelGICParams &params, sc_core::sc_module_name _name);

    SignalInterruptInitiatorSocket signalInterrupt;

    void
    end_of_elaboration() override
    {
        scx_evs_GIC::end_of_elaboration();
        scx_evs_GIC::start_of_simulation();
    }
    void start_of_simulation() override {}
};

// This class pairs with the one above to implement the receiving end of gem5's
// GIC API. It acts as an interface which passes work to the fast model GIC,
// and lets the fast model GIC interact with the rest of the system.
class GIC : public BaseGic
{
  private:
    typedef sc_gem5::TlmInitiatorBaseWrapper<
        64, svp_gicv3_comms::gicv3_comms_fw_if,
        svp_gicv3_comms::gicv3_comms_bw_if, 1,
        sc_core::SC_ONE_OR_MORE_BOUND> TlmGicInitiator;

    AmbaInitiator ambaM;
    AmbaTarget ambaS;
    TlmGicInitiator redistributor;

    SCGIC *scGIC;

  public:
    GIC(const FastModelGICParams &params);

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    void sendInt(uint32_t num) override;
    void clearInt(uint32_t num) override;

    void sendPPInt(uint32_t num, uint32_t cpu) override;
    void clearPPInt(uint32_t num, uint32_t cpu) override;

    AddrRangeList getAddrRanges() const override { return AddrRangeList(); }
    Tick read(PacketPtr pkt) override { return 0; }
    Tick write(PacketPtr pkt) override { return 0; }
};

} // namespace FastModel

#endif // __ARCH_ARM_FASTMODEL_GIC_GIC_HH__
