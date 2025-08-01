/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*******************************************************************************
 * NOTICE
 * The LL layer for ESP32C5 SPI register operations
 * It is NOT public api, don't use in application code.
 ******************************************************************************/

#pragma once

#include <stdlib.h> //for abs()
#include <string.h>
#include "esp_types.h"
#include "soc/spi_periph.h"
#include "soc/spi_struct.h"
#include "soc/lldesc.h"
#include "soc/clk_tree_defs.h"
#include "hal/assert.h"
#include "hal/misc.h"
#include "hal/spi_types.h"
#include "soc/pcr_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Interrupt not used. Don't use in app.
#define SPI_LL_UNUSED_INT_MASK  (SPI_TRANS_DONE_INT_ENA | SPI_SLV_WR_DMA_DONE_INT_ENA | SPI_SLV_RD_DMA_DONE_INT_ENA | SPI_SLV_WR_BUF_DONE_INT_ENA | SPI_SLV_RD_BUF_DONE_INT_ENA)
/// These 2 masks together will set SPI transaction to one line mode
#define SPI_LL_ONE_LINE_CTRL_MASK (SPI_FREAD_QUAD | SPI_FREAD_DUAL | SPI_FCMD_QUAD | SPI_FCMD_DUAL | SPI_FADDR_QUAD | SPI_FADDR_DUAL)
#define SPI_LL_ONE_LINE_USER_MASK (SPI_FWRITE_QUAD | SPI_FWRITE_DUAL)
/// Swap the bit order to its correct place to send
#define HAL_SPI_SWAP_DATA_TX(data, len) HAL_SWAP32((uint32_t)(data) << (32 - len))
#define SPI_LL_GET_HW(ID) (((ID)==1) ? &GPSPI2 : NULL)

#define SPI_LL_DMA_MAX_BIT_LEN    SPI_MS_DATA_BITLEN
#define SPI_LL_CPU_MAX_BIT_LEN    (16 * 32)    //Fifo len: 16 words
#define SPI_LL_MOSI_FREE_LEVEL    1            //Default level after bus initialized
#define SPI_LL_SUPPORT_CLK_SRC_PRE_DIV      1  //clock source have divider before peripheral
#define SPI_LL_PERIPH_CLK_DIV_MAX   ((SPI_CLKCNT_N + 1) * (SPI_CLKDIV_PRE + 1)) //peripheral internal maxmum clock divider

/**
 * The data structure holding calculated clock configuration. Since the
 * calculation needs long time, it should be calculated during initialization and
 * stored somewhere to be quickly used.
 */
typedef uint32_t spi_ll_clock_val_t;
typedef spi_dev_t spi_dma_dev_t;

// Type definition of all supported interrupts
typedef enum {
    SPI_LL_INTR_TRANS_DONE =    BIT(0),     ///< A transaction has done
    SPI_LL_INTR_RDBUF =         BIT(6),     ///< Has received RDBUF command. Only available in slave HD.
    SPI_LL_INTR_WRBUF =         BIT(7),     ///< Has received WRBUF command. Only available in slave HD.
    SPI_LL_INTR_RDDMA =         BIT(8),     ///< Has received RDDMA command. Only available in slave HD.
    SPI_LL_INTR_WRDMA =         BIT(9),     ///< Has received WRDMA command. Only available in slave HD.
    SPI_LL_INTR_CMD7  =         BIT(10),    ///< Has received CMD7 command. Only available in slave HD.
    SPI_LL_INTR_CMD8 =          BIT(11),    ///< Has received CMD8 command. Only available in slave HD.
    SPI_LL_INTR_CMD9 =          BIT(12),    ///< Has received CMD9 command. Only available in slave HD.
    SPI_LL_INTR_CMDA =          BIT(13),    ///< Has received CMDA command. Only available in slave HD.
    SPI_LL_INTR_SEG_DONE =      BIT(14),
} spi_ll_intr_t;

// Flags for conditions under which the transaction length should be recorded
typedef enum {
    SPI_LL_TRANS_LEN_COND_WRBUF =   BIT(0), ///< WRBUF length will be recorded
    SPI_LL_TRANS_LEN_COND_RDBUF =   BIT(1), ///< RDBUF length will be recorded
    SPI_LL_TRANS_LEN_COND_WRDMA =   BIT(2), ///< WRDMA length will be recorded
    SPI_LL_TRANS_LEN_COND_RDDMA =   BIT(3), ///< RDDMA length will be recorded
} spi_ll_trans_len_cond_t;

// SPI base command
typedef enum {
    /* Slave HD Only */
    SPI_LL_BASE_CMD_HD_WRBUF    = 0x01,
    SPI_LL_BASE_CMD_HD_RDBUF    = 0x02,
    SPI_LL_BASE_CMD_HD_WRDMA    = 0x03,
    SPI_LL_BASE_CMD_HD_RDDMA    = 0x04,
    SPI_LL_BASE_CMD_HD_SEG_END  = 0x05,
    SPI_LL_BASE_CMD_HD_EN_QPI   = 0x06,
    SPI_LL_BASE_CMD_HD_WR_END   = 0x07,
    SPI_LL_BASE_CMD_HD_INT0     = 0x08,
    SPI_LL_BASE_CMD_HD_INT1     = 0x09,
    SPI_LL_BASE_CMD_HD_INT2     = 0x0A,
} spi_ll_base_command_t;

/*------------------------------------------------------------------------------
 * Control
 *----------------------------------------------------------------------------*/
/**
 * Enable peripheral register clock
 *
 * @param host_id   Peripheral index number, see `spi_host_device_t`
 * @param enable    Enable/Disable
 */
static inline void spi_ll_enable_bus_clock(spi_host_device_t host_id, bool enable)
{
    switch (host_id) {
    case SPI2_HOST:
        PCR.spi2_conf.spi2_clk_en = enable;
        break;
    default:
        HAL_ASSERT(false);
    }
}

/**
 * Reset whole peripheral register to init value defined by HW design
 *
 * @param host_id   Peripheral index number, see `spi_host_device_t`
 */
