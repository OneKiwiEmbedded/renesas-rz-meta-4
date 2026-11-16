import sys
import os
import re

def patch_file(filepath, patches):
    if not os.path.exists(filepath):
        print(f"Error: {filepath} not found!")
        return False
    
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()
    
    modified = False
    for pattern, replacement in patches:
        new_content, count = re.subn(pattern, replacement, content, flags=re.DOTALL)
        if count > 0:
            content = new_content
            modified = True
            print(f"Applied patch to {filepath} ({count} matches)")
        else:
            print(f"Warning: pattern not matched in {filepath}")
            
    if modified:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 patch_cru.py <kernel_source_dir>")
        sys.exit(1)
        
    kernel_dir = sys.argv[1]
    
    # 1. Patch rzg2l-core.c (expand max_width to 4095)
    core_c = os.path.join(kernel_dir, "drivers/media/platform/renesas/rzg2l-cru/rzg2l-core.c")
    core_patches = [
        (r'\.max_width\s*=\s*\d+', '.max_width = 4095')
    ]
    patch_file(core_c, core_patches)
    
    # 2. Patch rzg2l-csi2.c (expand RZG2L_CSI2_MAX_WIDTH and force-disable DESKEW)
    csi2_c = os.path.join(kernel_dir, "drivers/media/platform/renesas/rzg2l-cru/rzg2l-csi2.c")
    csi2_patches = [
        (r'#define\s+RZG2L_CSI2_MAX_WIDTH\s+\d+', '#define RZG2L_CSI2_MAX_WIDTH 4095'),
        # Force disable deskew by substituting set with clr (supports both kernel 5.10 and 6.1 naming)
        (r'rzg2l_csi2_set\s*\(\s*(priv|csi2)\s*,\s*(DPHY_S_DPHYCTL_MSB|CRUm_S_DPHYCTL_MSB)\s*,\s*(DPHY_S_DPHYCTL_MSB_DESKEW|CRUm_S_DPHYCTL_MSB_DESKEW)\s*\);',
         r'rzg2l_csi2_clr(\1, \2, \3);')
    ]
    patch_file(csi2_c, csi2_patches)
    
    # 3. Patch rzg2l-video.c (inject RAW_CRU10 packing stride calculation)
    video_c = os.path.join(kernel_dir, "drivers/media/platform/renesas/rzg2l-cru/rzg2l-video.c")
    
    video_patches = [
        # Kernel 6.10: bytesperline and sizeimage with margin lines
        (r'pix->bytesperline\s*=\s*pix->width\s*\*\s*fmt->bpp;\s*if\s*\(pix->pixelformat\s*==\s*V4L2_PIX_FMT_NV16\)\s*pix->sizeimage\s*=\s*pix->bytesperline\s*\*\s*pix->height\s*\*\s*2;\s*else\s*pix->sizeimage\s*=\s*pix->bytesperline\s*\*\s*pix->height;',
         '''pix->bytesperline = ALIGN(pix->width * fmt->bpp, 128);
	if (pix->pixelformat == V4L2_PIX_FMT_NV16)
		pix->sizeimage = pix->bytesperline * pix->height * 2;
	else
		pix->sizeimage = pix->bytesperline * (pix->height + 64);'''),
        # Kernel 6.10: IRQ dma_size check (allow up to buffer boundary)
        (r'if\s*\(\(\(long\)dma_size\)\s*&&\s*dma_size\s*<=\s*cru->format\.sizeimage\)',
         'if (((long)dma_size > 0) && dma_size <= cru->format.sizeimage + 0x200000)'),
        # Kernel 6.10: stride check
        (r'stride\s*=\s*cru->format\.bytesperline;\s*if\s*\(\s*stride\s*%\s*128\s*\)\s*\{\s*spin_unlock_irqrestore\(&cru->qlock,\s*flags\);\s*dev_err\(cru->dev,\s*"Bytesperline must be multiple of 128 bytes\\n"\);\s*return\s*-EINVAL;\s*\}\s*stride\s*=\s*stride\s*/\s*128;\s*rzg2l_cru_write\(cru,\s*AMnIS,\s*AMnIS_IS\(stride\)\);',
         '''stride = ALIGN(cru->format.bytesperline, 128);
		stride = stride / 128;
		rzg2l_cru_write(cru, AMnIS, AMnIS_IS(stride));'''),
        # Kernel 6.1 / legacy pattern
        (r'v4l2_fill_pixfmt\s*\(\s*pix\s*,\s*pix->pixelformat\s*,\s*pix->width\s*,\s*pix->height\s*\);',
         '''if (pix->pixelformat == V4L2_PIX_FMT_RAW_CRU10) {
		pix->bytesperline = ALIGN(pix->width * 5 / 4, 128);
		pix->sizeimage = pix->bytesperline * (pix->height + 64);
	} else {
		v4l2_fill_pixfmt(pix, pix->pixelformat, pix->width, pix->height);
		if (pix->pixelformat == V4L2_PIX_FMT_SRGGB10 ||
		    pix->pixelformat == V4L2_PIX_FMT_SGRBG10 ||
		    pix->pixelformat == V4L2_PIX_FMT_SGBRG10 ||
		    pix->pixelformat == V4L2_PIX_FMT_SBGGR10) {
			pix->bytesperline = ((pix->width + 11) / 12) * 16;
		}
		pix->bytesperline = ALIGN(pix->bytesperline, 128);
		pix->sizeimage = pix->bytesperline * pix->height;
	}'''),
        (r'if\s*\(\(stride\s*%\s*128\)\s*==\s*0\)\s*\{\s*stride\s*=\s*stride\s*/\s*128;\s*rzg2l_cru_write\(cru,\s*AMnIS,\s*AMnIS_IS\(stride\)\);\s*\}\s*else\s*\{[^}]+return\s*-EINVAL;\s*\}',
         '''stride = ALIGN(cru->format.bytesperline, 128);
		rzg2l_cru_write(cru, AMnIS, AMnIS_IS(stride / 128));''')
    ]
    patch_file(video_c, video_patches)

    print("Cru driver patch script finished!")
