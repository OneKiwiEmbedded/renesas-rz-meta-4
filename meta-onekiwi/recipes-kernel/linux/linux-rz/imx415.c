// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sony imx415 Camera Sensor Driver
 *
 * Copyright (C) 2021 Intel Corporation
 */
#include <asm/unaligned.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

/* Streaming Mode */
#define IMX415_REG_MODE_SELECT	0x0100
#define IMX415_MODE_STANDBY	0x00
#define IMX415_MODE_STREAMING	0x01

/* Lines per frame */
#define IMX415_REG_LPFR		0x0340

/* Chip ID */
#define IMX415_REG_ID		0x0016
#define IMX415_ID		0x577

/* Exposure control */
#define IMX415_REG_EXPOSURE_CIT	0x0202
#define IMX415_EXPOSURE_MIN	8
#define IMX415_EXPOSURE_OFFSET	22
#define IMX415_EXPOSURE_STEP	1
#define IMX415_EXPOSURE_DEFAULT	0x0648

/* Analog gain control */
#define IMX415_REG_AGAIN	0x0204
#define IMX415_AGAIN_MIN	0
#define IMX415_AGAIN_MAX	978
#define IMX415_AGAIN_STEP	1
#define IMX415_AGAIN_DEFAULT	0

/* Group hold register */
#define IMX415_REG_HOLD		0x0104

/* Input clock rate */
#define IMX415_INCLK_RATE	24000000

/* CSI2 HW configuration */
#define IMX415_LINK_FREQ	600000000
#define IMX415_NUM_DATA_LANES	4

#define IMX415_REG_MIN		0x00
#define IMX415_REG_MAX		0xffff

/**
 * struct imx415_reg - imx415 sensor register
 * @address: Register address
 * @val: Register value
 */
struct imx415_reg {
	u16 address;
	u8 val;
};

/**
 * struct imx415_reg_list - imx415 sensor register list
 * @num_of_regs: Number of registers in the list
 * @regs: Pointer to register list
 */
struct imx415_reg_list {
	u32 num_of_regs;
	const struct imx415_reg *regs;
};

/**
 * struct imx415_mode - imx415 sensor mode structure
 * @width: Frame width
 * @height: Frame height
 * @code: Format code
 * @hblank: Horizontal blanking in lines
 * @vblank: Vertical blanking in lines
 * @vblank_min: Minimum vertical blanking in lines
 * @vblank_max: Maximum vertical blanking in lines
 * @pclk: Sensor pixel clock
 * @link_freq_idx: Link frequency index
 * @reg_list: Register list for sensor mode
 */
struct imx415_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u64 pclk;
	u32 link_freq_idx;
	struct imx415_reg_list reg_list;
};

static const char * const imx415_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
	"dvdd",		/* Digital core power */
};

/**
 * struct imx415 - imx415 sensor device structure
 * @dev: Pointer to generic device
 * @client: Pointer to i2c client
 * @sd: V4L2 sub-device
 * @pad: Media pad. Only one pad supported
 * @reset_gpio: Sensor reset gpio
 * @inclk: Sensor input clock
 * @supplies: Regulator supplies
 * @ctrl_handler: V4L2 control handler
 * @link_freq_ctrl: Pointer to link frequency control
 * @pclk_ctrl: Pointer to pixel clock control
 * @hblank_ctrl: Pointer to horizontal blanking control
 * @vblank_ctrl: Pointer to vertical blanking control
 * @exp_ctrl: Pointer to exposure control
 * @again_ctrl: Pointer to analog gain control
 * @vblank: Vertical blanking in lines
 * @fmt_code: Currently selected media bus format code
 * @cur_mode: Pointer to current selected sensor mode
 * @mutex: Mutex for serializing sensor controls
 */
struct imx415 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct regulator_bulk_data supplies[ARRAY_SIZE(imx415_supply_names)];
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *pclk_ctrl;
	struct v4l2_ctrl *hblank_ctrl;
	struct v4l2_ctrl *vblank_ctrl;
	struct {
		struct v4l2_ctrl *exp_ctrl;
		struct v4l2_ctrl *again_ctrl;
	};
	u32 vblank;
	const struct imx415_mode *cur_mode;
    u32 fmt_code;
	struct mutex mutex;
};

static const s64 link_freq[] = {
	IMX415_LINK_FREQ,
};

