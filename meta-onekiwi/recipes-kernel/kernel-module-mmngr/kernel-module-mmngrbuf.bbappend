# Fix mmngrbuf driver compilation against kernel >= 6.3
# - vma->vm_flags became read-only in kernel 6.3, use vm_flags_set()

MMNGRBUF_DRV_C = "${S}/mmngr_drv/mmngrbuf/mmngrbuf-module/files/mmngrbuf/drv/mmngr_buf_drv.c"

do_compile:prepend() {
    python3 - <<'PYEOF'
import sys

path = "${MMNGRBUF_DRV_C}"
try:
    with open(path, 'r') as f:
        src = f.read()
except FileNotFoundError:
    print("WARNING: mmngr_buf_drv.c not found, skipping kernel 6.10 fixes")
    sys.exit(0)

# Add linux/version.h if missing
if '#include <linux/version.h>' not in src:
    src = '#include <linux/version.h>\n' + src

# Fix vma->vm_flags read-only (kernel 6.3+)
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
    print("mmngrbuf: patched vm_flags set for kernel 6.3+")

with open(path, 'w') as f:
    f.write(src)
PYEOF
}
