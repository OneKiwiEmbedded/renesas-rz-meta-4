FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += " \
    file://onekiwi-rzv2h-4gb.dts \
    file://onekiwi-rzv2h-8gb.dts \
    file://onekiwi-rzv2h-16gb.dts \
    file://onekiwi-rzv2h-dev.dts \
    file://ubuntu-features.cfg \
    file://udl.cfg \
    file://imx415.c \
    file://IMX415_drv.c \
    file://IMX415_seq.h \
    file://IMX415_config.h \
    file://isp_config_seq.h \
    file://patch_cru.py \
"

do_configure:prepend() {
    cp ${WORKDIR}/onekiwi-*.dts ${S}/arch/arm64/boot/dts/renesas/

    if ! grep -q "onekiwi-rzv2h-8gb.dtb" ${S}/arch/arm64/boot/dts/renesas/Makefile; then
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-8gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-16gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-dev.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
    fi
    # 4GB board (separate check so it is also added when the Makefile already has the 8GB entries)
    if ! grep -q "onekiwi-rzv2h-4gb.dtb" ${S}/arch/arm64/boot/dts/renesas/Makefile; then
        echo "dtb-\$(CONFIG_ARCH_R9A09G057) += onekiwi-rzv2h-4gb.dtb" >> ${S}/arch/arm64/boot/dts/renesas/Makefile
    fi

    # Dynamically patch CRU driver files to expand max_width to 4095 and RAW10 packing
    python3 ${WORKDIR}/patch_cru.py ${S}

    # Override IMX415 sensor driver in kernel with IMX412 code
    cp ${WORKDIR}/imx415.c ${S}/drivers/media/i2c/imx415.c
    # imx415.c (IMX412 600MHz, gremsy) is written for kernel 6.1: port the 3 API changes to 6.10
    # (same as HUONG_DAN_TICH_HOP_IMX412_ISP_UBUNTU.md, step 3.2)
    sed -i \
        -e 's/framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);/framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);/' \
        -e 's/\.init_cfg = imx415_init_pad_cfg,/\/\/ .init_cfg = imx415_init_pad_cfg,/' \
        -e 's/\.probe_new = imx415_probe,/.probe = imx415_probe,/' \
        ${S}/drivers/media/i2c/imx415.c
    if grep -qE "v4l2_subdev_get_try_format\(|\.probe_new|^\s*\.init_cfg" ${S}/drivers/media/i2c/imx415.c; then
        bbfatal "imx415.c: kernel 6.10 API port did not apply"
    fi

    # Override ISP sensor driver wrapper in kernel with IMX412 configuration
    ISP_SENSOR_DIR=${S}/drivers/media/platform/renesas/rzv2h-isp/subdev/sensor/src/driver/sensor
    mkdir -p ${ISP_SENSOR_DIR}
    cp ${WORKDIR}/IMX415_drv.c ${ISP_SENSOR_DIR}/
    cp ${WORKDIR}/IMX415_seq.h ${ISP_SENSOR_DIR}/
    cp ${WORKDIR}/IMX415_config.h ${ISP_SENSOR_DIR}/

    # IMX412 ISP configuration & Color Correction Matrix (CCM), as in the gremsy meta
    ISP_FWLIB_DIR=${S}/drivers/media/platform/renesas/rzv2h-isp/v4l2_dev/src/fw_lib
    mkdir -p ${ISP_FWLIB_DIR}
    cp ${WORKDIR}/isp_config_seq.h ${ISP_FWLIB_DIR}/
}
