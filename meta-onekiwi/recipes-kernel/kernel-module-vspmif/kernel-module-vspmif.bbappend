# Fix vspmif driver compilation against kernel 6.10
# - struct of_device_id incomplete: add explicit #include <linux/of.h>
# - vma->vm_flags read-only since kernel 6.3: use vm_flags_set()

VSPMIF_DRV_C = "${S}/vspm_if-module/files/vspm_if/drv/vspm_if_main.c"

do_compile:prepend() {
    python3 - <<'PYEOF'
import sys

path = "${VSPMIF_DRV_C}"
try:
    with open(path, 'r') as f:
        src = f.read()
except FileNotFoundError:
    print("WARNING: vspm_if_main.c not found, skipping kernel 6.10 fixes")
    sys.exit(0)

# Fix 1: add linux/of.h for struct of_device_id
if '#include <linux/of.h>' not in src:
    src = '#include <linux/of.h>\n' + src
    print("vspmif: added #include <linux/of.h> for struct of_device_id")

# Fix 2: vm_flags read-only since kernel 6.3
old1 = 'vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);'
new1 = 'vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);'
if old1 in src:
    src = src.replace(old1, new1)
    print("vspmif: patched vm_flags set for kernel 6.3+")

old2 = 'vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);'
new2 = 'vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);'
if old2 in src:
    src = src.replace(old2, new2)
    print("vspmif: patched vm_flags (no VM_IO) set for kernel 6.3+")

with open(path, 'w') as f:
    f.write(src)
PYEOF
}
