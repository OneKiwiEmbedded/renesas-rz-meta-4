# Linux 6.1 Device Tree Migration Notes (OneKiwi RZ/V2H SOM)

This document outlines the modifications required to migrate the OneKiwi RZ/V2H SOM device trees (`onekiwi-rzv2h-8gb.dts`, `16gb.dts`, `dev.dts`) from the Linux 5.10 (Dunfell) SDK to the Linux 6.1 (Scarthgap) SDK.

## 1. Removed Custom SoC File
- **Deleted:** `onekiwi-r9a09g057.dtsi`
- **Reason:** In Linux 6.1, Renesas removed the CPG macro definitions (e.g., `R9A09G057_ADC_ADCLK`) and moved to raw hex bindings (`<&cpg 0x107>`). The custom 5.10 SoC `.dtsi` file is incompatible with the 6.1 kernel compiler.
- **Solution:** Switched the `#include "onekiwi-r9a09g057.dtsi"` to the official `#include "r9a09g057.dtsi"` provided by the 6.1 kernel.

## 2. Renamed Standard Nodes
The Renesas 6.1 kernel base definitions renamed several standard node labels. The following overrides were updated to match the new 6.1 bindings:
- `&extal_clk` ➔ `&qextal_clk`
- `&scif0` ➔ `&scif` (also updated `serial0 = &scif;` in `aliases`)
- `&spi1` ➔ `&rsci_spi1`

## 3. Fixed DRP Reserved Memory Labels
- **Modified:** `&drp1` node memory regions.
- **Reason:** In 5.10, the DRP memory regions were aliased as `codec0` and `opencva0`. Since the custom `.dtsi` file was removed, these aliases no longer exist, throwing a "non-existent node" error.
- **Solution:** Updated `&drp1` to point directly to the actual memory regions defined in the SOM files:
  - `<&codec0>` ➔ `<&drp_codec>`
  - `<&opencva0>` ➔ `<&opencva_reserved>`

## 4. Disabled Deprecated Camera/ISP Nodes
- **Commented out:** `&input_video_control`, `&iq`, `&isp`, `&lens`, `&sensor`
- **Reason:** In Linux 5.10, Renesas used a custom ISP tuning driver architecture with these specific device tree nodes. In Linux 6.1, Renesas fully transitioned to the standard Video4Linux2 (V4L2) framework utilizing the Camera Receiver Unit (`cru`). As a result, the `isp` and `iq` nodes no longer exist in the 6.1 SoC definitions. 

## Reference for Future Camera Porting
To restore full camera ISP functionality, the camera subsystem device tree definitions need to be rewritten to target the new V4L2 `cru` architecture.

**Reference Material:**
Please refer to the official Renesas Evaluation Kit (EVK) device tree included in the 6.1 kernel source for an example of how the new `cru` and camera nodes are configured:
- `arch/arm64/boot/dts/renesas/r9a09g057h44-rzv2h-evk.dts`
- `arch/arm64/boot/dts/renesas/r9a09g057h44-rzv2h-evk-ver2.dts`
