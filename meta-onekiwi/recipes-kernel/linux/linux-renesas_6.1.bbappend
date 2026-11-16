FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += " \
    file://onekiwi-rzv2h-4gb.dts \
    file://onekiwi-rzv2h-8gb.dts \
    file://onekiwi-rzv2h-16gb.dts \
    file://onekiwi-rzv2h-dev.dts \
    file://udl.cfg \
    file://ubuntu-features.cfg \
    file://0003-fix-v4l2-async-null-deref.patch \
    file://imx415.c \
    file://IMX415_drv.c \
    file://IMX415_seq.h \
    file://IMX415_config.h \
    file://isp_config_seq.h \
    file://patch_cru.py \
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

    # Dynamically patch CRU driver files to support IMX412 on top of Renesas ISP modifications
    python3 ${WORKDIR}/patch_cru.py ${S}

    # Override IMX415 sensor driver in kernel with IMX412 code
    cp ${WORKDIR}/imx415.c ${S}/drivers/media/i2c/imx415.c

    # Override ISP sensor driver wrapper in kernel with IMX412 configuration
    ISP_SENSOR_DIR=${S}/drivers/media/platform/renesas/rzv2h-isp/subdev/sensor/src/driver/sensor
    cp ${WORKDIR}/IMX415_drv.c ${ISP_SENSOR_DIR}/
    cp ${WORKDIR}/IMX415_seq.h ${ISP_SENSOR_DIR}/
    cp ${WORKDIR}/IMX415_config.h ${ISP_SENSOR_DIR}/

    # Copy IMX412 ISP configuration & Color Correction Matrix (CCM)
    ISP_FWLIB_DIR=${S}/drivers/media/platform/renesas/rzv2h-isp/v4l2_dev/src/fw_lib
    mkdir -p ${ISP_FWLIB_DIR}
    cp ${WORKDIR}/isp_config_seq.h ${ISP_FWLIB_DIR}/
}