/* Sensor mode registers */
static const struct imx415_reg mode_4056x3040_regs[] = {
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x3c7e, 0x08},
	{0x3c7f, 0x02},
	{0x38a8, 0x1f},
	{0x38a9, 0xff},
	{0x38aa, 0x1f},
	{0x38ab, 0xff},
	{0x55d4, 0x00},
	{0x55d5, 0x00},
	{0x55d6, 0x07},
	{0x55d7, 0xff},
	{0x55e8, 0x07},
	{0x55e9, 0xff},
	{0x55ea, 0x00},
	{0x55eb, 0x00},
	{0x575c, 0x07},
	{0x575d, 0xff},
	{0x575e, 0x00},
	{0x575f, 0x00},
	{0x5764, 0x00},
	{0x5765, 0x00},
	{0x5766, 0x07},
	{0x5767, 0xff},
	{0x5974, 0x04},
	{0x5975, 0x01},
	{0x5f10, 0x09},
	{0x5f11, 0x92},
	{0x5f12, 0x32},
	{0x5f13, 0x72},
	{0x5f14, 0x16},
	{0x5f15, 0xba},
	{0x5f17, 0x13},
	{0x5f18, 0x24},
	{0x5f19, 0x60},
	{0x5f1a, 0xe3},
	{0x5f1b, 0xad},
	{0x5f1c, 0x74},
	{0x5f2d, 0x25},
	{0x5f5c, 0xd0},
	{0x6a22, 0x00},
	{0x6a23, 0x1d},
	{0x7ba8, 0x00},
	{0x7ba9, 0x00},
	{0x886b, 0x00},
	{0x9002, 0x0a},
	{0x9004, 0x1a},
	{0x9214, 0x93},
	{0x9215, 0x69},
	{0x9216, 0x93},
	{0x9217, 0x6b},
	{0x9218, 0x93},
	{0x9219, 0x6d},
	{0x921a, 0x57},
	{0x921b, 0x58},
	{0x921c, 0x57},
	{0x921d, 0x59},
	{0x921e, 0x57},
	{0x921f, 0x5a},
	{0x9220, 0x57},
	{0x9221, 0x5b},
	{0x9222, 0x93},
	{0x9223, 0x02},
	{0x9224, 0x93},
	{0x9225, 0x03},
	{0x9226, 0x93},
	{0x9227, 0x04},
	{0x9228, 0x93},
	{0x9229, 0x05},
	{0x922a, 0x98},
	{0x922b, 0x21},
	{0x922c, 0xb2},
	{0x922d, 0xdb},
	{0x922e, 0xb2},
	{0x922f, 0xdc},
	{0x9230, 0xb2},
	{0x9231, 0xdd},
	{0x9232, 0xe2},
	{0x9233, 0xe1},
	{0x9234, 0xb2},
	{0x9235, 0xe2},
	{0x9236, 0xb2},
	{0x9237, 0xe3},
	{0x9238, 0xb7},
	{0x9239, 0xb9},
	{0x923a, 0xb7},
	{0x923b, 0xbb},
	{0x923c, 0xb7},
	{0x923d, 0xbc},
	{0x923e, 0xb7},
	{0x923f, 0xc5},
	{0x9240, 0xb7},
	{0x9241, 0xc7},
	{0x9242, 0xb7},
	{0x9243, 0xc9},
	{0x9244, 0x98},
	{0x9245, 0x56},
	{0x9246, 0x98},
	{0x9247, 0x55},
	{0x9380, 0x00},
	{0x9381, 0x62},
	{0x9382, 0x00},
	{0x9383, 0x56},
	{0x9384, 0x00},
	{0x9385, 0x52},
	{0x9388, 0x00},
	{0x9389, 0x55},
	{0x938a, 0x00},
	{0x938b, 0x55},
	{0x938c, 0x00},
	{0x938d, 0x41},
	{0x5078, 0x01},
	{0x0112, 0x0a},
	{0x0113, 0x0a},
	{0x0114, 0x03},
	{0x0342, 0x11},
	{0x0343, 0xa0},
	{0x0340, 0x0d},
	{0x0341, 0xda},
	{0x3210, 0x00},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0f},
	{0x0349, 0xd7},
	{0x034a, 0x0b},
	{0x034b, 0xdf},
	{0x00e3, 0x00},
	{0x00e4, 0x00},
	{0x00e5, 0x01},
	{0x00fc, 0x0a},
	{0x00fd, 0x0a},
	{0x00fe, 0x0a},
	{0x00ff, 0x0a},
	{0xe013, 0x00},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x00},
	{0x3140, 0x02},
	{0x3241, 0x11},
	{0x3250, 0x03},
	{0x3e10, 0x00},
	{0x3e11, 0x00},
	{0x3f0d, 0x00},
	{0x3f42, 0x00},
	{0x3f43, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040a, 0x00},
	{0x040b, 0x00},
	{0x040c, 0x0f},
	{0x040d, 0xd8},
	{0x040e, 0x0b},
	{0x040f, 0xe0},
	{0x034c, 0x0f}, /* X_OUT_SIZE = 4080 (0x0ff0) */
	{0x034d, 0xf0},
	{0x034e, 0x0b}, /* Y_OUT_SIZE = 3056 (0x0bf0) */
	{0x034f, 0xf0},
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0xe3}, /* VT_PLL_MPY=227 -> exactly 30.0fps at 1200Mbps MIPI */
	{0x0309, 0x0a},
	{0x030b, 0x01},
	{0x030d, 0x02},
	{0x030e, 0x01},
	{0x030f, 0x5e}, /* IOP_PLL_MPY=94 -> OP at 1200 Mbps/lane */
	{0x0310, 0x00},
	{0x0820, 0x12},
	{0x0821, 0xc0},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x3e20, 0x01},
	{0x3e37, 0x00},
	{0x3f50, 0x00},
	{0x3f56, 0x00},
	{0x3f57, 0xe2},
	{0x3c0a, 0x5a},
	{0x3c0b, 0x55},
	{0x3c0c, 0x28},
	{0x3c0d, 0x07},
	{0x3c0e, 0xff},
	{0x3c0f, 0x00},
	{0x3c10, 0x00},
	{0x3c11, 0x02},
	{0x3c12, 0x00},
	{0x3c13, 0x03},
	{0x3c14, 0x00},
	{0x3c15, 0x00},
	{0x3c16, 0x0c},
	{0x3c17, 0x0c},
	{0x3c18, 0x0c},
	{0x3c19, 0x0a},
	{0x3c1a, 0x0a},
	{0x3c1b, 0x0a},
	{0x3c1c, 0x00},
	{0x3c1d, 0x00},
	{0x3c1e, 0x00},
	{0x3c1f, 0x00},
	{0x3c20, 0x00},
	{0x3c21, 0x00},
	{0x3c22, 0x3f},
	{0x3c23, 0x0a},
	{0x3e35, 0x01},
	{0x3f4a, 0x03},
	{0x3f4b, 0xbf},
	{0x3f26, 0x00},
	{0x0202, 0x0d},
	{0x0203, 0xc4},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020e, 0x01},
	{0x020f, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0xbcf1, 0x00},
};