static inline void spi_ll_reset_register(spi_host_device_t host_id)
{
    switch (host_id) {
    case SPI2_HOST:
        PCR.spi2_conf.spi2_rst_en = 1;
        PCR.spi2_conf.spi2_rst_en = 0;
        break;
    default:
        HAL_ASSERT(false);
    }
}

/**
 * Enable functional output clock within peripheral
 *
 * @param host_id   Peripheral index number, see `spi_host_device_t`
 * @param enable    Enable/Disable
 */
static inline void spi_ll_enable_clock(spi_host_device_t host_id, bool enable)
{
    PCR.spi2_clkm_conf.spi2_clkm_en = enable;
}

/**
 * Select SPI peripheral clock source (master).
 *
 * @param hw Beginning address of the peripheral registers.
 * @param clk_source clock source to select, see valid sources in type `spi_clock_source_t`
 */
__attribute__((always_inline))
static inline void spi_ll_set_clk_source(spi_dev_t *hw, spi_clock_source_t clk_source)
{
    uint32_t clk_id = 0;
    switch (clk_source) {
    case SOC_MOD_CLK_PLL_F160M:
        clk_id = 1;
        break;
    case SOC_MOD_CLK_RC_FAST:
        clk_id = 2;
        break;
    case SOC_MOD_CLK_XTAL:
        clk_id = 0;
        break;
    default:
        HAL_ASSERT(false);
    }

    PCR.spi2_clkm_conf.spi2_clkm_sel = clk_id;
}

/**
 * Config clock source integrate pre_div before it enter GPSPI peripheral
 *
 * @note 1. For timing turning(e.g. input_delay) feature available, should be (mst_div >= 2)
 *       2. From peripheral limitation: (sour_freq/hs_div <= 160M) and (sour_freq/hs_div/mst_div <= 80M)
 *
 * @param hw        Beginning address of the peripheral registers.
 * @param hs_div    Timing turning clock divider: (hs_clk_o = sour_freq/hs_div)
 * @param mst_div   Functional output clock divider: (mst_clk_o = sour_freq/hs_div/mst_div)
 */
__attribute__((always_inline))
static inline void spi_ll_clk_source_pre_div(spi_dev_t *hw, uint8_t hs_div, uint8_t mst_div)
{
    // In IDF master driver 'mst_div' will be const 2 and 'hs_div' is actually pre_div temporally
    (void) hs_div;
    HAL_FORCE_MODIFY_U32_REG_FIELD(PCR.spi2_clkm_conf, spi2_clkm_div_num, mst_div - 1);
}

/**
 * Initialize SPI peripheral (master).
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_master_init(spi_dev_t *hw)
{
    //Reset timing
    hw->user1.cs_setup_time = 0;
    hw->user1.cs_hold_time = 0;

    //use all 64 bytes of the buffer
    hw->user.usr_miso_highpart = 0;
    hw->user.usr_mosi_highpart = 0;

    //Disable unused error_end condition
    hw->user1.mst_wfull_err_end_en = 0;
    hw->user2.mst_rempty_err_end_en = 0;

    //Disable unneeded ints
    hw->slave.val = 0;
    hw->user.val = 0;

    hw->dma_conf.val = 0;
    hw->dma_conf.slv_tx_seg_trans_clr_en = 1;
    hw->dma_conf.slv_rx_seg_trans_clr_en = 1;
    hw->dma_conf.dma_slv_seg_trans_en = 0;
}

/**
 * Initialize SPI peripheral (slave).
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_slave_init(spi_dev_t *hw)
{
    //Configure slave
    hw->clock.val = 0;
    hw->user.val = 0;
    hw->ctrl.val = 0;
    hw->user.doutdin = 1; //we only support full duplex
    hw->user.sio = 0;
    hw->slave.slave_mode = 1;
    hw->slave.soft_reset = 1;
    hw->slave.soft_reset = 0;
    //use all 64 bytes of the buffer
    hw->user.usr_miso_highpart = 0;
    hw->user.usr_mosi_highpart = 0;

    // Configure DMA In-Link to not be terminated when transaction bit counter exceeds
    hw->dma_conf.rx_eof_en = 0;
    hw->dma_conf.dma_slv_seg_trans_en = 0;

    //Disable unneeded ints
    hw->dma_int_ena.val &= ~SPI_LL_UNUSED_INT_MASK;
}

/**
 * Initialize SPI peripheral (slave half duplex mode)
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_slave_hd_init(spi_dev_t *hw)
{
    hw->clock.val = 0;
    hw->user.val = 0;
    hw->ctrl.val = 0;
    hw->user.doutdin = 0;
    hw->user.sio = 0;

    hw->slave.soft_reset = 1;
    hw->slave.soft_reset = 0;
    hw->slave.slave_mode = 1;
}

/**
 * Check whether user-defined transaction is done.
 *
 * @param hw Beginning address of the peripheral registers.
 *
 * @return   True if transaction is done, otherwise false.
 */
static inline bool spi_ll_usr_is_done(spi_dev_t *hw)
{
    return hw->dma_int_raw.trans_done_int_raw;
}

/**
 * Apply the register configurations and wait until it's done
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_apply_config(spi_dev_t *hw)
{
    hw->cmd.update = 1;
    while (hw->cmd.update);    //waiting config applied
}

/**
 * Trigger start of user-defined transaction.
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_user_start(spi_dev_t *hw)
{
    hw->cmd.usr = 1;
}

/**
 * Get current running command bit-mask. (Preview)
 *
 * @param hw Beginning address of the peripheral registers.
 *
 * @return   Bitmask of running command, see ``SPI_CMD_REG``. 0 if no in-flight command.
 */
static inline uint32_t spi_ll_get_running_cmd(spi_dev_t *hw)
{
    return hw->cmd.usr;
}

