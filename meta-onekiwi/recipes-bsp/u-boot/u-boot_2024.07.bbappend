FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Override the official Renesas U-Boot repository with the custom OneKiwi fork
UBOOT_URI = "git://github.com/OneKiwiEmbedded/renesas-rz-uboot.git;protocol=https"
UBOOT_BRANCH = "v2024.07/rz_dev_v2h"

# Tell bitbake to always fetch the latest commit from the custom branch
UBOOT_REV:rzv2h-family = "${AUTOREV}"
SRCREV = "${AUTOREV}"


# 4GB board: OneKiwiEmbedded/renesas-rz-uboot-4 (same v2024.07/rz_dev_v2h base as above
# + board onekiwi-rzv2h-4gb). 8GB/16GB keep the repository above.
UBOOT_URI:onekiwi-rzv2h-4gb = "git://github.com/OneKiwiEmbedded/renesas-rz-uboot-4.git;protocol=https"
UBOOT_BRANCH:onekiwi-rzv2h-4gb = "v2024.07/rz_dev_v2h"
UBOOT_REV:onekiwi-rzv2h-4gb = "${AUTOREV}"