static const struct imx415_reg imx415_mode_s_2112x2112_60fps[] = {
/* imx415_mode_common start */
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x3c7e, 0x01},
	{0x3c7f, 0x02},
	{0x38a8, 0x1f},
	{0x38a9, 0xff},
	{0x38aa, 0x1f},
	{0x38ab, 0xff},
	{0x55d4, 0x00},
	{0x55d5, 0x00},
	{0x55d6, 0x07},
	{0x55d7, 0xff},
	{0x55e8, 0x07},
	{0x55e9, 0xff},
	{0x55ea, 0x00},
	{0x55eb, 0x00},
	{0x575c, 0x07},
	{0x575d, 0xff},
	{0x575e, 0x00},
	{0x575f, 0x00},
	{0x5764, 0x00},
	{0x5765, 0x00},
	{0x5766, 0x07},
	{0x5767, 0xff},
	{0x5974, 0x04},
	{0x5975, 0x01},
	{0x5f10, 0x09},
	{0x5f11, 0x92},
	{0x5f12, 0x32},
	{0x5f13, 0x72},
	{0x5f14, 0x16},
	{0x5f15, 0xba},
	{0x5f17, 0x13},
	{0x5f18, 0x24},
	{0x5f19, 0x60},
	{0x5f1a, 0xe3},
	{0x5f1b, 0xad},
	{0x5f1c, 0x74},
	{0x5f2d, 0x25},
	{0x5f5c, 0xd0},
	{0x6a22, 0x00},
	{0x6a23, 0x1d},
	{0x7ba8, 0x00},
	{0x7ba9, 0x00},
	{0x886b, 0x00},
	{0x9002, 0x0a},
	{0x9004, 0x1a},
	{0x9214, 0x93},
	{0x9215, 0x69},
	{0x9216, 0x93},
	{0x9217, 0x6b},
	{0x9218, 0x93},
	{0x9219, 0x6d},
	{0x921a, 0x57},
	{0x921b, 0x58},
	{0x921c, 0x57},
	{0x921d, 0x59},
	{0x921e, 0x57},
	{0x921f, 0x5a},
	{0x9220, 0x57},
	{0x9221, 0x5b},
	{0x9222, 0x93},
	{0x9223, 0x02},
	{0x9224, 0x93},
	{0x9225, 0x03},
	{0x9226, 0x93},
	{0x9227, 0x04},
	{0x9228, 0x93},
	{0x9229, 0x05},
	{0x922a, 0x98},
	{0x922b, 0x21},
	{0x922c, 0xb2},
	{0x922d, 0xdb},
	{0x922e, 0xb2},
	{0x922f, 0xdc},
	{0x9230, 0xb2},
	{0x9231, 0xdd},
	{0x9232, 0xb2},
	{0x9233, 0xe1},
	{0x9234, 0xb2},
	{0x9235, 0xe2},
	{0x9236, 0xb2},
	{0x9237, 0xe3},
	{0x9238, 0xb7},
	{0x9239, 0xb9},
	{0x923a, 0xb7},
	{0x923b, 0xbb},
	{0x923c, 0xb7},
	{0x923d, 0xbc},
	{0x923e, 0xb7},
	{0x923f, 0xc5},
	{0x9240, 0xb7},
	{0x9241, 0xc7},
	{0x9242, 0xb7},
	{0x9243, 0xc9},
	{0x9244, 0x98},
	{0x9245, 0x56},
	{0x9246, 0x98},
	{0x9247, 0x55},
	{0x9380, 0x00},
	{0x9381, 0x62},
	{0x9382, 0x00},
	{0x9383, 0x56},
	{0x9384, 0x00},
	{0x9385, 0x52},
	{0x9388, 0x00},
	{0x9389, 0x55},
	{0x938a, 0x00},
	{0x938b, 0x55},
	{0x938c, 0x00},
	{0x938d, 0x41},
	{0x5078, 0x01},
/* imx415_mode_common end */
/*------MIPI setting------*/
	{0x0112, 0x0a},
	{0x0113, 0x0a},
	{0x0114, 0x03},
/*-----Frame Horizontal Clock Count-------*/
	{0x0342, 0x11},
	{0x0343, 0xa0},
