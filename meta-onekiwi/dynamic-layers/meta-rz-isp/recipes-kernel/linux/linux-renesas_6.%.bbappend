# meta-rz-isp limits this recipe to (rzv2h-evk|onekiwi-rzv2h-8gb); allow the 4GB board too
COMPATIBLE_MACHINE:onekiwi-rzv2h-4gb = "onekiwi-rzv2h-4gb"

# Arducam IMX415 module (24 MHz INCK, 4 lanes x 720 Mbps) - driver part of the
# OneKiwi SDK 6.0 Arducam port. It changes imx415.c, rzg2l-csi2.c, rzg2l-dma.c and
# IMX415_drv.c as created by the meta-rz-isp kernel patches, so it must run after
# them: fetched with apply=no and applied at the end of do_patch.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

ONEKIWI_LATE_PATCHES ?= ""
ONEKIWI_LATE_PATCHES:onekiwi-rzv2h-4gb = "0001-imx415-arducam-24mhz-720mbps-driver.patch 0002-rzv2h-isp-min-capture-buffers.patch"

SRC_URI:append:onekiwi-rzv2h-4gb = " file://0001-imx415-arducam-24mhz-720mbps-driver.patch;apply=no file://0002-rzv2h-isp-min-capture-buffers.patch;apply=no"

do_patch[postfuncs] += "onekiwi_apply_late_patches"
onekiwi_apply_late_patches() {
    for p in ${ONEKIWI_LATE_PATCHES}; do
        if patch -p1 -R --dry-run -s -d ${S} < ${WORKDIR}/$p >/dev/null 2>&1; then
            bbnote "onekiwi: $p already applied"
        else
            bbnote "onekiwi: applying $p"
            patch -p1 -d ${S} < ${WORKDIR}/$p || bbfatal "onekiwi: $p failed to apply"
        fi
    done
}
