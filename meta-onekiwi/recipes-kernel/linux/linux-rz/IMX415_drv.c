/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2016-2022 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifdef CONFIG_VIDEO_RZV2H_ISP_IMX415_DRV_SENSOR_CTX_DEBUG
#include <linux/slab.h>
#include <linux/debugfs.h>
#endif
#include "acamera_types.h"
#include "sensor_init.h"
#include "acamera_logger.h"
#include "acamera_math.h"
#include "acamera_command_api.h"
#include "acamera_sbus_api.h"
#include "system_sensor.h"
#include "acamera_sensor_api.h"
#include "system_timer.h"
#include "IMX415_seq.h"
#include "IMX415_config.h"
#include "sensor_bus_config.h"
#include "system_timer.h"

#define AGAIN_PRECISION 12
#define DGAIN_PRECISION 8

#define PIXEL_CLOCK 74250000
#define LANE_MBPS 1000

#ifdef CONFIG_VIDEO_RZG2L_CRU_EMBEDDED_DATA
#define EMBEDDED_DATA_LINE 1
#else
#define EMBEDDED_DATA_LINE 0
#endif

#define PIXEL_AREA_4056x3040 {.horizontal = {0, 0, 0, 4056, 0, 0, 0}, .vertical = {0, 0, 0, 3040, 0, 0, 1}}
#define PIXEL_AREA_2112x2112 {.horizontal = {0, 0, 0, 2112, 0, 0, 0}, .vertical = {0, 0, 0, 2112, 0, 0, 1}}

static void sensor_get_lines_per_second( void *ctx );
static void start_streaming( void *ctx );
static void stop_streaming( void *ctx );

static sensor_mode_t supported_modes[] = {
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 30 * 256,
        .resolution.width = 4056,
        .resolution.height = 3040,
        .bits = 10,
        .exposures = 1,
        .seq_table_index = 0,
        .sensor_settings = {
            .pixel_area = PIXEL_AREA_4056x3040,
            .mbps = LANE_MBPS,
        }
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 30 * 256,
        .resolution.width = 4056,
        .resolution.height = 3040,
        .bits = 10,
        .exposures = 1,
        .seq_table_index = 1,
        .sensor_settings = {
            .pixel_area = PIXEL_AREA_4056x3040,
            .mbps = LANE_MBPS,
        }
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 60 * 256,
        .resolution.width = 2112,
        .resolution.height = 2112,
        .bits = 10,
        .exposures = 1,
        .seq_table_index = 2,
        .sensor_settings = {
            .pixel_area = PIXEL_AREA_2112x2112,
            .mbps = LANE_MBPS,
        }
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 60 * 256,
        .resolution.width = 2112,
        .resolution.height = 2112,
        .bits = 10,
        .exposures = 1,
        .seq_table_index = 3,
        .sensor_settings = {
            .pixel_area = PIXEL_AREA_2112x2112,
            .mbps = LANE_MBPS,
        }
    },
};

typedef struct _sensor_context_t {
    uint8_t address; // Sensor address for direct write (not used currently)
    uint8_t seq_width;
    uint8_t streaming_flg;
    uint16_t again[2];
    uint8_t again_change;
    uint32_t again_limit;
    uint16_t dgain[2];
    uint8_t dgain_change;
    uint32_t dgain_limit;
    uint16_t int_time_min;
    uint16_t int_time_limit;
    uint8_t change_flg;
    uint16_t int_time;
    uint32_t shr0;
    uint32_t shr1;
    uint32_t shr2;
    uint32_t int_time_S;
    uint32_t int_time_M;
    uint32_t int_time_L;
    uint32_t max_L;
    uint32_t max_M;
    uint32_t max_S;
    uint32_t rhs1;
    uint32_t rhs2;
    uint16_t HMAX;
    uint32_t VMAX;
    uint32_t wdr_mode;
    uint32_t int_max;
    acamera_sbus_t sbus;
    sensor_param_t param;
    uint16_t last_written_again;
    uint16_t last_written_int_time;
} sensor_context_t;

static sensor_context_t s_ctx[FIRMWARE_CONTEXT_NUMBER];
static int ctx_counter = 0;
static uint32_t reset_state = 0;

static const acam_reg_t **p_sensor_data = seq_table;

//--------------------RESET------------------------------------------------------------
static void sensor_hw_reset_enable( void )
{
    reset_state &= ~( 1 << ( 2 * ( ctx_counter - 1 ) ) );
    system_reset_sensor( reset_state );
    system_timer_usleep( 10000 );
}