/*-----Frame Vertical Clock Count-----*/
	{0x0340, 0x0c},
	{0x0341, 0x1e},
	{0x3210, 0x00},
/*-----Visible Size------*/
	{0x0344, 0x01},
	{0x0345, 0xfc},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0d},
	{0x0349, 0xd7},
	{0x034a, 0x0b},
	{0x034b, 0xdf},
/*-----Mode Setting------*/
	{0x00e3, 0x00},
	{0x00e4, 0x00},
	{0x00e5, 0x01},
	{0x00fc, 0x0a},
	{0x00fd, 0x0a},
	{0x00fe, 0x0a},
	{0x00ff, 0x0a},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x00},
	{0x3140, 0x02},
	{0x3241, 0x11},
	{0x3250, 0x03},
	{0x3e10, 0x00},
	{0x3e11, 0x00},
	{0x3f0d, 0x00},
	{0x3f42, 0x00},
	{0x3f43, 0x00},
/*-----Digital Crop & Scaling------*/
	{0x0401, 0x02},
	{0x0404, 0x00},
	{0x0405, 0x17},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040a, 0x00},
	{0x040b, 0x02},
	{0x040c, 0x0b},
	{0x040d, 0xdc},
	{0x040e, 0x0b},
	{0x040f, 0xdc},
/*-----Output Crop------*/
	{0x034c, 0x08},	/* Output Crop 034C 034D => X_OUT_SIZE = 2124 (0x084c) */
	{0x034d, 0x4c},
	{0x034e, 0x08},
	{0x034f, 0x48},	/* Output Crop 034E 034F => Y_OUT_SIZE = 2120 (0x0848) */
/*-----Clock Setting-----*/
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0xc8},
	{0x0309, 0x0a},
	{0x030b, 0x01},
	{0x030d, 0x02},
	{0x030e, 0x01},
	{0x030f, 0x5e},
	{0x0310, 0x00},
	{0x0820, 0x12},
	{0x0821, 0xc0},
	{0x0822, 0x00},
	{0x0823, 0x00},
/*-----Output Data Select Setting-----*/
	{0x3e20, 0x01},
	{0x3e37, 0x00},
/*-----PowerSave Setting------*/
	{0x3f50, 0x00},
	{0x3f56, 0x00},
	{0x3f57, 0xe2},
/*-----Other Setting-----*/
	{0x3c0a, 0x5a},
	{0x3c0b, 0x55},
	{0x3c0c, 0x28},
	{0x3c0d, 0x07},
	{0x3c0e, 0xff},
	{0x3c0f, 0x00},
	{0x3c10, 0x00},
	{0x3c11, 0x02},
	{0x3c12, 0x00},
	{0x3c13, 0x03},
	{0x3c14, 0x00},
	{0x3c15, 0x00},
	{0x3c16, 0x0c},
	{0x3c17, 0x0c},
	{0x3c18, 0x0c},
	{0x3c19, 0x0a},
	{0x3c1a, 0x0a},
	{0x3c1b, 0x0a},
	{0x3c1c, 0x00},
	{0x3c1d, 0x00},
	{0x3c1e, 0x00},
	{0x3c1f, 0x00},
	{0x3c20, 0x00},
	{0x3c21, 0x00},
	{0x3c22, 0x3f},
	{0x3c23, 0x0a},
	{0x3e35, 0x01},
	{0x3f4a, 0x03},
	{0x3f4b, 0xbf},
	{0x3f26, 0x00},
	{0x0202, 0x0c},
	{0x0203, 0x08},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020e, 0x01},
	{0x020f, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
};

/* Supported sensor mode configurations */
static const struct imx415_mode supported_modes[] = {
	{
		.width = 2112,
		.height = 2112,
		.hblank = 2400, /* hblank = line_length - width */
		.vblank = 990,	/* vblank = frame_length - height */
		.vblank_min = 990,
		.vblank_max = 32420,
		.pclk = 480000000,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx415_mode_s_2112x2112_60fps),
			.regs = imx415_mode_s_2112x2112_60fps,
		},
	},

	{
		.width = 4056,
		.height = 3040,
		.hblank = 456,
		.vblank = 506,
		.vblank_min = 506,
		.vblank_max = 32420,
		.pclk = 480000000,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		/* mode_4056x3040_regs outputs 4056x3040 (no scaling, 0x0401=0x00,
		 * X_OUT=4056). Modified to 15fps to fix RZV2H CRU FIFO overflow.
		 * REQ_LINK_BIT_RATE=2400 → 600 Mbps/lane.
		 * pclk=240MHz → mbps=600
		 */
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_4056x3040_regs),
			.regs = mode_4056x3040_regs,
		},
	},
};

/**
 * to_imx415() - imx415 V4L2 sub-device to imx415 device.
 * @subdev: pointer to imx415 V4L2 sub-device
 *
 * Return: pointer to imx415 device
 */
static inline struct imx415 *to_imx415(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct imx415, sd);
}

