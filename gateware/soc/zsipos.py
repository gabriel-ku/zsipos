#!/usr/bin/env python3

# This file is Copyright (c) 2015-2019 Florent Kermarrec <florent@enjoy-digital.fr>
# License: BSD

import argparse

from litex.soc.cores.clock import *
from litex.soc.integration.soc_sdram import *
from litex.soc.integration.builder import *

from litedram.modules import IM4G08D3FABG125
from litedram.phy import s7ddrphy

from liteeth.phy.mii import LiteEthPHYMII
from liteeth.mac import LiteEthMAC

from platform import Platform
from dts import *
from litex.soc.cores.cpu.rocket2 import Rocket64

# CRG ----------------------------------------------------------------------------------------------

class _CRG(Module):
    def __init__(self, platform, sys_clk_freq):
        self.clock_domains.cd_sys = ClockDomain()
        self.clock_domains.cd_sys4x = ClockDomain(reset_less=True)
        self.clock_domains.cd_sys4x_dqs = ClockDomain(reset_less=True)
        self.clock_domains.cd_clk200 = ClockDomain()

        # # #

        self.cd_sys.clk.attr.add("keep")
        self.cd_sys4x.clk.attr.add("keep")
        self.cd_sys4x_dqs.clk.attr.add("keep")

        self.submodules.pll = pll = S7MMCM(speedgrade=-2)

        self.comb += pll.reset.eq(platform.request("cpu_reset"))

        pll.register_clkin(platform.request("clk100"), 100e6)
        pll.create_clkout(self.cd_sys, sys_clk_freq)
        pll.create_clkout(self.cd_sys4x, 4*sys_clk_freq)
        pll.create_clkout(self.cd_sys4x_dqs, 4*sys_clk_freq, phase=90)
        pll.create_clkout(self.cd_clk200, 200e6)

        self.submodules.idelayctrl = S7IDELAYCTRL(self.cd_clk200)

# BaseSoC ------------------------------------------------------------------------------------------

class BaseSoC(SoCSDRAM):
    def __init__(self, sys_clk_freq=int(60e6), **kwargs):
        platform = Platform()
        SoCSDRAM.__init__(self, platform, clk_freq=sys_clk_freq,
                          cpu_type="rocket64", cpu_variant="linux",
                          integrated_rom_size=0x8000,
                          integrated_sram_size=0x8000,
                          l2_size=0, **kwargs)

        self.submodules.crg = _CRG(platform, sys_clk_freq)

        # sdram
        self.submodules.ddrphy = s7ddrphy.A7DDRPHY(platform.request("ddram"), sys_clk_freq=sys_clk_freq)
        self.add_csr("ddrphy")
        sdram_module = IM4G08D3FABG125(sys_clk_freq, "1:4")
        self.register_sdram(self.ddrphy,
                            sdram_module.geom_settings,
                            sdram_module.timing_settings)

# EthernetSoC --------------------------------------------------------------------------------------

class EthernetSoC(BaseSoC):
    mem_map = {
        "ethmac": 0x30000000,  # (shadow @0xb0000000)
    }
    mem_map.update(BaseSoC.mem_map)

    def __init__(self, **kwargs):
        BaseSoC.__init__(self, **kwargs)

        self.submodules.ethphy = LiteEthPHYMII(self.platform.request("eth_clocks"),
                                               self.platform.request("eth"))
        self.add_csr("ethphy")
        self.submodules.ethmac = LiteEthMAC(phy=self.ethphy, dw=32,
            interface="wishbone", endianness=self.cpu.endianness)
        self.add_wb_slave(self.mem_map["ethmac"], self.ethmac.bus, 0x2000)
        self.add_memory_region("ethmac", self.mem_map["ethmac"] | self.shadow_base, 0x2000)
        self.add_csr("ethmac")
        self.add_interrupt("ethmac")

        self.ethphy.crg.cd_eth_rx.clk.attr.add("keep")
        self.ethphy.crg.cd_eth_tx.clk.attr.add("keep")
        self.platform.add_period_constraint(self.ethphy.crg.cd_eth_rx.clk, 1e9/25e6)
        self.platform.add_period_constraint(self.ethphy.crg.cd_eth_tx.clk, 1e9/25e6)
        self.platform.add_false_path_constraints(
            self.crg.cd_sys.clk,
            self.ethphy.crg.cd_eth_rx.clk,
            self.ethphy.crg.cd_eth_tx.clk)

        pwdn = self.platform.request("eth_pwdn")
        self.comb += pwdn.eq(1)

    def get_dts(self):
        d = DTSHelper(self)
        d.add_litex_uart(0, "uart")
        d.add_litex_eth(0, "ethphy", "ethmac")
        s = Rocket64.build_dts(variant=d.get_cpu_variant(),
                               bootargs="",
                               sdram_size=d.get_sdram_size(),
                               timebase_frequency=d.get_sys_clk_freq(),
                               devices=d.get_devices())
        return s

    def write_dts(self, dts_file):
        with open(dts_file, "w") as f:
            f.write(self.get_dts())

# Build --------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="LiteX SoC on ZSIPOS")
    builder_args(parser)
    soc_sdram_args(parser)
    dtshelper_args(parser)
    args = parser.parse_args()
    soc = EthernetSoC(**soc_sdram_argdict(args))
    builder = Builder(soc, **builder_argdict(args))
    builder.build()
    if args.dts_file:
        soc.write_dts(args.dts_file)

if __name__ == "__main__":
    main()
