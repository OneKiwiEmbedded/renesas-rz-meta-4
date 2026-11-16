FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Override the official Renesas U-Boot repository with the custom OneKiwi fork
UBOOT_URI = "git://github.com/OneKiwiEmbedded/renesas-rz-uboot.git;protocol=https"
UBOOT_BRANCH = "v2024.07/rz_dev_v2h"

# Tell bitbake to always fetch the latest commit from the custom branch
UBOOT_REV:rzv2h-family = "${AUTOREV}"
SRCREV = "${AUTOREV}"