/**
 * Reset the slave peripheral before next transaction.
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_slave_reset(spi_dev_t *hw)
{
    hw->slave.soft_reset = 1;
    hw->slave.soft_reset = 0;
}

/**
 * Reset SPI CPU TX FIFO
 *
 * On esp32c5, this function is not separated
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_cpu_tx_fifo_reset(spi_dev_t *hw)
{
    hw->dma_conf.buf_afifo_rst = 1;
    hw->dma_conf.buf_afifo_rst = 0;
}

/**
 * Reset SPI CPU RX FIFO
 *
 * On esp32c5, this function is not separated
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_cpu_rx_fifo_reset(spi_dev_t *hw)
{
    hw->dma_conf.rx_afifo_rst = 1;
    hw->dma_conf.rx_afifo_rst = 0;
}

/**
 * Reset SPI DMA TX FIFO
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_dma_tx_fifo_reset(spi_dev_t *hw)
{
    hw->dma_conf.dma_afifo_rst = 1;
    hw->dma_conf.dma_afifo_rst = 0;
}

/**
 * Reset SPI DMA RX FIFO
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_dma_rx_fifo_reset(spi_dev_t *hw)
{
    hw->dma_conf.rx_afifo_rst = 1;
    hw->dma_conf.rx_afifo_rst = 0;
}

/**
 * Clear in fifo full error
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_infifo_full_clr(spi_dev_t *hw)
{
    hw->dma_int_clr.dma_infifo_full_err_int_clr = 1;
}

/**
 * Clear out fifo empty error
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_outfifo_empty_clr(spi_dev_t *hw)
{
    hw->dma_int_clr.dma_outfifo_empty_err_int_clr = 1;
}

/*------------------------------------------------------------------------------
 * DMA
 *----------------------------------------------------------------------------*/
/**
 * Enable/Disable RX DMA (Peripherals->DMA->RAM)
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param enable 1: enable; 2: disable
 */
static inline void spi_ll_dma_rx_enable(spi_dev_t *hw, bool enable)
{
    hw->dma_conf.dma_rx_ena = enable;
}

/**
 * Enable/Disable TX DMA (RAM->DMA->Peripherals)
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param enable 1: enable; 2: disable
 */
static inline void spi_ll_dma_tx_enable(spi_dev_t *hw, bool enable)
{
    hw->dma_conf.dma_tx_ena = enable;
}

/**
 * Configuration of RX DMA EOF interrupt generation way
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param enable 1: spi_dma_inlink_eof is set when the number of dma pushed data bytes is equal to the value of spi_slv/mst_dma_rd_bytelen[19:0] in spi dma transition.  0: spi_dma_inlink_eof is set by spi_trans_done in non-seg-trans or spi_dma_seg_trans_done in seg-trans.
 */
static inline void spi_ll_dma_set_rx_eof_generation(spi_dev_t *hw, bool enable)
{
    hw->dma_conf.rx_eof_en = enable;
}

/*------------------------------------------------------------------------------
 * Buffer
 *----------------------------------------------------------------------------*/
/**
 * Write to SPI hardware data buffer.
 *
 * @param hw             Beginning address of the peripheral registers.
 * @param buffer_to_send Address of the data to be written to the hardware data buffer.
 * @param bitlen         Length to write, in bits.
 */
static inline void spi_ll_write_buffer(spi_dev_t *hw, const uint8_t *buffer_to_send, size_t bitlen)
{
    for (int x = 0; x < bitlen; x += 32) {
        //Use memcpy to get around alignment issues for txdata
        uint32_t word;
        memcpy(&word, &buffer_to_send[x / 8], 4);
        hw->data_buf[(x / 32)].buf = word;
    }
}

/**
 * Write to SPI hardware data buffer by buffer ID (address)
 *
 * @param hw      Beginning address of the peripheral registers
 * @param byte_id Start ID (address) of the hardware buffer to be written
 * @param data    Address of the data to be written to the hardware data buffer.
 * @param len     Length to write, in bytes.
 */
static inline void spi_ll_write_buffer_byte(spi_dev_t *hw, int byte_id, uint8_t *data, int len)
{
    HAL_ASSERT(byte_id + len <= 64);
    HAL_ASSERT(len > 0);
    HAL_ASSERT(byte_id >= 0);

    while (len > 0) {
        uint32_t word;
        int offset = byte_id % 4;
        int copy_len = 4 - offset;
        if (copy_len > len) {
            copy_len = len;
        }

        //read-modify-write
        if (copy_len != 4) {
            word = hw->data_buf[byte_id / 4].buf;    //read
        }
        memcpy(((uint8_t *)&word) + offset, data, copy_len);  //modify
        hw->data_buf[byte_id / 4].buf = word;                     //write

        data += copy_len;
        byte_id += copy_len;
        len -= copy_len;
    }
}

/**
 * Read from SPI hardware data buffer.
 *
 * @param hw            Beginning address of the peripheral registers.
 * @param buffer_to_rcv Address of a buffer to read data from hardware data buffer
 * @param bitlen        Length to read, in bits.
 */
static inline void spi_ll_read_buffer(spi_dev_t *hw, uint8_t *buffer_to_rcv, size_t bitlen)
{
    for (int x = 0; x < bitlen; x += 32) {
        //Do a memcpy to get around possible alignment issues in rx_buffer
        uint32_t word = hw->data_buf[x / 32].buf;
        int len = bitlen - x;
        if (len > 32) {
            len = 32;
        }
        memcpy(&buffer_to_rcv[x / 8], &word, (len + 7) / 8);
    }
}

/**
 * Read from SPI hardware data buffer by buffer ID (address)
 *
 * @param hw      Beginning address of the peripheral registers
 * @param byte_id Start ID (address) of the hardware buffer to be read
 * @param data    Address of a buffer to read data from hardware data buffer
 * @param len     Length to read, in bytes.
 */
static inline void spi_ll_read_buffer_byte(spi_dev_t *hw, int byte_id, uint8_t *out_data, int len)
{
    while (len > 0) {
        uint32_t word = hw->data_buf[byte_id / 4].buf;
        int offset = byte_id % 4;
        int copy_len = 4 - offset;
        if (copy_len > len) {
            copy_len = len;
        }

        memcpy(out_data, ((uint8_t *)&word) + offset, copy_len);
        byte_id += copy_len;
        out_data += copy_len;
        len -= copy_len;
    }
}

/*------------------------------------------------------------------------------
 * Configs: mode
 *----------------------------------------------------------------------------*/