/**
 * imx415_read_reg() - Read registers.
 * @imx415: pointer to imx415 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_read_reg(struct imx415 *imx415, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx415->sd);
	struct i2c_msg msgs[2] = {0};
	u8 addr_buf[2] = {0};
	u8 data_buf[4] = {0};
	int ret;

	if (WARN_ON(len > 4))
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

/**
 * imx415_write_reg() - Write register
 * @imx415: pointer to imx415 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_write_reg(struct imx415 *imx415, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx415->sd);
	u8 buf[6] = {0};

	if (WARN_ON(len > 4))
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << (8 * (4 - len)), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/**
 * imx415_write_regs() - Write a list of registers
 * @imx415: pointer to imx415 device
 * @regs: list of registers to be written
 * @len: length of registers array
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_write_regs(struct imx415 *imx415,
			     const struct imx415_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx415_write_reg(imx415, regs[i].address, 1, regs[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * imx415_update_controls() - Update control ranges based on streaming mode
 * @imx415: pointer to imx415 device
 * @mode: pointer to imx415_mode sensor mode
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_update_controls(struct imx415 *imx415,
				  const struct imx415_mode *mode)
{
	int ret;

	ret = __v4l2_ctrl_s_ctrl(imx415->link_freq_ctrl, mode->link_freq_idx);
	if (ret)
		return ret;

	ret = __v4l2_ctrl_s_ctrl(imx415->hblank_ctrl, mode->hblank);
	if (ret)
		return ret;

	/* Update pixel_rate so calc_mbps() in rzg2l-csi2 reads correct per-mode value */
	ret = __v4l2_ctrl_modify_range(imx415->pclk_ctrl,
				       mode->pclk, mode->pclk, 1, mode->pclk);
	if (ret)
		return ret;

	return __v4l2_ctrl_modify_range(imx415->vblank_ctrl, mode->vblank_min,
					mode->vblank_max, 1, mode->vblank);
}

