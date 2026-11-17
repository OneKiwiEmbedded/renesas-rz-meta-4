FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Arducam IMX415 variant: the kernel uses the Renesas IMX415 driver and ISP sensor
# files from meta-rz-isp, plus the Arducam driver patch applied in
# dynamic-layers/meta-rz-isp (24 MHz INCK, 720 Mbps). The IMX412 (gremsy) driver
# overrides of the IMX412 meta are NOT used here.
SRC_URI += " \
    file://onekiwi-rzv2h-4gb.dts \
    file://onekiwi-rzv2h-8gb.dts \
    file://onekiwi-rzv2h-16gb.dts \
    file://onekiwi-rzv2h-dev.dts \
    file://udl.cfg \
    file://ubuntu-features.cfg \
    file://onekiwi-rzv2h.cfg \
    file://0003-fix-v4l2-async-null-deref.patch \
"

do_configure:prepend() {
    # Copy custom devicetrees into the kernel source tree
    cp ${WORKDIR}/onekiwi-*.dts ${S}/arch/arm64/boot/dts/renesas/

    # Inject them into the Makefile so the kernel build system knows about them
    if ! grep -q "onekiwi-rzv2h-8gb.dtb" ${S}/arch/arm64/boot/dts/renesas/Makefile; then
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-8gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-16gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-dev.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
    fi
    # 4GB board (separate check so it is also added when the Makefile already has the 8GB entries)
    if ! grep -q "onekiwi-rzv2h-4gb.dtb" ${S}/arch/arm64/boot/dts/renesas/Makefile; then
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-4gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
    fi
}