/**
 * Enable/disable the postive-cs feature.
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param cs     One of the CS (0-2) to enable/disable the feature.
 * @param pos_cs True to enable the feature, otherwise disable (default).
 */
static inline void spi_ll_master_set_pos_cs(spi_dev_t *hw, int cs, uint32_t pos_cs)
{
    if (pos_cs) {
        hw->misc.master_cs_pol |= (1 << cs);
    } else {
        hw->misc.master_cs_pol &= ~(1 << cs);
    }
}

/**
 * Enable/disable the LSBFIRST feature for TX data.
 *
 * @param hw       Beginning address of the peripheral registers.
 * @param lsbfirst True if LSB of TX data to be sent first, otherwise MSB is sent first (default).
 */
static inline void spi_ll_set_tx_lsbfirst(spi_dev_t *hw, bool lsbfirst)
{
    hw->ctrl.wr_bit_order = lsbfirst;
}

/**
 * Enable/disable the LSBFIRST feature for RX data.
 *
 * @param hw       Beginning address of the peripheral registers.
 * @param lsbfirst True if first bit received as LSB, otherwise as MSB (default).
 */
static inline void spi_ll_set_rx_lsbfirst(spi_dev_t *hw, bool lsbfirst)
{
    hw->ctrl.rd_bit_order = lsbfirst;
}

/**
 * Set SPI mode for the peripheral as master.
 *
 * @param hw   Beginning address of the peripheral registers.
 * @param mode SPI mode to work at, 0-3.
 */
static inline void spi_ll_master_set_mode(spi_dev_t *hw, uint8_t mode)
{
    //Configure polarity
    if (mode == 0) {
        hw->misc.ck_idle_edge = 0;
        hw->user.ck_out_edge = 0;
    } else if (mode == 1) {
        hw->misc.ck_idle_edge = 0;
        hw->user.ck_out_edge = 1;
    } else if (mode == 2) {
        hw->misc.ck_idle_edge = 1;
        hw->user.ck_out_edge = 1;
    } else if (mode == 3) {
        hw->misc.ck_idle_edge = 1;
        hw->user.ck_out_edge = 0;
    }
}

/**
 * Set SPI mode for the peripheral as slave.
 *
 * @param hw   Beginning address of the peripheral registers.
 * @param mode SPI mode to work at, 0-3.
 */
static inline void spi_ll_slave_set_mode(spi_dev_t *hw, const int mode, bool dma_used)
{
    if (mode == 0) {
        hw->misc.ck_idle_edge = 0;
        hw->user.rsck_i_edge = 0;
        hw->user.tsck_i_edge = 0;
        hw->slave.clk_mode_13 = 0;
    } else if (mode == 1) {
        hw->misc.ck_idle_edge = 0;
        hw->user.rsck_i_edge = 1;
        hw->user.tsck_i_edge = 1;
        hw->slave.clk_mode_13 = 1;
    } else if (mode == 2) {
        hw->misc.ck_idle_edge = 1;
        hw->user.rsck_i_edge = 1;
        hw->user.tsck_i_edge = 1;
        hw->slave.clk_mode_13 = 0;
    } else if (mode == 3) {
        hw->misc.ck_idle_edge = 1;
        hw->user.rsck_i_edge = 0;
        hw->user.tsck_i_edge = 0;
        hw->slave.clk_mode_13 = 1;
    }
    hw->slave.rsck_data_out = 0;
}

/**
 * Set SPI to work in full duplex or half duplex mode.
 *
 * @param hw          Beginning address of the peripheral registers.
 * @param half_duplex True to work in half duplex mode, otherwise in full duplex mode.
 */
static inline void spi_ll_set_half_duplex(spi_dev_t *hw, bool half_duplex)
{
    hw->user.doutdin = !half_duplex;
}

/**
 * Set SPI to work in SIO mode or not.
 *
 * SIO is a mode which MOSI and MISO share a line. The device MUST work in half-duplexmode.
 *
 * @param hw       Beginning address of the peripheral registers.
 * @param sio_mode True to work in SIO mode, otherwise false.
 */
static inline void spi_ll_set_sio_mode(spi_dev_t *hw, int sio_mode)
{
    hw->user.sio = sio_mode;
}

/**
 * Configure the SPI transaction line mode for the master to use.
 *
 * @param hw        Beginning address of the peripheral registers.
 * @param line_mode SPI transaction line mode to use, see ``spi_line_mode_t``.
 */
static inline void spi_ll_master_set_line_mode(spi_dev_t *hw, spi_line_mode_t line_mode)
{
    hw->ctrl.val &= ~SPI_LL_ONE_LINE_CTRL_MASK;
    hw->user.val &= ~SPI_LL_ONE_LINE_USER_MASK;
    hw->ctrl.fcmd_dual = (line_mode.cmd_lines == 2);
    hw->ctrl.fcmd_quad = (line_mode.cmd_lines == 4);
    hw->ctrl.faddr_dual = (line_mode.addr_lines == 2);
    hw->ctrl.faddr_quad = (line_mode.addr_lines == 4);
    hw->ctrl.fread_dual = (line_mode.data_lines == 2);
    hw->user.fwrite_dual = (line_mode.data_lines == 2);
    hw->ctrl.fread_quad = (line_mode.data_lines == 4);
    hw->user.fwrite_quad = (line_mode.data_lines == 4);
}

/**
 * Set the SPI slave to work in segment transaction mode
 *
 * @param hw        Beginning address of the peripheral registers.
 * @param seg_trans True to work in seg mode, otherwise false.
 */
static inline void spi_ll_slave_set_seg_mode(spi_dev_t *hw, bool seg_trans)
{
    hw->dma_conf.dma_slv_seg_trans_en = seg_trans;
}

/**
 * Select one of the CS to use in current transaction.
 *
 * @param hw    Beginning address of the peripheral registers.
 * @param cs_id The cs to use, 0-2, otherwise none of them is used.
 */
