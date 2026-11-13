# meta-onekiwi

Board support for the OneKiwi RZ/V2H SOM on top of Renesas `meta-rz-bsp` / `meta-rz-distro`
(RZ/V2H AI SDK v6.00, Yocto scarthgap, Linux 6.1.141-cip43).

`meta-rz-bsp` and `meta-rz-distro` are unmodified Renesas layers. Everything OneKiwi-specific is in this layer.

## Machines

| MACHINE | RAM | Template |
|---|---|---|
| `onekiwi-rzv2h-4gb` | 4GB (2 x 2GB banks) | `conf/templates/onekiwi-rzv2h-4gb` |
| `onekiwi-rzv2h-8gb` | 8GB | `conf/templates/onekiwi-rzv2h-8gb` |

## Sources

| Component | Repository | Branch / revision |
|---|---|---|
| Linux | `OneKiwiEmbedded/renesas-rz-linux-4` | `rz-6.1-cip43` |
| U-Boot | `OneKiwiEmbedded/renesas-rz-uboot-4` | `v2024.07/rz_dev_v2h` |
| TF-A | `OneKiwiEmbedded/renesas-rz-atf-4` | `8160111194f7` (branch `2.10.5/rzv2h_1.1.0`) |

## Layer contents

| Path | Purpose |
|---|---|
| `conf/machine/` | Machine configurations |
| `conf/templates/` | `bblayers.conf` / `local.conf` per board (copy into `build/conf/`) |
| `recipes-kernel/linux/linux-renesas_6.1.bbappend` | Kernel source for OneKiwi boards |
| `recipes-bsp/u-boot/u-boot_2024.07.bbappend` | U-Boot source for OneKiwi boards |
| `recipes-bsp/trusted-firmware-a/trusted-firmware-a_2.10.bbappend` | TF-A source for OneKiwi boards (`TFA_BOARD` is set in the machine conf, currently `evk_1`) |
| `dynamic-layers/meta-rz-isp/` | Allow the meta-rz-isp recipes on OneKiwi machines (used only when meta-rz-isp is present) |
| `recipes-rtl8821au/` | RTL8821AU USB Wi-Fi driver (add `rtl8821au` to `IMAGE_INSTALL` to use) |
| `recipes-userfiles/` | Sample audio files in `/home/root` (add `userfiles` to `IMAGE_INSTALL` to use) |

## Build

```bash
source poky/oe-init-build-env build
cp ../meta-renesas/meta-onekiwi/conf/templates/onekiwi-rzv2h-8gb/*.conf conf/
bitbake core-image-weston
```
For 4GB use `onekiwi-rzv2h-4gb`.
