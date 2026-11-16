FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += " \
    file://onekiwi-rzv2h-8gb.dts \
    file://onekiwi-rzv2h-16gb.dts \
    file://onekiwi-rzv2h-dev.dts \
    file://ubuntu-features.cfg \
    file://udl.cfg \
    file://imx415.c \
    file://IMX415_drv.c \
    file://IMX415_seq.h \
    file://IMX415_config.h \
    file://patch_cru.py \
"

do_configure:prepend() {
    cp ${WORKDIR}/onekiwi-*.dts ${S}/arch/arm64/boot/dts/renesas/

    if ! grep -q "onekiwi-rzv2h-8gb.dtb" ${S}/arch/arm64/boot/dts/renesas/Makefile; then
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-8gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-16gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-dev.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
    fi

    # Dynamically patch CRU driver files to expand max_width to 4095 and RAW10 packing
    python3 ${WORKDIR}/patch_cru.py ${S}

    # Override IMX415 sensor driver in kernel with IMX412 code
    cp ${WORKDIR}/imx415.c ${S}/drivers/media/i2c/imx415.c

    # Override ISP sensor driver wrapper in kernel with IMX412 configuration
    ISP_SENSOR_DIR=${S}/drivers/media/platform/renesas/rzv2h-isp/subdev/sensor/src/driver/sensor
    mkdir -p ${ISP_SENSOR_DIR}
    cp ${WORKDIR}/IMX415_drv.c ${ISP_SENSOR_DIR}/
    cp ${WORKDIR}/IMX415_seq.h ${ISP_SENSOR_DIR}/
    cp ${WORKDIR}/IMX415_config.h ${ISP_SENSOR_DIR}/
}