static inline void spi_ll_master_select_cs(spi_dev_t *hw, int cs_id)
{
    hw->misc.cs0_dis = (cs_id == 0) ? 0 : 1;
    hw->misc.cs1_dis = (cs_id == 1) ? 0 : 1;
    hw->misc.cs2_dis = (cs_id == 2) ? 0 : 1;
    hw->misc.cs3_dis = (cs_id == 3) ? 0 : 1;
    hw->misc.cs4_dis = (cs_id == 4) ? 0 : 1;
    hw->misc.cs5_dis = (cs_id == 5) ? 0 : 1;
}

/**
 * Keep Chip Select activated after the current transaction.
 *
 * @param hw Beginning address of the peripheral registers.
 * @param keep_active if 0 don't keep CS activated, else keep CS activated
 */
static inline void spi_ll_master_keep_cs(spi_dev_t *hw, int keep_active)
{
    hw->misc.cs_keep_active = (keep_active != 0) ? 1 : 0;
}

/*------------------------------------------------------------------------------
 * Configs: parameters
 *----------------------------------------------------------------------------*/
/**
 * Set the standard clock mode for master.
 * This config take effect only when SPI_CLK (pre-div before periph) div >=2
 *
 * @param hw  Beginning address of the peripheral registers.
 * @param enable_std True for std timing, False for half cycle delay sampling.
 */
static inline void spi_ll_master_set_rx_timing_mode(spi_dev_t *hw, spi_sampling_point_t sample_point)
{
    hw->clock.clk_edge_sel = (sample_point == SPI_SAMPLING_POINT_PHASE_1);
}

/**
 * Get if standard clock mode is supported.
 */
static inline bool spi_ll_master_is_rx_std_sample_supported(void)
{
    return true;
}

/**
 * Set the clock for master by stored value.
 *
 * @param hw  Beginning address of the peripheral registers.
 * @param val Stored clock configuration calculated before (by ``spi_ll_cal_clock``).
 */
static inline void spi_ll_master_set_clock_by_reg(spi_dev_t *hw, const spi_ll_clock_val_t *val)
{
    hw->clock.val = *(uint32_t *)val;
}

/**
 * Get the frequency of given dividers. Don't use in app.
 *
 * @param fapb APB clock of the system.
 * @param pre  Pre divider.
 * @param n    Main divider.
 *
 * @return     Frequency of given dividers.
 */
__attribute__((always_inline))
static inline int spi_ll_freq_for_pre_n(int fapb, int pre, int n)
{
    return (fapb / (pre * n));
}

/**
 * Calculate the nearest frequency available for master.
 *
 * @param fapb       APB clock of the system.
 * @param hz         Frequency desired.
 * @param duty_cycle Duty cycle desired.
 * @param out_reg    Output address to store the calculated clock configurations for the return frequency.
 *
 * @return           Actual (nearest) frequency.
 */
__attribute__((always_inline))
static inline int spi_ll_master_cal_clock(int fapb, int hz, int duty_cycle, spi_ll_clock_val_t *out_reg)
{
    typeof(GPSPI2.clock) reg = {.val = 0};
    int eff_clk;

    //In hw, n, h and l are 1-64, pre is 1-8K. Value written to register is one lower than used value.
    if (hz > ((fapb / 4) * 3)) {
        //Using Fapb directly will give us the best result here.
        reg.clkcnt_l = 0;
        reg.clkcnt_h = 0;
        reg.clkcnt_n = 0;
        reg.clkdiv_pre = 0;
        reg.clk_equ_sysclk = 1;
        eff_clk = fapb;
    } else {
        //For best duty cycle resolution, we want n to be as close to 32 as possible, but
        //we also need a pre/n combo that gets us as close as possible to the intended freq.
        //To do this, we bruteforce n and calculate the best pre to go along with that.
        //If there's a choice between pre/n combos that give the same result, use the one
        //with the higher n.
        int pre, n, h, l;
        int bestn = -1;
        int bestpre = -1;
        int besterr = 0;
        int errval;
        for (n = 2; n <= 64; n++) { //Start at 2: we need to be able to set h/l so we have at least one high and one low pulse.
            //Effectively, this does pre=round((fapb/n)/hz).
            pre = ((fapb / n) + (hz / 2)) / hz;
            if (pre <= 0) {
                pre = 1;
            }
            if (pre > 16) {
                pre = 16;
            }
            errval = abs(spi_ll_freq_for_pre_n(fapb, pre, n) - hz);
            if (bestn == -1 || errval <= besterr) {
                besterr = errval;
                bestn = n;
                bestpre = pre;
            }
        }

        n = bestn;
        pre = bestpre;
        l = n;
        //This effectively does round((duty_cycle*n)/256)
        h = (duty_cycle * n + 127) / 256;
        if (h <= 0) {
            h = 1;
        }

        reg.clk_equ_sysclk = 0;
        reg.clkcnt_n = n - 1;
        reg.clkdiv_pre = pre - 1;
        reg.clkcnt_h = h - 1;
        reg.clkcnt_l = l - 1;
        eff_clk = spi_ll_freq_for_pre_n(fapb, pre, n);
    }
    if (out_reg != NULL) {
        *(uint32_t *)out_reg = reg.val;
    }
    return eff_clk;
}

/**
 * Calculate and set clock for SPI master according to desired parameters.
 *
 * This takes long, suggest to calculate the configuration during
 * initialization by ``spi_ll_master_cal_clock`` and store the result, then
 * configure the clock by stored value when used by
 * ``spi_ll_msater_set_clock_by_reg``.
 *
 * @param hw         Beginning address of the peripheral registers.
 * @param fapb       APB clock of the system.
 * @param hz         Frequency desired.
 * @param duty_cycle Duty cycle desired.
 *
 * @return           Actual frequency that is used.
 */
static inline int spi_ll_master_set_clock(spi_dev_t *hw, int fapb, int hz, int duty_cycle)
{
    spi_ll_clock_val_t reg_val;
    int freq = spi_ll_master_cal_clock(fapb, hz, duty_cycle, &reg_val);
    spi_ll_master_set_clock_by_reg(hw, &reg_val);
    return freq;
}

