DESCRIPTION = "Linux kernel from Renesas RZ/V2H RDK (linux-rz) - kernel 6.10"

require recipes-kernel/linux/linux-yocto.inc
require linux-rz_6.10.inc

LINUX_VERSION ?= "6.10.14"

# Use renesas_defconfig from linux-rz repo (ubuntu/rz-v2h-rdk branch)
KBUILD_DEFCONFIG ?= "renesas_defconfig"
KCONFIG_MODE ?= "alldefconfig"

KERNEL_URL ?= "git://github.com/Renesas-SST/linux-rz.git"
KERNEL_BRANCH ?= "ubuntu/rz-v2h-rdk"
KERNEL_REV ?= "c38fb44e664c5bc01e8ab398450b51e1dd5a4a93"