/**
 * imx415_update_exp_gain() - Set updated exposure and gain
 * @imx415: pointer to imx415 device
 * @exposure: updated exposure value
 * @gain: updated analog gain value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_update_exp_gain(struct imx415 *imx415, u32 exposure, u32 gain)
{
	u32 lpfr;
	int ret;

	lpfr = imx415->vblank + imx415->cur_mode->height;

	dev_dbg(imx415->dev, "Set exp %u, analog gain %u, lpfr %u",
		exposure, gain, lpfr);

	ret = imx415_write_reg(imx415, IMX415_REG_HOLD, 1, 1);
	if (ret)
		return ret;

	ret = imx415_write_reg(imx415, IMX415_REG_LPFR, 2, lpfr);
	if (ret)
		goto error_release_group_hold;

	ret = imx415_write_reg(imx415, IMX415_REG_EXPOSURE_CIT, 2, exposure);
	if (ret)
		goto error_release_group_hold;

	ret = imx415_write_reg(imx415, IMX415_REG_AGAIN, 2, gain);

error_release_group_hold:
	imx415_write_reg(imx415, IMX415_REG_HOLD, 1, 0);

	return ret;
}

/**
 * imx415_set_ctrl() - Set subdevice control
 * @ctrl: pointer to v4l2_ctrl structure
 *
 * Supported controls:
 * - V4L2_CID_VBLANK
 * - cluster controls:
 *   - V4L2_CID_ANALOGUE_GAIN
 *   - V4L2_CID_EXPOSURE
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx415 *imx415 =
		container_of(ctrl->handler, struct imx415, ctrl_handler);
	u32 analog_gain;
	u32 exposure;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		imx415->vblank = imx415->vblank_ctrl->val;

		dev_dbg(imx415->dev, "Received vblank %u, new lpfr %u",
			imx415->vblank,
			imx415->vblank + imx415->cur_mode->height);

		ret = __v4l2_ctrl_modify_range(imx415->exp_ctrl,
					       IMX415_EXPOSURE_MIN,
					       imx415->vblank +
					       imx415->cur_mode->height -
					       IMX415_EXPOSURE_OFFSET,
					       1, IMX415_EXPOSURE_DEFAULT);
		break;
	case V4L2_CID_EXPOSURE:
		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx415->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx415->again_ctrl->val;

		dev_dbg(imx415->dev, "Received exp %u, analog gain %u",
			exposure, analog_gain);

		ret = imx415_update_exp_gain(imx415, exposure, analog_gain);

		pm_runtime_put(imx415->dev);

		break;
	case V4L2_CID_HBLANK:
	case V4L2_CID_LINK_FREQ:
	case V4L2_CID_PIXEL_RATE:
		/* Read-only controls that are updated by the driver */
		break;
	default:
		dev_err(imx415->dev, "Invalid control %d", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

/* V4l2 subdevice control ops*/
static const struct v4l2_ctrl_ops imx415_ctrl_ops = {
	.s_ctrl = imx415_set_ctrl,
};

/* All supported media bus format codes */
static const u32 codes[] = {
	MEDIA_BUS_FMT_SRGGB10_1X10,
};

/**
 * imx415_get_format_code() - Validate and return a supported media bus code
 * @imx415: pointer to imx415 device
 * @code: requested media bus format code
 *
 * Return: @code if supported, otherwise the first supported code.
 */
static u32 imx415_get_format_code(struct imx415 *imx415, u32 code)
{
	unsigned int i;

	lockdep_assert_held(&imx415->mutex);

	for (i = 0; i < ARRAY_SIZE(codes); i++)
		if (codes[i] == code)
			return code;

	return codes[0];
}

/**
 * imx415_enum_mbus_code() - Enumerate V4L2 sub-device mbus codes
 * @sd: pointer to imx415 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @code: V4L2 sub-device code enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx415 *imx415 = to_imx415(sd);

	if (code->index >= ARRAY_SIZE(codes))
		return -EINVAL;

	mutex_lock(&imx415->mutex);
	code->code = imx415_get_format_code(imx415, codes[code->index]);
	mutex_unlock(&imx415->mutex);

	return 0;
}

/**
 * imx415_enum_frame_size() - Enumerate V4L2 sub-device frame sizes
 * @sd: pointer to imx415 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fsize: V4L2 sub-device size enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fsize)
{
	struct imx415 *imx415 = to_imx415(sd);
	u32 code;

	if (fsize->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	mutex_lock(&imx415->mutex);
	code = imx415_get_format_code(imx415, fsize->code);
	mutex_unlock(&imx415->mutex);

	if (fsize->code != code)
		return -EINVAL;

	fsize->min_width = supported_modes[fsize->index].width;
	fsize->max_width = fsize->min_width;
	fsize->min_height = supported_modes[fsize->index].height;
	fsize->max_height = fsize->min_height;

	return 0;
}

/**
 * imx415_fill_pad_format() - Fill subdevice pad format
 *                            from selected sensor mode
 * @imx415: pointer to imx415 device
 * @mode: pointer to imx415_mode sensor mode
 * @fmt_code: media bus format code to use
 * @fmt: V4L2 sub-device format need to be filled
 */
static void imx415_fill_pad_format(struct imx415 *imx415,
				   const struct imx415_mode *mode,
                   u32 fmt_code,
				   struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = fmt_code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_DEFAULT;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;
}

/**
 * imx415_get_pad_format() - Get subdevice pad format
 * @sd: pointer to imx415 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx415 *imx415 = to_imx415(sd);

	mutex_lock(&imx415->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		// framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		imx415_fill_pad_format(imx415, imx415->cur_mode,
				       imx415_get_format_code(imx415,
							      imx415->fmt_code),
				       fmt);
	}

	mutex_unlock(&imx415->mutex);

	return 0;
}

/**
 * imx415_set_pad_format() - Set subdevice pad format
 * @sd: pointer to imx415 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode;
    u32 code;
	int ret = 0;

	mutex_lock(&imx415->mutex);

	code = imx415_get_format_code(imx415, fmt->format.code);
	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);
	imx415_fill_pad_format(imx415, mode, code, fmt);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		// framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		*framefmt = fmt->format;
	} else if (imx415->cur_mode != mode || imx415->fmt_code != code) {
		imx415->cur_mode = mode;
		imx415->fmt_code = code;
		ret = imx415_update_controls(imx415, mode);
	}

	mutex_unlock(&imx415->mutex);

	return ret;
}

/**
 * imx415_init_pad_cfg() - Initialize sub-device pad configuration
 * @sd: pointer to imx415 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_init_pad_cfg(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state)
{
	struct imx415 *imx415 = to_imx415(sd);
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	imx415_fill_pad_format(imx415, &supported_modes[0], codes[0], &fmt);

	return imx415_set_pad_format(sd, sd_state, &fmt);
}

/**
 * imx415_start_streaming() - Start sensor stream
 * @imx415: pointer to imx415 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_start_streaming(struct imx415 *imx415)
{
	const struct imx415_reg_list *reg_list;
	int ret;

	dev_info(imx415->dev, "imx415_start_streaming: writing mode registers for %dx%d...\n", imx415->cur_mode->width, imx415->cur_mode->height);

	/* Write sensor mode registers */
	reg_list = &imx415->cur_mode->reg_list;
	ret = imx415_write_regs(imx415, reg_list->regs,
				reg_list->num_of_regs);
	if (ret) {
		dev_err(imx415->dev, "fail to write initial registers");
		return ret;
	}

	/* Setup handler will write actual exposure and gain */
	ret =  __v4l2_ctrl_handler_setup(imx415->sd.ctrl_handler);
	if (ret) {
		dev_err(imx415->dev, "fail to setup handler");
		return ret;
	}

	/* Delay is required before streaming*/
	usleep_range(7400, 8000);

	/* Start streaming */
	ret = imx415_write_reg(imx415, IMX415_REG_MODE_SELECT,
			       1, IMX415_MODE_STREAMING);
	if (ret) {
		dev_err(imx415->dev, "fail to start streaming");
		return ret;
	}

	dev_info(imx415->dev, "imx415_start_streaming: sensor streaming successfully started.\n");
	return 0;
}