/**
 * Set the mosi delay after the output edge to the signal. (Preview)
 *
 * The delay mode/num is a Espressif conception, may change in the new chips.
 *
 * @param hw         Beginning address of the peripheral registers.
 * @param delay_mode Delay mode, see TRM.
 * @param delay_num  APB clocks to delay.
 */
static inline void spi_ll_set_mosi_delay(spi_dev_t *hw, int delay_mode, int delay_num)
{
}

/**
 * Set the miso delay applied to the input signal before the internal peripheral. (Preview)
 *
 * The delay mode/num is a Espressif conception, may change in the new chips.
 *
 * @param hw         Beginning address of the peripheral registers.
 * @param delay_mode Delay mode, see TRM.
 * @param delay_num  APB clocks to delay.
 */
static inline void spi_ll_set_miso_delay(spi_dev_t *hw, int delay_mode, int delay_num)
{
}

/**
 * Set the delay of SPI clocks before the CS inactive edge after the last SPI clock.
 *
 * @param hw   Beginning address of the peripheral registers.
 * @param hold Delay of SPI clocks after the last clock, 0 to disable the hold phase.
 */
static inline void spi_ll_master_set_cs_hold(spi_dev_t *hw, int hold)
{
    hw->user1.cs_hold_time = hold;
    hw->user.cs_hold = hold ? 1 : 0;
}

/**
 * Set the delay of SPI clocks before the first SPI clock after the CS active edge.
 *
 * Note ESP32C5 doesn't support to use this feature when command/address phases
 * are used in full duplex mode.
 *
 * @param hw    Beginning address of the peripheral registers.
 * @param setup Delay of SPI clocks after the CS active edge, 0 to disable the setup phase.
 */
static inline void spi_ll_master_set_cs_setup(spi_dev_t *hw, uint8_t setup)
{
    hw->user1.cs_setup_time = setup - 1;
    hw->user.cs_setup = setup ? 1 : 0;
}

/**
 * Determine and unify the default level of mosi line when bus free
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_set_mosi_free_level(spi_dev_t *hw, bool level)
{
    hw->ctrl.d_pol = level;     //set default level for MOSI only on IDLE state
}

/*------------------------------------------------------------------------------
 * Configs: data
 *----------------------------------------------------------------------------*/
/**
 * Set the output length (master).
 * This should be called before master setting MISO(input) length
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param bitlen output length, in bits.
 */
static inline void spi_ll_set_mosi_bitlen(spi_dev_t *hw, size_t bitlen)
{
    if (bitlen > 0) {
        hw->ms_dlen.ms_data_bitlen = bitlen - 1;
    }
}

/**
 * Set the input length (master).
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param bitlen input length, in bits.
 */
static inline void spi_ll_set_miso_bitlen(spi_dev_t *hw, size_t bitlen)
{
    if (bitlen > 0) {
        hw->ms_dlen.ms_data_bitlen = bitlen - 1;
    }
}

/**
 * Set the maximum input length (slave).
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param bitlen Input length, in bits.
 */
static inline void spi_ll_slave_set_rx_bitlen(spi_dev_t *hw, size_t bitlen)
{
    //This is not used in esp32c5
}

/**
 * Set the maximum output length (slave).
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param bitlen Output length, in bits.
 */
static inline void spi_ll_slave_set_tx_bitlen(spi_dev_t *hw, size_t bitlen)
{
    //This is not used in esp32c5
}

/**
 * Set the length of command phase.
 *
 * When in 4-bit mode, the SPI cycles of the phase will be shorter. E.g. 16-bit
 * command phases takes 4 cycles in 4-bit mode.
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param bitlen Length of command phase, in bits. 0 to disable the command phase.
 */
static inline void spi_ll_set_command_bitlen(spi_dev_t *hw, int bitlen)
{
    hw->user2.usr_command_bitlen = bitlen - 1;
    hw->user.usr_command = bitlen ? 1 : 0;
}

/**
 * Set the length of address phase.
 *
 * When in 4-bit mode, the SPI cycles of the phase will be shorter. E.g. 16-bit
 * address phases takes 4 cycles in 4-bit mode.
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param bitlen Length of address phase, in bits. 0 to disable the address phase.
 */
static inline void spi_ll_set_addr_bitlen(spi_dev_t *hw, int bitlen)
{
    hw->user1.usr_addr_bitlen = bitlen - 1;
    hw->user.usr_addr = bitlen ? 1 : 0;
}

/**
 * Set the address value in an intuitive way.
 *
 * The length and lsbfirst is required to shift and swap the address to the right place.
 *
 * @param hw       Beginning address of the peripheral registers.
 * @param address  Address to set
 * @param addrlen  Length of the address phase
 * @param lsbfirst Whether the LSB first feature is enabled.
 */
static inline void spi_ll_set_address(spi_dev_t *hw, uint64_t addr, int addrlen, uint32_t lsbfirst)
{
    if (lsbfirst) {
        /* The output address start from the LSB of the highest byte, i.e.
        * addr[24] -> addr[31]
        * ...
        * addr[0] -> addr[7]
        * So swap the byte order to let the LSB sent first.
        */
        addr = HAL_SWAP32(addr);
        //otherwise only addr register is sent
        hw->addr.val = addr;
    } else {
        // shift the address to MSB of addr register.
        // output address will be sent from MSB to LSB of addr register
        hw->addr.val = addr << (32 - addrlen);
    }
}

/**
 * Set the command value in an intuitive way.
 *
 * The length and lsbfirst is required to shift and swap the command to the right place.
 *
 * @param hw       Beginning command of the peripheral registers.
 * @param command  Command to set
 * @param addrlen  Length of the command phase
 * @param lsbfirst Whether the LSB first feature is enabled.
 */
static inline void spi_ll_set_command(spi_dev_t *hw, uint16_t cmd, int cmdlen, bool lsbfirst)
{
    if (lsbfirst) {
        // The output command start from bit0 to bit 15, kept as is.
        HAL_FORCE_MODIFY_U32_REG_FIELD(hw->user2, usr_command_value, cmd);
    } else {
        /* Output command will be sent from bit 7 to 0 of command_value, and
         * then bit 15 to 8 of the same register field. Shift and swap to send
         * more straightly.
         */
        HAL_FORCE_MODIFY_U32_REG_FIELD(hw->user2, usr_command_value, HAL_SPI_SWAP_DATA_TX(cmd, cmdlen));
    }
}

