# OneKiwi RZ/V2H boards: Linux 6.1.141-cip43 (Renesas rz_linux-cip @ 10b3443)
# + OneKiwi board device trees + Renesas ethernet fixes.
KERNEL_URL:onekiwi-rzv2h-4gb = "git://github.com/OneKiwiEmbedded/renesas-rz-linux-4.git"
KERNEL_URL:onekiwi-rzv2h-8gb = "git://github.com/OneKiwiEmbedded/renesas-rz-linux-4.git"

KERNEL_BRANCH:onekiwi-rzv2h-4gb = "rz-6.1-cip43"
KERNEL_BRANCH:onekiwi-rzv2h-8gb = "rz-6.1-cip43"

KERNEL_REV:onekiwi-rzv2h-4gb = "${AUTOREV}"
KERNEL_REV:onekiwi-rzv2h-8gb = "${AUTOREV}"

COMPATIBLE_MACHINE:onekiwi-rzv2h-4gb = "onekiwi-rzv2h-4gb"
COMPATIBLE_MACHINE:onekiwi-rzv2h-8gb = "onekiwi-rzv2h-8gb"

# Board kernel options (config fragment)
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI:append:onekiwi-rzv2h-4gb = " file://onekiwi-rzv2h.cfg"
SRC_URI:append:onekiwi-rzv2h-8gb = " file://onekiwi-rzv2h.cfg"