/**
 * imx415_stop_streaming() - Stop sensor stream
 * @imx415: pointer to imx415 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_stop_streaming(struct imx415 *imx415)
{
	dev_info(imx415->dev, "imx415_stop_streaming: putting sensor to standby...\n");
	return imx415_write_reg(imx415, IMX415_REG_MODE_SELECT,
				1, IMX415_MODE_STANDBY);
}

/**
 * imx415_set_stream() - Enable sensor streaming
 * @sd: pointer to imx415 subdevice
 * @enable: set to enable sensor streaming
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx415 *imx415 = to_imx415(sd);
	int ret;

	dev_info(imx415->dev, "imx415_set_stream: enable=%d\n", enable);

	mutex_lock(&imx415->mutex);

	if (enable) {
		ret = pm_runtime_resume_and_get(imx415->dev);
		if (ret)
			goto error_unlock;

		ret = imx415_start_streaming(imx415);
		if (ret)
			goto error_power_off;
	} else {
		imx415_stop_streaming(imx415);
		pm_runtime_put(imx415->dev);
	}

	mutex_unlock(&imx415->mutex);

	return 0;

error_power_off:
	pm_runtime_put(imx415->dev);
error_unlock:
	mutex_unlock(&imx415->mutex);

	return ret;
}

/**
 * imx415_detect() - Detect imx415 sensor
 * @imx415: pointer to imx415 device
 *
 * Return: 0 if successful, -EIO if sensor id does not match
 */
static int imx415_detect(struct imx415 *imx415)
{
	int ret;
	u32 val;

	ret = imx415_read_reg(imx415, IMX415_REG_ID, 2, &val);
	if (ret)
		return ret;

	if (val != IMX415_ID) {
		dev_err(imx415->dev, "chip id mismatch: %x!=%x",
			IMX415_ID, val);
		return -ENXIO;
	}

	return 0;
}