/**
 * Set dummy clocks to output before RX phase (master), or clocks to skip
 * before the data phase and after the address phase (slave).
 *
 * Note this phase is also used to compensate RX timing in half duplex mode.
 *
 * @param hw      Beginning address of the peripheral registers.
 * @param dummy_n Dummy cycles used. 0 to disable the dummy phase.
 */
static inline void spi_ll_set_dummy(spi_dev_t *hw, int dummy_n)
{
    hw->user.usr_dummy = dummy_n ? 1 : 0;
    if (dummy_n > 0) {
        HAL_FORCE_MODIFY_U32_REG_FIELD(hw->user1, usr_dummy_cyclelen, dummy_n - 1);
    }
}

/**
 * Enable/disable the RX data phase.
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param enable True if RX phase exist, otherwise false.
 */
static inline void spi_ll_enable_miso(spi_dev_t *hw, int enable)
{
    hw->user.usr_miso = enable;
}

/**
 * Enable/disable the TX data phase.
 *
 * @param hw     Beginning address of the peripheral registers.
 * @param enable True if TX phase exist, otherwise false.
 */
static inline void spi_ll_enable_mosi(spi_dev_t *hw, int enable)
{
    hw->user.usr_mosi = enable;
}

/**
 * Get the received bit length of the slave.
 *
 * @param hw Beginning address of the peripheral registers.
 *
 * @return   Received bits of the slave.
 */
static inline uint32_t spi_ll_slave_get_rcv_bitlen(spi_dev_t *hw)
{
    return hw->slave1.slv_data_bitlen;
}

/*------------------------------------------------------------------------------
 * Interrupts
 *----------------------------------------------------------------------------*/
//helper macros to generate code for each interrupts
#define FOR_EACH_ITEM(op, list) do { list(op) } while(0)
#define INTR_LIST(item)    \
    item(SPI_LL_INTR_TRANS_DONE,    dma_int_ena.trans_done_int_ena,         dma_int_raw.trans_done_int_raw,         dma_int_clr.trans_done_int_clr,            dma_int_set.trans_done_int_set) \
    item(SPI_LL_INTR_RDBUF,         dma_int_ena.slv_rd_buf_done_int_ena,    dma_int_raw.slv_rd_buf_done_int_raw,    dma_int_clr.slv_rd_buf_done_int_clr,       dma_int_set.slv_rd_buf_done_int_set) \
    item(SPI_LL_INTR_WRBUF,         dma_int_ena.slv_wr_buf_done_int_ena,    dma_int_raw.slv_wr_buf_done_int_raw,    dma_int_clr.slv_wr_buf_done_int_clr,       dma_int_set.slv_wr_buf_done_int_set) \
    item(SPI_LL_INTR_RDDMA,         dma_int_ena.slv_rd_dma_done_int_ena,    dma_int_raw.slv_rd_dma_done_int_raw,    dma_int_clr.slv_rd_dma_done_int_clr,       dma_int_set.slv_rd_dma_done_int_set) \
    item(SPI_LL_INTR_WRDMA,         dma_int_ena.slv_wr_dma_done_int_ena,    dma_int_raw.slv_wr_dma_done_int_raw,    dma_int_clr.slv_wr_dma_done_int_clr,       dma_int_set.slv_wr_dma_done_int_set) \
    item(SPI_LL_INTR_SEG_DONE,      dma_int_ena.dma_seg_trans_done_int_ena, dma_int_raw.dma_seg_trans_done_int_raw, dma_int_clr.dma_seg_trans_done_int_clr,    dma_int_set.dma_seg_trans_done_int_set) \
    item(SPI_LL_INTR_CMD7,          dma_int_ena.slv_cmd7_int_ena,           dma_int_raw.slv_cmd7_int_raw,           dma_int_clr.slv_cmd7_int_clr,              dma_int_set.slv_cmd7_int_set) \
    item(SPI_LL_INTR_CMD8,          dma_int_ena.slv_cmd8_int_ena,           dma_int_raw.slv_cmd8_int_raw,           dma_int_clr.slv_cmd8_int_clr,              dma_int_set.slv_cmd8_int_set) \
    item(SPI_LL_INTR_CMD9,          dma_int_ena.slv_cmd9_int_ena,           dma_int_raw.slv_cmd9_int_raw,           dma_int_clr.slv_cmd9_int_clr,              dma_int_set.slv_cmd9_int_set) \
    item(SPI_LL_INTR_CMDA,          dma_int_ena.slv_cmda_int_ena,           dma_int_raw.slv_cmda_int_raw,           dma_int_clr.slv_cmda_int_clr,              dma_int_set.slv_cmda_int_set)


static inline void spi_ll_enable_intr(spi_dev_t *hw, spi_ll_intr_t intr_mask)
{
#define ENA_INTR(intr_bit, en_reg, ...) if (intr_mask & (intr_bit)) hw->en_reg = 1;
    FOR_EACH_ITEM(ENA_INTR, INTR_LIST);
#undef ENA_INTR
}

static inline void spi_ll_disable_intr(spi_dev_t *hw, spi_ll_intr_t intr_mask)
{
#define DIS_INTR(intr_bit, en_reg, ...) if (intr_mask & (intr_bit)) hw->en_reg = 0;
    FOR_EACH_ITEM(DIS_INTR, INTR_LIST);
#undef DIS_INTR
}

static inline void spi_ll_set_intr(spi_dev_t *hw, spi_ll_intr_t intr_mask)
{
#define SET_INTR(intr_bit, _, __, ___, set_reg) if (intr_mask & (intr_bit)) hw->set_reg = 1;
    FOR_EACH_ITEM(SET_INTR, INTR_LIST);
#undef SET_INTR
}

static inline void spi_ll_clear_intr(spi_dev_t *hw, spi_ll_intr_t intr_mask)
{
#define CLR_INTR(intr_bit, _, __, clr_reg, ...) if (intr_mask & (intr_bit)) hw->clr_reg = 1;
    FOR_EACH_ITEM(CLR_INTR, INTR_LIST);
#undef CLR_INTR
}

