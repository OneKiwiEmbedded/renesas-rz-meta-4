# Preemptive fix for vspm driver against kernel 6.10
# - struct of_device_id incomplete: add explicit #include <linux/of.h>
# - vma->vm_flags read-only since kernel 6.3: use vm_flags_set()

VSPM_DRV_DIR_PATH = "${S}/vspm-module/files/vspm/drv"

do_compile:prepend() {
    python3 - <<'PYEOF'
import os, sys, glob

drv_dir = "${VSPM_DRV_DIR_PATH}"
c_files = glob.glob(os.path.join(drv_dir, '*.c'))

if not c_files:
    print("WARNING: no vspm drv C files found, skipping kernel 6.10 fixes")
    sys.exit(0)

for path in c_files:
    with open(path, 'r') as f:
        src = f.read()
    changed = False

    # Fix 1: add linux/of.h for struct of_device_id
    if 'of_device_id' in src and '#include <linux/of.h>' not in src:
        src = '#include <linux/of.h>\n' + src
        changed = True
        print(f"vspm: added #include <linux/of.h> in {os.path.basename(path)}")

    # Fix 2: vm_flags read-only since kernel 6.3
    old1 = 'vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);'
    new1 = 'vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);'
    if old1 in src:
        src = src.replace(old1, new1)
        changed = True
        print(f"vspm: patched vm_flags in {os.path.basename(path)}")

    old2 = 'vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);'
    new2 = 'vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);'
    if old2 in src:
        src = src.replace(old2, new2)
        changed = True
        print(f"vspm: patched vm_flags (no VM_IO) in {os.path.basename(path)}")

    if changed:
        with open(path, 'w') as f:
            f.write(src)
PYEOF
}