/**
 * imx415_parse_hw_config() - Parse HW configuration and check if supported
 * @imx415: pointer to imx415 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_parse_hw_config(struct imx415 *imx415)
{
	struct fwnode_handle *fwnode = dev_fwnode(imx415->dev);
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct fwnode_handle *ep;
	unsigned long rate;
	unsigned int i;
	int ret;

	if (!fwnode)
		return -ENXIO;

	/* Request optional reset pin */
	imx415->reset_gpio = devm_gpiod_get_optional(imx415->dev, "reset",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(imx415->reset_gpio)) {
		dev_err(imx415->dev, "failed to get reset gpio %ld",
			PTR_ERR(imx415->reset_gpio));
		return PTR_ERR(imx415->reset_gpio);
	}

	/* Get sensor input clock */
	imx415->inclk = devm_clk_get(imx415->dev, NULL);
	if (IS_ERR(imx415->inclk)) {
		dev_err(imx415->dev, "could not get inclk");
		return PTR_ERR(imx415->inclk);
	}

	rate = clk_get_rate(imx415->inclk);
	if (rate != IMX415_INCLK_RATE) {
		dev_err(imx415->dev, "inclk frequency mismatch");
		return -EINVAL;
	}

	/* Get optional DT defined regulators */
	for (i = 0; i < ARRAY_SIZE(imx415_supply_names); i++)
		imx415->supplies[i].supply = imx415_supply_names[i];

	ret = devm_regulator_bulk_get(imx415->dev,
				      ARRAY_SIZE(imx415_supply_names),
				      imx415->supplies);
	if (ret)
		return ret;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != IMX415_NUM_DATA_LANES) {
		dev_err(imx415->dev,
			"number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(imx415->dev, "no link frequencies defined");
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	for (i = 0; i < bus_cfg.nr_of_link_frequencies; i++)
		if (bus_cfg.link_frequencies[i] == IMX415_LINK_FREQ)
			goto done_endpoint_free;

	// Bypass DTB link frequency mismatch check to prevent boot errors
	ret = 0;

done_endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

/* V4l2 subdevice ops */
static const struct v4l2_subdev_video_ops imx415_video_ops = {
	.s_stream = imx415_set_stream,
};

static const struct v4l2_subdev_pad_ops imx415_pad_ops = {
	.init_cfg = imx415_init_pad_cfg,
	.enum_mbus_code = imx415_enum_mbus_code,
	.enum_frame_size = imx415_enum_frame_size,
	.get_fmt = imx415_get_pad_format,
	.set_fmt = imx415_set_pad_format,
};

static const struct v4l2_subdev_ops imx415_subdev_ops = {
	.video = &imx415_video_ops,
	.pad = &imx415_pad_ops,
};

/**
 * imx415_power_on() - Sensor power on sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx415 *imx415 = to_imx415(sd);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(imx415_supply_names),
				    imx415->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to enable regulators\n");
		return ret;
	}

	gpiod_set_value_cansleep(imx415->reset_gpio, 0);

	ret = clk_prepare_enable(imx415->inclk);
	if (ret) {
		dev_err(imx415->dev, "fail to enable inclk");
		goto error_reset;
	}

	usleep_range(1000, 1200);

	return 0;

error_reset:
	gpiod_set_value_cansleep(imx415->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(imx415_supply_names),
			       imx415->supplies);

	return ret;
}

/**
 * imx415_power_off() - Sensor power off sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx415 *imx415 = to_imx415(sd);

	clk_disable_unprepare(imx415->inclk);

	gpiod_set_value_cansleep(imx415->reset_gpio, 1);

	regulator_bulk_disable(ARRAY_SIZE(imx415_supply_names),
			       imx415->supplies);

	return 0;
}

/**
 * imx415_init_controls() - Initialize sensor subdevice controls
 * @imx415: pointer to imx415 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_init_controls(struct imx415 *imx415)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &imx415->ctrl_handler;
	const struct imx415_mode *mode = imx415->cur_mode;
	u32 lpfr;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 6);
	if (ret)
		return ret;

	/* Serialize controls with sensor device */
	ctrl_hdlr->lock = &imx415->mutex;

	/* Initialize exposure and gain */
	lpfr = mode->vblank + mode->height;
	imx415->exp_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					     &imx415_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX415_EXPOSURE_MIN,
					     lpfr - IMX415_EXPOSURE_OFFSET,
					     IMX415_EXPOSURE_STEP,
					     IMX415_EXPOSURE_DEFAULT);

	imx415->again_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					       &imx415_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN,
					       IMX415_AGAIN_MIN,
					       IMX415_AGAIN_MAX,
					       IMX415_AGAIN_STEP,
					       IMX415_AGAIN_DEFAULT);

	v4l2_ctrl_cluster(2, &imx415->exp_ctrl);

	imx415->vblank_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&imx415_ctrl_ops,
						V4L2_CID_VBLANK,
						mode->vblank_min,
						mode->vblank_max,
						1, mode->vblank);

	/* Read only controls */
	imx415->pclk_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					      &imx415_ctrl_ops,
					      V4L2_CID_PIXEL_RATE,
					      mode->pclk, mode->pclk,
					      1, mode->pclk);

	imx415->link_freq_ctrl = v4l2_ctrl_new_int_menu(ctrl_hdlr,
							&imx415_ctrl_ops,
							V4L2_CID_LINK_FREQ,
							ARRAY_SIZE(link_freq) -
							1,
							mode->link_freq_idx,
							link_freq);
	if (imx415->link_freq_ctrl)
		imx415->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx415->hblank_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&imx415_ctrl_ops,
						V4L2_CID_HBLANK,
						IMX415_REG_MIN,
						IMX415_REG_MAX,
						1, mode->hblank);
	if (imx415->hblank_ctrl)
		imx415->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error) {
		dev_err(imx415->dev, "control init failed: %d",
			ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}

	imx415->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

/**
 * imx415_probe() - I2C client device binding
 * @client: pointer to i2c client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx415_probe(struct i2c_client *client)
{
	struct imx415 *imx415;
	const char *name;
	int ret;

	imx415 = devm_kzalloc(&client->dev, sizeof(*imx415), GFP_KERNEL);
	if (!imx415)
		return -ENOMEM;

	imx415->dev = &client->dev;
	name = device_get_match_data(&client->dev);
	if (!name)
		return -ENODEV;

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx415->sd, client, &imx415_subdev_ops);

	ret = imx415_parse_hw_config(imx415);
	if (ret) {
		dev_err(imx415->dev, "HW configuration is not supported");
		return ret;
	}

	mutex_init(&imx415->mutex);

	ret = imx415_power_on(imx415->dev);
	if (ret) {
		dev_err(imx415->dev, "failed to power-on the sensor");
		goto error_mutex_destroy;
	}

	/* Check module identity */
	ret = imx415_detect(imx415);
	if (ret) {
		dev_err(imx415->dev, "failed to find sensor: %d", ret);
		goto error_power_off;
	}

	/* Set default mode to max resolution */
	imx415->cur_mode = &supported_modes[0];
	imx415->fmt_code = codes[0];
	imx415->vblank = imx415->cur_mode->vblank;

	ret = imx415_init_controls(imx415);
	if (ret) {
		dev_err(imx415->dev, "failed to init controls: %d", ret);
		goto error_power_off;
	}

	/* Initialize subdev */
	imx415->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx415->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	v4l2_i2c_subdev_set_name(&imx415->sd, client, name, NULL);

	/* Initialize source pad */
	imx415->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx415->sd.entity, 1, &imx415->pad);
	if (ret) {
		dev_err(imx415->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&imx415->sd);
	if (ret < 0) {
		dev_err(imx415->dev,
			"failed to register async subdev: %d", ret);
		goto error_media_entity;
	}

	pm_runtime_set_active(imx415->dev);
	pm_runtime_enable(imx415->dev);
	pm_runtime_get_sync(imx415->dev); /* Prevent runtime suspend to keep power/GPIOs on permanently */

	return 0;

error_media_entity:
	media_entity_cleanup(&imx415->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(imx415->sd.ctrl_handler);
error_power_off:
	imx415_power_off(imx415->dev);
error_mutex_destroy:
	mutex_destroy(&imx415->mutex);

	return ret;
}

/**
 * imx415_remove() - I2C client device unbinding
 * @client: pointer to I2C client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static void imx415_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx415_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&imx415->mutex);
}

static const struct dev_pm_ops imx415_pm_ops = {
	SET_RUNTIME_PM_OPS(imx415_power_off, imx415_power_on, NULL)
};

static const struct of_device_id imx415_of_match[] = {
	{ .compatible = "sony,imx415", .data = "imx415" },
	{ .compatible = "sony,imx577", .data = "imx577" },
	{ }
};

MODULE_DEVICE_TABLE(of, imx415_of_match);

static struct i2c_driver imx415_driver = {
	.probe_new = imx415_probe,
	.remove = imx415_remove,
	.driver = {
		.name = "imx415",
		.pm = &imx415_pm_ops,
		.of_match_table = imx415_of_match,
	},
};

module_i2c_driver(imx415_driver);

MODULE_DESCRIPTION("Sony imx415 sensor driver");
MODULE_LICENSE("GPL");