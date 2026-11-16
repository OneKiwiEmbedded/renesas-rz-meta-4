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
    
    # 2. Patch rzg2l-csi2.c (expand RZG2L_CSI2_MAX_WIDTH, force-disable DESKEW and adjust hssettle)
    csi2_c = os.path.join(kernel_dir, "drivers/media/platform/renesas/rzg2l-cru/rzg2l-csi2.c")
    csi2_patches = [
        (r'#define\s+RZG2L_CSI2_MAX_WIDTH\s+\d+', '#define RZG2L_CSI2_MAX_WIDTH 4095'),
        # Force disable deskew by substituting set with clr (supports both kernel 5.10 and 6.1 naming)
        (r'rzg2l_csi2_set\s*\(\s*(priv|csi2)\s*,\s*(DPHY_S_DPHYCTL_MSB|CRUm_S_DPHYCTL_MSB)\s*,\s*(DPHY_S_DPHYCTL_MSB_DESKEW|CRUm_S_DPHYCTL_MSB_DESKEW)\s*\);',
         r'rzg2l_csi2_clr(\1, \2, \3);'),
        # Increase hssettle for 1000 Mbps to 30 for clean signal lock without glitches
        (r'\{\s*1030\s*,\s*22\s*\},', '{ 1030, 30 },')
    ]
    patch_file(csi2_c, csi2_patches)
    
    # 3. Patch rzg2l-video.c (inject RAW_CRU10 packing stride calculation)
    video_c = os.path.join(kernel_dir, "drivers/media/platform/renesas/rzg2l-cru/rzg2l-video.c")
    
    # Look for v4l2_fill_pixfmt(pix, pix->pixelformat, pix->width, pix->height); inside rzg2l_cru_format_align
    video_patches = [
        (r'v4l2_fill_pixfmt\s*\(\s*pix\s*,\s*pix->pixelformat\s*,\s*pix->width\s*,\s*pix->height\s*\);',
         '''if (pix->pixelformat == V4L2_PIX_FMT_RAW_CRU10) {
		pix->bytesperline = ALIGN(pix->width * 5 / 4, 128);
		pix->sizeimage = pix->bytesperline * pix->height + 128;
	} else {
		v4l2_fill_pixfmt(pix, pix->pixelformat, pix->width, pix->height);
		if (pix->pixelformat == V4L2_PIX_FMT_SRGGB10 ||
		    pix->pixelformat == V4L2_PIX_FMT_SGRBG10 ||
		    pix->pixelformat == V4L2_PIX_FMT_SGBRG10 ||
		    pix->pixelformat == V4L2_PIX_FMT_SBGGR10) {
			pix->bytesperline = ((pix->width + 11) / 12) * 16;
		}
		/* CRITICAL: RZ/G2L CRU AMnIS register requires 128-byte alignment for ALL formats */
		pix->bytesperline = ALIGN(pix->bytesperline, 128);
		pix->sizeimage = pix->bytesperline * pix->height;
	}''')
    ]
    patch_file(video_c, video_patches)

    # 4. Patch rzg2l-ip.c (add standard RAW10 formats mapping)
    ip_c = os.path.join(kernel_dir, "drivers/media/platform/renesas/rzg2l-cru/rzg2l-ip.c")
    ip_patches = [
        (r'\{\s*\.codes\s*=\s*\{\s*MEDIA_BUS_FMT_SBGGR10_1X10,\s*MEDIA_BUS_FMT_SGBRG10_1X10,\s*MEDIA_BUS_FMT_SGRBG10_1X10,\s*MEDIA_BUS_FMT_SRGGB10_1X10\s*\},\s*\.format\s*=\s*V4L2_PIX_FMT_RAW_CRU10,\s*\.datatype\s*=\s*MIPI_CSI2_DT_RAW10,\s*\.icndmr\s*=\s*0,\s*\.fmt_types\s*=\s*V4L2_PIXEL_ENC_BAYER,\s*\},',
         '''{
		.codes = {
			MEDIA_BUS_FMT_SBGGR10_1X10,
			MEDIA_BUS_FMT_SGBRG10_1X10,
			MEDIA_BUS_FMT_SGRBG10_1X10,
			MEDIA_BUS_FMT_SRGGB10_1X10
		},
		.format = V4L2_PIX_FMT_RAW_CRU10,
		.datatype = MIPI_CSI2_DT_RAW10,
		.icndmr = 0,
		.fmt_types = V4L2_PIXEL_ENC_BAYER,
	},
	{
		.codes = {
			MEDIA_BUS_FMT_SRGGB10_1X10,
		},
		.format = V4L2_PIX_FMT_SRGGB10,
		.datatype = MIPI_CSI2_DT_RAW10,
		.icndmr = 0,
		.fmt_types = V4L2_PIXEL_ENC_BAYER,
	},
	{
		.codes = {
			MEDIA_BUS_FMT_SGRBG10_1X10,
		},
		.format = V4L2_PIX_FMT_SGRBG10,
		.datatype = MIPI_CSI2_DT_RAW10,
		.icndmr = 0,
		.fmt_types = V4L2_PIXEL_ENC_BAYER,
	},
	{
		.codes = {
			MEDIA_BUS_FMT_SGBRG10_1X10,
		},
		.format = V4L2_PIX_FMT_SGBRG10,
		.datatype = MIPI_CSI2_DT_RAW10,
		.icndmr = 0,
		.fmt_types = V4L2_PIXEL_ENC_BAYER,
	},
	{
		.codes = {
			MEDIA_BUS_FMT_SBGGR10_1X10,
		},
		.format = V4L2_PIX_FMT_SBGGR10,
		.datatype = MIPI_CSI2_DT_RAW10,
		.icndmr = 0,
		.fmt_types = V4L2_PIXEL_ENC_BAYER,
	},''')
    ]
    patch_file(ip_c, ip_patches)

    print("Cru driver patch script finished!")
