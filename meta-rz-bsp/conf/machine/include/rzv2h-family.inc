require conf/machine/include/soc-family.inc
SOC_FAMILY = "rzv2h-family:mali-family"

MACHINE_FEATURES = "opengles opencl mali-g31"

# Linux kernel configuration
PREFERRED_PROVIDER_virtual/kernel ?= "linux-rz"
PREFERRED_VERSION_linux-rz ?= "6.10%"
KERNEL_IMAGETYPE ?= "Image"

# Wic image offset
FIP_WIC_OFFSET = "--offset 768s"
