# Arducam IMX415 on the 4GB board (only when meta-rz-isp is in bblayers.conf).
#
# The patch changes drivers/media/i2c/imx415.c and rzg2l-csi2.c as patched by
# meta-rz-isp, so it must be applied after the meta-rz-isp patches. The order of
# SRC_URI patches between layers is not guaranteed, so it is fetched with
# apply=no and applied at the end of do_patch.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

ONEKIWI_LATE_PATCHES ?= ""
ONEKIWI_LATE_PATCHES:onekiwi-rzv2h-4gb = "0002-onekiwi-rzv2h-4gb-imx415-arducam.patch"

SRC_URI:append:onekiwi-rzv2h-4gb = " file://0002-onekiwi-rzv2h-4gb-imx415-arducam.patch;apply=no"

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
