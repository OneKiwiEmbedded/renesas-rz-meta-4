# Fix mmngr driver compilation against kernel >= 6.3 / 6.5
# - vma->vm_flags became read-only in kernel 6.3, use vm_flags_set()
# - follow_pfn() removed in kernel 6.5, derive PFN from vm_pgoff for IO/DMA VMAs

MMNGR_DRV_C = "${S}/mmngr_drv/mmngr/mmngr-module/files/mmngr/drv/mmngr_drv.c"

do_compile:prepend() {
    python3 - <<'PYEOF'
import re, sys

path = "${MMNGR_DRV_C}"
try:
    with open(path, 'r') as f:
        src = f.read()
except FileNotFoundError:
    print("WARNING: mmngr_drv.c not found, skipping kernel 6.10 fixes")
    sys.exit(0)

# 1. Add linux/version.h if missing
if '#include <linux/version.h>' not in src:
    src = '#include <linux/version.h>\n' + src

# 2. Fix follow_pfn (removed in kernel 6.5)
old_pfn = '\tret = follow_pfn(vma, start, &pfn);'
new_pfn = (
    '\t/* follow_pfn removed in 6.5: for IO/DMA VMAs derive PFN from vm_pgoff */\n'
    '#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)\n'
    '\tpfn = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);\n'
    '\tret = 0;\n'
    '#else\n'
    '\tret = follow_pfn(vma, start, &pfn);\n'
    '#endif'
)
if old_pfn in src:
    src = src.replace(old_pfn, new_pfn)
    print("mmngr: patched follow_pfn for kernel 6.5+")

# 3. Fix vma->vm_flags read-only (kernel 6.3+)
old1 = '\tvma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);'
new1 = (
    '#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)\n'
    '\tvm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);\n'
    '#else\n'
    '\tvma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);\n'
    '#endif'
)
if old1 in src:
    src = src.replace(old1, new1)
    print("mmngr: patched vm_flags VM_IO set for kernel 6.3+")

old2 = '\tvma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);'
new2 = (
    '#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)\n'
    '\tvm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);\n'
    '#else\n'
    '\tvma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);\n'
    '#endif'
)
if old2 in src:
    src = src.replace(old2, new2)
    print("mmngr: patched vm_flags VM_DONTEXPAND set for kernel 6.3+")

with open(path, 'w') as f:
    f.write(src)
PYEOF
}