static inline bool spi_ll_get_intr(spi_dev_t *hw, spi_ll_intr_t intr_mask)
{
#define GET_INTR(intr_bit, _, sta_reg, ...) if (intr_mask & (intr_bit) && hw->sta_reg) return true;
    FOR_EACH_ITEM(GET_INTR, INTR_LIST);
    return false;
#undef GET_INTR
}

#undef FOR_EACH_ITEM
#undef INTR_LIST

/**
 * Disable the trans_done interrupt.
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_disable_int(spi_dev_t *hw)
{
    hw->dma_int_ena.trans_done_int_ena = 0;
}

/**
 * Clear the trans_done interrupt.
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_clear_int_stat(spi_dev_t *hw)
{
    hw->dma_int_clr.trans_done_int_clr = 1;
}

/**
 * Set the trans_done interrupt.
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_set_int_stat(spi_dev_t *hw)
{
    hw->dma_int_set.trans_done_int_set = 1;
}

/**
 * Enable the trans_done interrupt.
 *
 * @param hw Beginning address of the peripheral registers.
 */
static inline void spi_ll_enable_int(spi_dev_t *hw)
{
    hw->dma_int_ena.trans_done_int_ena = 1;
}

/*------------------------------------------------------------------------------
 * Slave HD
 *----------------------------------------------------------------------------*/
static inline void spi_ll_slave_hd_set_len_cond(spi_dev_t *hw, spi_ll_trans_len_cond_t cond_mask)
{
    hw->slave.slv_rdbuf_bitlen_en = (cond_mask & SPI_LL_TRANS_LEN_COND_RDBUF) ? 1 : 0;
    hw->slave.slv_wrbuf_bitlen_en = (cond_mask & SPI_LL_TRANS_LEN_COND_WRBUF) ? 1 : 0;
    hw->slave.slv_rddma_bitlen_en = (cond_mask & SPI_LL_TRANS_LEN_COND_RDDMA) ? 1 : 0;
    hw->slave.slv_wrdma_bitlen_en = (cond_mask & SPI_LL_TRANS_LEN_COND_WRDMA) ? 1 : 0;
}

static inline int spi_ll_slave_get_rx_byte_len(spi_dev_t *hw)
{
    return hw->slave1.slv_data_bitlen / 8;
}

static inline uint32_t spi_ll_slave_hd_get_last_addr(spi_dev_t *hw)
{
    return hw->slave1.slv_last_addr;
}

#undef SPI_LL_RST_MASK
#undef SPI_LL_UNUSED_INT_MASK

/**
 * Get the base spi command
 *
 * @param cmd_t           Command value
 */
static inline uint8_t spi_ll_get_slave_hd_base_command(spi_command_t cmd_t)
{
    uint8_t cmd_base = 0x00;
    switch (cmd_t) {
    case SPI_CMD_HD_WRBUF:
        cmd_base = SPI_LL_BASE_CMD_HD_WRBUF;
        break;
    case SPI_CMD_HD_RDBUF:
        cmd_base = SPI_LL_BASE_CMD_HD_RDBUF;
        break;
    case SPI_CMD_HD_WRDMA:
        cmd_base = SPI_LL_BASE_CMD_HD_WRDMA;
        break;
    case SPI_CMD_HD_RDDMA:
        cmd_base = SPI_LL_BASE_CMD_HD_RDDMA;
        break;
    case SPI_CMD_HD_SEG_END:
        cmd_base = SPI_LL_BASE_CMD_HD_SEG_END;
        break;
    case SPI_CMD_HD_EN_QPI:
        cmd_base = SPI_LL_BASE_CMD_HD_EN_QPI;
        break;
    case SPI_CMD_HD_WR_END:
        cmd_base = SPI_LL_BASE_CMD_HD_WR_END;
        break;
    case SPI_CMD_HD_INT0:
        cmd_base = SPI_LL_BASE_CMD_HD_INT0;
        break;
    case SPI_CMD_HD_INT1:
        cmd_base = SPI_LL_BASE_CMD_HD_INT1;
        break;
    case SPI_CMD_HD_INT2:
        cmd_base = SPI_LL_BASE_CMD_HD_INT2;
        break;
    default:
        HAL_ASSERT(cmd_base);
    }
    return cmd_base;
}

/**
 * Get the spi communication command
 *
 * @param cmd_t           Base command value
 * @param line_mode       Line mode of SPI transaction phases: CMD, ADDR, DOUT/DIN.
 */
static inline uint16_t spi_ll_get_slave_hd_command(spi_command_t cmd_t, spi_line_mode_t line_mode)
{
    uint8_t cmd_base = spi_ll_get_slave_hd_base_command(cmd_t);
    uint8_t cmd_mod = 0x00; //CMD:1-bit, ADDR:1-bit, DATA:1-bit

    if (line_mode.data_lines == 2) {
        if (line_mode.addr_lines == 2) {
            cmd_mod = 0x50; //CMD:1-bit, ADDR:2-bit, DATA:2-bit
        } else {
            cmd_mod = 0x10; //CMD:1-bit, ADDR:1-bit, DATA:2-bit
        }
    } else if (line_mode.data_lines == 4) {
        if (line_mode.addr_lines == 4) {
            cmd_mod = 0xA0; //CMD:1-bit, ADDR:4-bit, DATA:4-bit
        } else {
            cmd_mod = 0x20; //CMD:1-bit, ADDR:1-bit, DATA:4-bit
        }
    }
    if (cmd_base == SPI_LL_BASE_CMD_HD_SEG_END || cmd_base == SPI_LL_BASE_CMD_HD_EN_QPI) {
        cmd_mod = 0x00;
    }

    return cmd_base | cmd_mod;
}

/**
 * Get the dummy bits
 *
 * @param line_mode       Line mode of SPI transaction phases: CMD, ADDR, DOUT/DIN.
 */
static inline int spi_ll_get_slave_hd_dummy_bits(spi_line_mode_t line_mode)
{
    return 8;
}

#ifdef __cplusplus
}
#endif