static void sensor_hw_reset_disable( void )
{
    reset_state |= 1 << ( 2 * ( ctx_counter - 1 ) );
    system_reset_sensor( reset_state );
    system_timer_usleep( 10000 );
}

#define LOG10_2_AGAIN_PREC ( 1233 ) // log10(2) << AGAIN_PRECISION
#define LOG_TO_DB ( 20 )
#define SENSOR_AGAIN_STEP_UP ( 10 )
#define SENSOR_AGAIN_STEP_DOWN ( 3 )
#define NORMALISE_FACTOR ( LOG2_GAIN_SHIFT - AGAIN_PRECISION )
#define CONVERSION_FACTOR ( ( ( LOG10_2_AGAIN_PREC * LOG_TO_DB * SENSOR_AGAIN_STEP_UP ) / SENSOR_AGAIN_STEP_DOWN ) >> NORMALISE_FACTOR )
#define NORMALISE_REG_FACTOR ( 2 * AGAIN_PRECISION )

#define LOG2_10_GAIN_SHIFT_PREC ( 870823 )
#define LOG2_STEP_UP ( 3 )
#define LOG2_STEP_DOWN ( 200 )

static int32_t sensor_alloc_analog_gain( void *ctx, int32_t gain )
{
    sensor_context_t *p_ctx = ctx;
    uint32_t a_gain;

    if ( gain > p_ctx->again_limit ) gain = p_ctx->again_limit;

    a_gain = ( gain * CONVERSION_FACTOR ) >> NORMALISE_REG_FACTOR;

    uint32_t out_gain = ( a_gain * LOG2_10_GAIN_SHIFT_PREC * LOG2_STEP_UP ) / LOG2_STEP_DOWN;

    p_ctx->again_change = 1;
    p_ctx->again[0] = a_gain;

    return out_gain;
}

static int32_t sensor_alloc_digital_gain( void *ctx, int32_t gain )
{
    sensor_context_t *p_ctx = ctx;

    uint32_t dgain = acamera_math_exp2( gain, LOG2_GAIN_SHIFT, DGAIN_PRECISION );
    uint32_t d_gain = dgain;

    if ( dgain > p_ctx->dgain_limit ) dgain = p_ctx->dgain_limit;

    p_ctx->dgain_change = 1;
    p_ctx->dgain[0] = d_gain;

    return acamera_log2_fixed_to_fixed( dgain, DGAIN_PRECISION, LOG2_GAIN_SHIFT );
}

static void sensor_alloc_integration_time( void *ctx, uint16_t *int_time_S, uint16_t *int_time_M, uint16_t *int_time_L )
{
    sensor_context_t *p_ctx = ctx;

    switch ( p_ctx->wdr_mode ) {
    case WDR_MODE_LINEAR: // Normal mode
        if ( p_ctx->int_time != *int_time_S ) {
            if ( *int_time_S > p_ctx->int_max ) *int_time_S = p_ctx->int_max;
            if ( *int_time_S < 4 ) *int_time_S = 4;
            p_ctx->change_flg = 1;
            p_ctx->shr0 = p_ctx->VMAX - *int_time_S;
            p_ctx->int_time = *int_time_S;
        }
        break;
    }
}

static void sensor_update( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    uint8_t do_update = 0;

    if ( p_ctx->again_change && (p_ctx->again[0] != p_ctx->last_written_again) ) {
        do_update = 1;
    }
    if ( p_ctx->change_flg && (p_ctx->int_time != p_ctx->last_written_int_time) ) {
        do_update = 1;
    }

    if ( do_update ) {
        // Ghi nhóm hold trước khi cập nhật thanh ghi
        acamera_sbus_write_u8( p_sbus, 0x0104, 0x01 );

        if ( p_ctx->again_change && (p_ctx->again[0] != p_ctx->last_written_again) ) {
            // ---------- Analog Gain IMX412 -------------
            acamera_sbus_write_u8( p_sbus, 0x0205, ( uint8_t )( p_ctx->again[0] & 0xFF ) ); // LSB
            acamera_sbus_write_u8( p_sbus, 0x0204, ( uint8_t )( p_ctx->again[0] >> 8 ) );   // MSB
            p_ctx->last_written_again = p_ctx->again[0];
        }
        p_ctx->again_change = 0;

        if ( p_ctx->change_flg && (p_ctx->int_time != p_ctx->last_written_int_time) ) {
            // -------- Integration Time (CIT) IMX412 ----------
            acamera_sbus_write_u8( p_sbus, 0x0203, ( uint8_t )( p_ctx->int_time & 0xFF ) ); // LSB
            acamera_sbus_write_u8( p_sbus, 0x0202, ( uint8_t )( p_ctx->int_time >> 8 ) );   // MSB
            p_ctx->last_written_int_time = p_ctx->int_time;
        }
        p_ctx->change_flg = 0;

        // Giải phóng nhóm hold sau khi cập nhật xong
        acamera_sbus_write_u8( p_sbus, 0x0104, 0x00 );
    } else {
        p_ctx->again_change = 0;
        p_ctx->change_flg = 0;
    }
    p_ctx->again[1] = p_ctx->again[0];
    p_ctx->dgain[1] = p_ctx->dgain[0];
}

