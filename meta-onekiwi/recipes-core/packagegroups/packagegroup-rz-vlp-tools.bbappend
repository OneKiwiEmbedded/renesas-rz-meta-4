# kernel 6.10 (renesas_defconfig) builds these drivers built-in (=y) instead of
# modules (=m), so no .ko files are generated and no Yocto packages exist.
# Remove them from hard RDEPENDS to prevent do_rootfs failure.
# Move them to RRECOMMENDS so they are installed only if available.

RDEPENDS:packagegroup-rz-vlp-tools-base:remove = "kernel-module-uas"
RRECOMMENDS:packagegroup-rz-vlp-tools-base:append = " kernel-module-uas"

RDEPENDS:packagegroup-rz-vlp-tools-multimedia:remove = "kernel-module-uvcvideo"
RRECOMMENDS:packagegroup-rz-vlp-tools-multimedia:append = " kernel-module-uvcvideo"
