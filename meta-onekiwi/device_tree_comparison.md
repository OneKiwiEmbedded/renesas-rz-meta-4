# Device Tree Comparison (OneKiwi RZ/V2H SOM)

This document provides a detailed comparison between the base SoC files and the SOM device tree files across different Linux kernel versions (5.10 Dunfell vs 6.1 Scarthgap).

---

## 1. `r9a09g057.dtsi` vs `onekiwi-r9a09g057.dtsi` (Linux 5.10)

The original 5.10 Linux SDK included the standard `r9a09g057.dtsi` SoC file. The OneKiwi team created a custom `onekiwi-r9a09g057.dtsi` copy with the following exact additions and modifications:

1. **Modified CRU Interrupts:**
   - Changed `cru0`, `cru1`, `cru2`, and `cru3` nodes to have dual interrupts (`cru_vsd_addr_wend` and `axi_mst_err_int`) instead of the default single interrupt.

2. **Added Custom Camera/ISP Nodes:**
   The following proprietary Image Signal Processor blocks were added to the SoC definition:
   - `input_video_control: input_video_control@16040000`
   - `isp: isp@16080000`
   - `sensor: soc_sensor@16284000`
   - `lens: soc_lens@16280000`
   - `iq: soc_iq@16286000`

> [!NOTE]
> In Linux 6.1, Renesas entirely removed these proprietary ISP bindings from their drivers, requiring camera pipelines to run directly through the V4L2 `cru` nodes. This is why `onekiwi-r9a09g057.dtsi` had to be removed for the Scarthgap migration.

---

## 2. `onekiwi-rzv2h-8gb.dts` (Linux 6.1 Scarthgap) vs (Linux 5.10 Dunfell)

To successfully compile the 8GB SOM device tree on Linux 6.1, several breaking changes from the Renesas BSP 6.1 update were resolved:

1. **Base SoC Include Changed:**
   - **5.10:** `#include "onekiwi-r9a09g057.dtsi"`
   - **6.1:** `#include "r9a09g057.dtsi"` (Due to the deprecation of the custom file).

2. **Standard Label Renames (Renesas 6.1 changes):**
   - **5.10:** `&extal_clk` ➔ **6.1:** `&qextal_clk`
   - **5.10:** `&scif0` ➔ **6.1:** `&scif` (also updated alias `serial0 = &scif;`)
   - **5.10:** `&spi1` ➔ **6.1:** `&rsci_spi1`

3. **DRP Memory Label Fixes:**
   - In 5.10, the DRP-AI block (`&drp1`) used `memory-region = <&codec0>;` and `memory-oca-region = <&opencva0>;`. 
   - In 6.1, because we removed the custom SoC file, these missing labels were correctly pointed to the explicitly defined memory regions within the 8GB `.dts` file:
     - `<&codec0>` ➔ `<&drp_codec>`
     - `<&opencva0>` ➔ `<&opencva_reserved>`

4. **Camera/ISP Nodes Disabled:**
   - **5.10:** Enabled `&input_video_control`, `&iq`, `&isp`, `&lens`, `&sensor` nodes with IMX415 configuration.
   - **6.1:** These block overrides were commented out because their base driver definitions no longer exist in the 6.1 kernel. The IMX415 I2C definitions and `&csi20`/`&csi21` receivers were preserved, but the pipeline requires refactoring to the new V4L2 `cru` standard.