static uint16_t sensor_get_id( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;

    uint16_t sensor_id1 = acamera_sbus_read_u8( p_sbus, 0x0016 );
    uint16_t sensor_id2 = acamera_sbus_read_u8( p_sbus, 0x0017 );
    return ( ( sensor_id1 << 8 ) | ( sensor_id2 ) );
}

static void sensor_hw_conf_print( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    uint16_t HMAX;
    uint32_t VMAX;
    uint8_t is_master;

    VMAX = ( acamera_sbus_read_u8( p_sbus, 0x0340 ) << 8 ) | acamera_sbus_read_u8( p_sbus, 0x0341 );
    HMAX = ( acamera_sbus_read_u8( p_sbus, 0x0342 ) << 8 ) | acamera_sbus_read_u8( p_sbus, 0x0343 );
    is_master = 1;
    LOG( LOG_DEBUG, "sensor_hw_conf:\n%s H/VMAX %dx%d(%Xx%X)", is_master ? "master" : "slave", HMAX, VMAX, HMAX, VMAX );
}

static void sensor_set_mode( void *ctx, uint8_t mode )
{
    sensor_context_t *p_ctx = ctx;
    sensor_param_t *param = &p_ctx->param;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;

    if (mode >= array_num(supported_modes)) {
        LOG(LOG_CRIT, "invalid mode %d\n", mode);
        mode = 0;
    }
    sensor_load_sequence( p_sbus, p_ctx->seq_width, p_sensor_data, supported_modes[mode].seq_table_index);

    p_ctx->VMAX = ( acamera_sbus_read_u8( p_sbus, 0x0340 ) << 8 ) | acamera_sbus_read_u8( p_sbus, 0x0341 );
    p_ctx->int_time_limit = p_ctx->VMAX - 8;
    p_ctx->max_S = p_ctx->int_time_limit;
    p_ctx->param.integration_time_apply_delay = 2;

    p_ctx->wdr_mode = supported_modes[mode].wdr_mode;

    if ( p_ctx->streaming_flg )
        start_streaming( ctx );
    else
        stop_streaming( ctx );

    param->active.width = param->modes_table[mode].resolution.width;
    param->active.height = param->modes_table[mode].resolution.height;
    param->sensor_exp_number = param->modes_table[mode].exposures;

    p_ctx->HMAX = ( acamera_sbus_read_u8( p_sbus, 0x0342 ) << 8 ) | acamera_sbus_read_u8( p_sbus, 0x0343 );

    p_ctx->int_time_min = 8;
    p_ctx->int_max = p_ctx->int_time_limit;
    p_ctx->int_time = p_ctx->int_max / 2;

    param->total.height = p_ctx->VMAX;
    param->total.width = p_ctx->HMAX;
    param->pixels_per_line = param->total.width;
    param->mode = mode;
    sensor_get_lines_per_second( ctx );
    param->integration_time_min = p_ctx->int_time_min;
    param->integration_time_limit = p_ctx->int_time_limit;
    param->integration_time_max = p_ctx->int_time_limit;
    param->integration_time_long_max = p_ctx->int_time_limit;

    LOG( LOG_NOTICE, "RES:%dx%d\n", (int)param->active.width, (int)param->active.height );
    sensor_hw_conf_print( ctx );
}

static const sensor_param_t *sensor_get_parameters( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    return (const sensor_param_t *)&p_ctx->param;
}

static void sensor_disable_isp( void *ctx )
{
}

static void sensor_get_lines_per_second( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    sensor_param_t *param = &p_ctx->param;

    const uint32_t pixel_clock = PIXEL_CLOCK;

    param->lines_per_second = pixel_clock / p_ctx->HMAX;
}

static uint32_t read_register( void *ctx, uint32_t address )
{
    system_timer_usleep( 10 );
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    uint32_t val = acamera_sbus_read_u8( p_sbus, address );
    return val;
}

static void write_register( void *ctx, uint32_t address, uint32_t data )
{
    system_timer_usleep( 10 );
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    acamera_sbus_write_u8( p_sbus, address, data );
}

