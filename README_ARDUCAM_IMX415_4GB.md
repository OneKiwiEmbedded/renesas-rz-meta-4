# meta-renesas: Ubuntu 4GB với camera Arducam IMX415 (biến thể của bản IMX412)

Lập 2026-11-18. **Chưa build, chưa test trên board.**

Lịch sử phần Arducam:
- SDK 5.2 (kernel 5.10): đã sửa và **test chạy được** với Arducam IMX415.
- SDK 6.0 (kernel 6.1 + ISP): port lại từ 5.2 (merge 3 chiều vào driver IMX415 của `meta-rz-isp`), **chưa test trên board**.
- Bản Ubuntu này lấy đúng phần SDK 6.0 đó, nên lần test này cũng là lần đầu chạy Arducam trên kernel 6.1 + ISP.
- Renesas gốc chỉ test với module IMX415 của Renesas (INCK 74.25 MHz, 891 Mbps, địa chỉ `0x1a`), không phải Arducam.

Nếu lỗi, so với bản 5.2 đã chạy (`SOM_5.2_V2H_dunfell/meta-renesas-imx415-arduicam`): `imx415.c` (bảng clock 24 MHz, mode 720 Mbps, thứ tự bật nguồn/XCLR), DTS (`0x37`, `P3_0`, `P3_1`, link-frequencies), `rzg2l-csi2.c` (tốc độ D-PHY).

Bản này giống hệt `UBUNTU_SOM/claude/meta-renesas` (Ubuntu 24.04, board 4GB, TF-A `onekiwi_8gb`, U-Boot 4GB, HDMI ADV7535 2 lane), chỉ đổi camera **IMX412 (gremsy, 600MHz)** thành **Arducam IMX415**. Phần camera lấy từ bản Yocto SDK 6.0 Arducam (`renesas-rz-meta-4`, nhánh `sdk60-imx415-arducam-adv-4gb`), đã chạy với kernel 6.1 + ISP.

**Chỉ dùng với kernel 6.1** (`PREFERRED_PROVIDER_virtual/kernel = "linux-renesas"` trong `local.conf`). Phần kernel 6.10 (`linux-rz`) trong meta này vẫn là IMX412 và không có ISP.

## Khác bản IMX412

| | Bản IMX412 (`meta-renesas`) | Bản Arducam (`meta-renesas-arducam-4g`) |
|---|---|---|
| Driver cảm biến `imx415.c` | Code IMX412 gremsy chép đè vào kernel | Driver IMX415 của Renesas (`meta-rz-isp`) + patch Arducam: INCK 24 MHz, 4 lane x 720 Mbps, 25 fps |
| ISP `IMX415_drv.c`, `IMX415_seq.h`, `IMX415_config.h` | Bản IMX412 gremsy | Bản Renesas, `LANE_MBPS 720` |
| `isp_config_seq.h` (CCM) | CCM IMX412 gremsy | Bản Renesas (IMX415) |
| `patch_cru.py` (sửa CRU cho IMX412) | Có | Không dùng |
| `rzg2l-csi2.c` | — | Ép D-PHY 720 Mbps khi tính ra 730..750 Mbps |
| `rzg2l-dma.c` | — | Chờ 400 ms khi dừng CRU (như SDK 5.2) |
| DTS camera (`i2c6_csi0`) | `imx415@1a`, reset `P3_1`, link 600 MHz | `imx415@37`, reset `P3_0`, link 360 MHz (720 Mbps), clock 24 MHz |
| `P3_1` | reset camera | **gpio-hog bật nguồn camera** (`cam0_pwr_en`) |
| ISP `input_video_control` | `crus = <&cru0>`, `cru1` tắt | `crus = <&cru0 &cru1>` (như SDK 5.2 / 6.0) |

## File thay đổi (nhánh git `arducam-imx415-4gb`)

- `meta-onekiwi/recipes-kernel/linux/linux-renesas_6.1.bbappend`: bỏ phần chép đè driver IMX412, chỉ giữ DTS + các file `.cfg` + `0003-fix-v4l2-async-null-deref.patch`.
- `meta-onekiwi/dynamic-layers/meta-rz-isp/recipes-kernel/linux/linux-renesas_6.%.bbappend`: áp `0001-imx415-arducam-24mhz-720mbps-driver.patch` ở cuối `do_patch` (sau các patch của `meta-rz-isp`).
- `.../files/0001-imx415-arducam-24mhz-720mbps-driver.patch`: phần driver của patch Arducam SDK 6.0 (bỏ phần DTS). Đã kiểm khớp với cây kernel 6.1 + 13 patch ISP.
- `meta-onekiwi/recipes-kernel/linux/linux-renesas/onekiwi-rzv2h-4gb.dts`: phần camera như bảng trên.

## Build trên server

Giống bản IMX412, chỉ thay thư mục meta:
```bash
# giai đoạn 1 (docker)
cd ~/yocto
mv meta-renesas meta-renesas-imx412          # giữ bản IMX412
# chép meta-renesas-arducam-4g.tgz lên, giải nén, đổi tên thành meta-renesas
grep PREFERRED_PROVIDER_virtual/kernel build/conf/local.conf     # phải là linux-renesas
source poky/oe-init-build-env build
bitbake -c cleansstate linux-renesas
bitbake core-image-weston
# log do_patch phải có: "onekiwi: applying 0001-imx415-arducam-24mhz-720mbps-driver.patch"

# giai đoạn 2 (host): như bản IMX412
#   - chép rootfs mới thành rz-cmn-srp/renesas-ubuntu.tar.bz2
#   - thay yocto_rzcmn_board/meta-renesas, giữ templates
#   - đổi tên artifacts_rootfs_source cũ
#   - sudo MACHINE=onekiwi-rzv2h-4gb IMAGE=ubuntu-core ./rz_builder.sh build
```
Bootloader không đổi: dùng bản đang chạy trên board.

## Test trên board

```bash
dmesg | grep -iE "imx415|i2c not ok"     # không còn lỗi I2C ở địa chỉ 0x37
ls -l /dev/ac_isp /dev/video*fr
bash /root/v4l2-init.sh                  # bản mẫu IMX415 của meta-rz-isp có trong image
```
Tool chỉnh màu (act-server) dùng giống bản IMX412.