static void stop_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    p_ctx->streaming_flg = 0;
    acamera_sbus_write_u8( p_sbus, 0x0100, 0x00 ); // Standby mode
}

static void start_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    p_ctx->streaming_flg = 1;
    acamera_sbus_write_u8( p_sbus, 0x0100, 0x01 ); // Streaming mode
}

void sensor_deinit_imx415( void *ctx )
{
    if ( ctx ) {
        sensor_context_t *p_ctx = ctx;
        acamera_sbus_deinit( &p_ctx->sbus, sbus_i2c );

        if ( --ctx_counter < 0 )
            LOG( LOG_ERR, "Sensor context pointer is negative %d", ctx_counter );

        if ( ctx_counter <= 0 ) {
            reset_state = 0;
            ctx_counter = 0;
            reset_sensor_bus_counter();
        }
    }
}

//--------------------Initialization------------------------------------------------------------
void sensor_init_imx415( void **ctx, sensor_control_t *ctrl )
{
    if ( ctx_counter < FIRMWARE_CONTEXT_NUMBER ) {
        sensor_context_t *p_ctx = &s_ctx[ctx_counter];
#ifdef CONFIG_VIDEO_RZV2H_ISP_IMX415_DRV_SENSOR_CTX_DEBUG
        {
            char *dbgname = kzalloc(256, GFP_KERNEL);
            struct debugfs_blob_wrapper *desc_blob = kzalloc(sizeof(*desc_blob), GFP_KERNEL);
		    if (dbgname && desc_blob) {
        		desc_blob->data = p_ctx;
		        desc_blob->size = sizeof(*p_ctx);
                snprintf(dbgname, 256, "sensor_imx415-%d", ctx_counter);
		        debugfs_create_blob(dbgname, 0400, NULL, desc_blob);
            } else {
                printk("%s: kzalloc err\n", __func__);
            }
        }
#endif
        ctx_counter++;

        p_ctx->sbus.bus = get_next_sensor_bus_address();
        p_ctx->sbus.device = SENSOR_DEV_ADDRESS;
        p_ctx->sbus.mask = SBUS_MASK_SAMPLE_8BITS | SBUS_MASK_ADDR_16BITS | SBUS_MASK_ADDR_SWAP_BYTES | SBUS_MASK_NO_STOP;

        acamera_sbus_init( &p_ctx->sbus, sbus_i2c );

        // Initial local parameters
        p_ctx->address = SENSOR_DEV_ADDRESS;
        p_ctx->seq_width = 1;
        p_ctx->streaming_flg = 0;
        p_ctx->int_time = 8;
        p_ctx->change_flg = 0;
        p_ctx->wdr_mode = WDR_MODE_LINEAR;
        p_ctx->again_limit = 5 << LOG2_GAIN_SHIFT;
        p_ctx->dgain_limit = 0 << DGAIN_PRECISION;

        p_ctx->param.rggb_start = 3; // BGGR mode cho IMX412 (đã chuyển sang 3 để sửa lệch màu)
        p_ctx->param.sensor_exp_number = 1;
        p_ctx->param.again_log2_max = 5 << LOG2_GAIN_SHIFT;
        p_ctx->param.dgain_log2_max = 0 << LOG2_GAIN_SHIFT;
        p_ctx->param.integration_time_apply_delay = 2;
        p_ctx->param.isp_exposure_channel_delay = 0;
        p_ctx->param.modes_table = supported_modes;
        p_ctx->param.modes_num = array_num( supported_modes );
        p_ctx->param.sensor_ctx = p_ctx;
        ctrl->alloc_analog_gain = sensor_alloc_analog_gain;
        ctrl->alloc_digital_gain = sensor_alloc_digital_gain;
        ctrl->alloc_integration_time = sensor_alloc_integration_time;
        ctrl->sensor_update = sensor_update;
        ctrl->set_mode = sensor_set_mode;
        ctrl->get_id = sensor_get_id;
        ctrl->get_parameters = sensor_get_parameters;
        ctrl->disable_sensor_isp = sensor_disable_isp;
        ctrl->read_sensor_register = read_register;
        ctrl->write_sensor_register = write_register;
        ctrl->start_streaming = start_streaming;
        ctrl->stop_streaming = stop_streaming;

        // Reset sensor
        sensor_hw_reset_enable();
        sensor_hw_reset_disable();

        *ctx = p_ctx;
    } else {
        LOG( LOG_ERR, "Attempt to initialize more sensor instances than was configured. Sensor initialization failed." );
        *ctx = NULL;
    }
}
