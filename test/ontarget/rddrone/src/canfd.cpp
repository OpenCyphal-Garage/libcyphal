/*
 * Copyright (c) 2020, NXP. All rights reserved.
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Distributed under The MIT License.
 * Based on work by Abraham Rodriguez <abraham.rodriguez@nxp.com>
 */

/*
 * Source driver file for the media layer of Libcyphal v1 targeting
 * the NXP S32K14 family of automotive grade MCU's running
 * CAN-FD at 4Mbit/s data phase and 1Mbit/s in nominal phase.
 */

/*
 * Macro for additional configuration needed when using a TJA1044 transceiver, which is used
 * in NXP's UCANS32K146 board, set to 0 when using EVB's or other boards.
 */
#ifndef LIBCYPHAL_S32K_RDDRONE_BOARD_USED
#    define LIBCYPHAL_S32K_RDDRONE_BOARD_USED 1
#endif

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

/* S32K driver header file */
#include "libcyphal/media/S32K/canfd.hpp"

#include <type_traits>

/* libcyphal core header file for static pool allocator */
#include "libcyphal/platform/memory.hpp"

/* CMSIS Core for __REV macro use */
#include "s32_core_cm4.h"

/*
 * Include desired target S32K14x memory map header file dependency,
 * defaults to S32K146 from NXP's UCANS32K146 board
 */
#include "S32K146.h"

/*
 * Preprocessor conditionals for deducing the number of CANFD FlexCAN instances in target MCU,
 * this macro is defined inside the desired memory map "S32K14x.h" included header file
 */
#if defined(MCU_S32K142) || defined(MCU_S32K144)
#    define TARGET_S32K_CANFD_COUNT (1u)

#elif defined(MCU_S32K146)
#    define TARGET_S32K_CANFD_COUNT (2u)

#elif defined(MCU_S32K148)
#    define TARGET_S32K_CANFD_COUNT (3u)

#else
#    error "No NXP S32K compatible MCU header file included"
#endif

#if !defined LIBCYPHAL_S32K_RX_FIFO_LENGTH
#    define LIBCYPHAL_S32K_RX_FIFO_LENGTH 4
#endif

// TODO: error handling and statistics.
// TODO: reuse any unused RAM in the controller.
// TODO: TASD optimization
// TODO: configurable transceiver delay setting.
// TODO: flexable clock/bit-timing configuration
// TODO: limit to 2 filters and apply each to 2-mailboxes (2x2=4) which gives us an extra mailbox per filter.

// +--------------------------------------------------------------------------+
// | PRIVATE IMPLEMENTATION AND STATIC STORAGE
// +--------------------------------------------------------------------------+

namespace libcyphal
{
namespace media
{
namespace S32K
{
namespace
{
/* Number of filters supported by a single FlexCAN instance */
constexpr static unsigned int Filter_Count = 5u;

constexpr static std::size_t MailboxCount = 7u;

/* Lookup table for NVIC IRQ numbers for each FlexCAN instance */
constexpr static std::uint32_t FlexCAN_NVIC_Indices[][2u] = {{2u, 0x20000}, {2u, 0x1000000}, {2u, 0x80000000}};

/* Array of each FlexCAN instance's addresses for dereferencing from */
constexpr static CAN_Type* FlexCAN[] = CAN_BASE_PTRS;

/* Lookup table for FlexCAN indices in PCC register */
constexpr static unsigned int PCC_FlexCAN_Index[] = {36u, 37u, 43u};

struct alignas(std::uint32_t) MessageBufferByte0 final
{
    std::uint16_t timestamp : 16;
    std::uint8_t  dlc : 4;
    std::uint8_t  rtr : 1;
    std::uint8_t  ide : 1;
    std::uint8_t  srr : 1;
    std::uint8_t  reserved0 : 1;
    std::uint8_t  mb_code : 4;
    std::uint8_t  reserved1 : 1;
    std::uint8_t  esi : 1;
    std::uint8_t  brs : 1;
    std::uint8_t  edl : 1;
};

static_assert(sizeof(MessageBufferByte0) == 4, "MessageBufferByte0 bitfield must add up to 32 bits.");

struct alignas(std::uint32_t) MessageBufferByte1 final
{
    std::uint32_t id_extended : 29;
    std::uint8_t  priority : 3;
};

static_assert(sizeof(MessageBufferByte1) == 4, "MessageBufferByte1 bitfield must add up to 32 bits.");

struct alignas(std::uint32_t) MessageBuffer final
{
    static constexpr std::size_t MTUBytes = InterfaceManager::InterfaceGroupType::FrameType::MTUBytes;
    static constexpr std::size_t MTUWords = MTUBytes / 4u;
    static_assert(MTUBytes % 4 == 0,
                  "InterfaceManager::InterfaceGroupType::FrameType::MTUBytes must be 4-byte aligned.");
    static_assert(MTUWords > 0, "MTU must be at least 1, 4-byte word.");

    union
    {
        MessageBufferByte0 fields;
        std::uint32_t      reg;
    } byte0;
    union
    {
        MessageBufferByte1 fields;
        std::uint32_t      reg;
    } byte1;
    union
    {
        std::uint32_t words[MTUWords];
        std::uint8_t  bytes[MTUBytes];
    } data;
};

static_assert(sizeof(MessageBuffer) == 4 * 18, "MessageBuffers are 18 32-bit words.");

template <std::size_t CapacityParam>
class FifoBuffer
{
public:
    static constexpr std::size_t Capacity = CapacityParam;

    FifoBuffer()
        : write_(0)
        , read_(0)
        , length_(0)
    {}

    ~FifoBuffer() = default;

    FifoBuffer(const FifoBuffer&) = delete;
    FifoBuffer(FifoBuffer&&)      = delete;
    FifoBuffer& operator=(const FifoBuffer&) = delete;
    FifoBuffer& operator=(FifoBuffer&&) = delete;

    /**
     * Only ISRs and only one ISR-at-a-time can call this method. No other
     * method can be called on this object from the ISR.
     *
     * @return {@code true} if there was room in the FIFO and the item was copied.
     *         {@code false} if there wasn't room in the FIFO.
     */
    bool push_back_from_isr(const volatile MessageBuffer& item)
    {
        if (length_ == Capacity)
        {
            return false;
        }
        MessageBuffer local_buffer = data_[write_];
        ++write_;
        if (write_ == Capacity)
        {
            write_ = 0;
        }
        ++length_;
        local_buffer.byte0.reg = item.byte0.reg;
        local_buffer.byte1.reg = item.byte1.reg;
        std::copy(&item.data.words[0], &item.data.words[MessageBuffer::MTUWords - 1], &local_buffer.data.words[0]);
        return true;
    }

    const MessageBuffer& front() const
    {
        return data_[read_];
    }

    void pop_front()
    {
        read_ = (read_ == 0) ? Capacity - 1 : read_ - 1;
        --length_;
    }

    bool empty() const
    {
        return length_ > 0;
    }

private:
    std::size_t   write_;
    std::size_t   read_;
    std::size_t   length_;
    MessageBuffer data_[CapacityParam];
};

// +--------------------------------------------------------------------------+
// | S32KFlexCan
// +--------------------------------------------------------------------------+

/**
 * Per-interface implementation.
 * @tparam FifoBufferLen    The number of message buffers to allocate in RAM. Used to overcome the lack of
 *                          FIFO DMA support in the peripheral.
 */
class S32KFlexCan final
{
public:
    S32KFlexCan(unsigned peripheral_index)
        : index_(peripheral_index)
        , fc_(FlexCAN[peripheral_index])
        , buffers_(reinterpret_cast<volatile MessageBuffer*>(FlexCAN[peripheral_index]->RAMn))
        , statistics_{0}
        , fifo_buffer_()
    {}

    ~S32KFlexCan() = default;

    S32KFlexCan(const S32KFlexCan&) = delete;
    S32KFlexCan& operator=(const S32KFlexCan&) = delete;
    S32KFlexCan(S32KFlexCan&&)                 = delete;
    S32KFlexCan& operator=(S32KFlexCan&&) = delete;

    /**
     * Configure and start the interface.
     */
    Result start(const typename InterfaceManager::InterfaceGroupType::FrameType::Filter* filter_config,
                 std::size_t                                                             filter_config_length)
    {
        /* FlexCAN instance initialization */
        PCC->PCCn[PCC_FlexCAN_Index[index_]] = PCC_PCCn_CGC_MASK; /* FlexCAN clock gating */
        fc_->MCR |= CAN_MCR_MDIS_MASK;        /* Disable FlexCAN module for clock source selection */
        fc_->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK; /* Clear any previous clock source configuration */
        fc_->CTRL1 |= CAN_CTRL1_CLKSRC_MASK;  /* Select bus clock as source (80Mhz)*/

        enterFreezeMode();

        /* Next configurations are only permitted in freeze mode */
        fc_->MCR |= CAN_MCR_FDEN_MASK |          /* Enable CANFD feature */
                    CAN_MCR_FRZ_MASK;            /* Enable freeze mode entry when HALT bit is asserted */
        fc_->CTRL2 |= CAN_CTRL2_ISOCANFDEN_MASK; /* Activate the use of ISO 11898-1 CAN-FD standard */

        // TODO: Make the CBT parametric based on the peripheral clock.

        /* CAN Bit Timing (CBT) configuration for a nominal phase of 1 Mbit/s with 80 time quantas,
            in accordance with Bosch 2012 specification, sample point at 83.75% */
        fc_->CBT |= CAN_CBT_BTF_MASK |     /* Enable extended bit timing configurations for CAN-FD for
                                              setting up separately nominal and data phase */
                    CAN_CBT_EPRESDIV(0) |  /* Prescaler divisor factor of 1 */
                    CAN_CBT_EPROPSEG(46) | /* Propagation segment of 47 time quantas */
                    CAN_CBT_EPSEG1(18) |   /* Phase buffer segment 1 of 19 time quantas */
                    CAN_CBT_EPSEG2(12) |   /* Phase buffer segment 2 of 13 time quantas */
                    CAN_CBT_ERJW(12);      /* Resynchronization jump width same as PSEG2 */

        /* CAN-FD Bit Timing (FDCBT) for a data phase of 4 Mbit/s with 20 time quantas,
            in accordance with Bosch 2012 specification, sample point at 75% */
        fc_->FDCBT |= CAN_FDCBT_FPRESDIV(0) | /* Prescaler divisor factor of 1 */
                      CAN_FDCBT_FPROPSEG(7) | /* Propagation segment of 7 time quantas
                                                 (only register that doesn't add 1) */
                      CAN_FDCBT_FPSEG1(6) |   /* Phase buffer segment 1 of 7 time quantas */
                      CAN_FDCBT_FPSEG2(4) |   /* Phase buffer segment 2 of 5 time quantas */
                      CAN_FDCBT_FRJW(4);      /* Resynchorinzation jump width same as PSEG2 */

        /* Additional CAN-FD configurations */
        fc_->FDCTRL |= CAN_FDCTRL_FDRATE_MASK | /* Enable bit rate switch in data phase of frame */
                       CAN_FDCTRL_TDCEN_MASK |  /* Enable transceiver delay compensation */
                       CAN_FDCTRL_TDCOFF(5) |   /* Setup 5 cycles for data phase sampling delay */
                       CAN_FDCTRL_MBDSR0(3);    /* Setup 64 bytes per message buffer (7 MB's) */

        /* Setup maximum number of message buffers as 7, 0th and 1st for transmission and 2nd-6th for RX */
        fc_->MCR &= ~CAN_MCR_MAXMB_MASK;                     /* Clear previous configuration of MAXMB, default is 0xF */
        fc_->MCR |= CAN_MCR_MAXMB(6) | CAN_MCR_SRXDIS_MASK | /* Disable self-reception of frames if ID matches */
                    CAN_MCR_IRMQ_MASK;                       /* Enable individual message buffer masking */

        /* Enable interrupt in NVIC for FlexCAN reception with default priority (ID = 81) */
        S32_NVIC->ISER[FlexCAN_NVIC_Indices[index_][0]] = FlexCAN_NVIC_Indices[index_][1];

        /* Enable interrupts of reception MB's (0b1111100) */
        fc_->IMASK1 = CAN_IMASK1_BUF31TO0M(124);

        return reconfigureFilters(filter_config, filter_config_length);
    }

    void isrHandler()
    {
        /* Check which RX MB caused the interrupt (0b1111100) mask for 2nd-6th MB */
        for (std::size_t mb = 2, i = (1u << 2); mb < MailboxCount; i = (i << 1), ++mb)
        {
            if (fc_->IFLAG1 & i)
            {
                const volatile MessageBuffer& buffer = buffers_[mb];

                /* Instantiate monotonic object form a resolved timestamp */
                time::Monotonic timestamp_ISR = resolveTimestamp(buffer.byte0.fields.timestamp);

                /* Receive a frame only if the buffer its under its capacity */
                if (!fifo_buffer_.push_back_from_isr(buffer))
                {
                    /* Increment the number of discarded frames due to full RX FIFO */
                    statistics_.rx_overflows += 1;
                }

                /* Clear MB interrupt flag (write 1 to clear)*/
                fc_->IFLAG1 |= i;
            }
        }
    }

    bool isReady(bool ignore_write_available)
    {
        /* Poll for available frames in RX FIFO */
        if (!fifo_buffer_.empty())
        {
            return true;
        }

        /* Check for available message buffers for transmission if ignore_write_available is false */
        else if (!ignore_write_available)
        {
            /* Poll the Inactive Message Buffer and Valid Priority Status flags for TX availability */
            if ((fc_->ESR2 & CAN_ESR2_IMB_MASK) && (fc_->ESR2 & CAN_ESR2_VPS_MASK))
            {
                return true;
            }
        }
        return false;
    }

    Result reconfigureFilters(const typename InterfaceManager::InterfaceGroupType::FrameType::Filter* filter_config,
                              std::size_t filter_config_length)
    {
        /* Input validation */
        if (filter_config_length > Filter_Count)
        {
            return Result::BadArgument;
        }

        enterFreezeMode();

        /* Message buffers are located in a dedicated RAM inside FlexCAN, they aren't affected by reset,
         * so they must be explicitly initialized, they total 128 slots of 4 words each, which sum
         * to 512 bytes, each MB is 72 byte in size ( 64 payload and 8 for headers )
         */
        std::fill(fc_->RAMn, fc_->RAMn + CAN_RAMn_COUNT, 0);

        /* Clear the reception masks before configuring the new ones needed */
        std::fill(fc_->RXIMR, fc_->RXIMR + CAN_RXIMR_COUNT, 0);

        for (std::size_t j = 0; j < filter_config_length; ++j)
        {
            /* Setup reception MB's mask from input argument */
            fc_->RXIMR[j + 2] = filter_config[j].mask;

            /* Setup word 0 (4 Bytes) for ith MB
             * Extended Data Length      (EDL) = 1
             * Bit Rate Switch           (BRS) = 1
             * Error State Indicator     (ESI) = 0
             * Message Buffer Code      (CODE) = 4 ( Active for reception and empty )
             * Substitute Remote Request (SRR) = 0
             * ID Extended Bit           (IDE) = 1
             * Remote Tx Request         (RTR) = 0
             * Data Length Code          (DLC) = 0 ( Valid for transmission only )
             * Counter Time Stamp (TIME STAMP) = 0 ( Handled by hardware )
             */
            volatile MessageBuffer& buffer = buffers_[j + 2];
            buffer.byte0.fields.timestamp  = 0;
            buffer.byte0.fields.dlc        = 0;
            buffer.byte0.fields.rtr        = 0;
            buffer.byte0.fields.ide        = 1;
            buffer.byte0.fields.srr        = 1;
            buffer.byte0.fields.mb_code    = 4;
            buffer.byte0.fields.esi        = 0;
            buffer.byte0.fields.brs        = 1;
            buffer.byte0.fields.edl        = 1;

            /* Setup Message buffers 2-7 29-bit extended ID from parameter */
            buffer.byte1.fields.id_extended = filter_config[j].id;
        }

        exitFreezeMode();

        return Result::Success;
    }

    Result read(InterfaceGroup::FrameType (&out_frames)[InterfaceGroup::RxFramesLen], std::size_t& out_frames_read)
    {
        /* Initialize return value and out_frames_read output reference value */
        Result status   = Result::SuccessNothing;
        out_frames_read = 0;

        /* Check if the ISR buffer isn't empty */
        if (!fifo_buffer_.empty())
        {
            static_assert(InterfaceGroup::RxFramesLen == 1,
                          "We did not implement reading more than one message at a time.");

            InterfaceGroup::FrameType& out_frame = out_frames[0];

            /* Get the front element of the queue buffer */
            const MessageBuffer& next_buffer = fifo_buffer_.front();

            const std::uint_fast8_t payload_len =
                InterfaceGroup::FrameType::dlcToLength(libcyphal::media::CAN::FrameDLC(next_buffer.byte0.fields.dlc));
            for (std::uint_fast8_t b = 0, w = 0; b < payload_len; b += 4, ++w)
            {
                // TODO: when libcyphal issue #309 is fixed use REV_BYTES_32(from, to)
                // to accelerate this copy.
                std::uint32_t be_word = next_buffer.data.words[w];
                out_frame.data[b + 3] = (be_word & 0xFF000000U) >> 24U;
                out_frame.data[b + 2] = (be_word & 0xFF0000U) >> 16U;
                out_frame.data[b + 1] = (be_word & 0xFF00U) >> 8U;
                out_frame.data[b + 0] = (be_word & 0xFFU);
            }

            /* Pop the front element of the queue buffer */
            fifo_buffer_.pop_front();

            /* Default RX number of frames read at once by this implementation is 1 */
            out_frames_read = InterfaceGroup::RxFramesLen;

            /* If read is successful, status is success */
            status = Result::Success;
        }

        /* Return status code */
        return status;
    }

    Result write(const InterfaceGroup::FrameType (&frames)[InterfaceGroup::TxFramesLen],
                 std::size_t  frames_len,
                 std::size_t& out_frames_written)
    {
        /* Input validation */
        if (frames_len > InterfaceGroup::TxFramesLen)
        {
            return Result::BadArgument;
        }

        /* Initialize return value status */
        Result status = Result::BufferFull;

        /* Poll the Inactive Message Buffer and Valid Priority Status flags before checking for free MB's */
        if ((fc_->ESR2 & CAN_ESR2_IMB_MASK) && (fc_->ESR2 & CAN_ESR2_VPS_MASK))
        {
            /* Look for the lowest number free MB */
            std::uint8_t mb_index = (fc_->ESR2 & CAN_ESR2_LPTM_MASK) >> CAN_ESR2_LPTM_SHIFT;

            static_assert(InterfaceGroup::TxFramesLen == 1,
                          "We did not implement writing more than one message at a time.");

            /* Proceed with the tranmission */
            status = messageBufferTransmit(frames[0], buffers_[mb_index]);

            /* Argument assignment to 1 Frame transmitted successfully */
            out_frames_written = isSuccess(status) ? 1 : 0;
        }

        /* Return status code */
        return status;
    }

    Result get_statistics(InterfaceGroup::Statistics& out_statistics) const
    {
        out_statistics.rx_overflows = statistics_.rx_overflows;
        return Result::Success;
    }

private:
    /**
     * See section 53.1.8.1 of the reference manual.
     * Idempotent helper method for entering freeze mode.
     */
    void enterFreezeMode()
    {
        if (fc_->MCR & CAN_MCR_FRZACK_MASK)
        {
            // already in freeze mode
            return;
        }

        if (fc_->MCR & CAN_MCR_MDIS_MASK)
        {
            fc_->MCR &=
                ~CAN_MCR_MDIS_MASK; /* Unset disable bit (per procedure in section 53.1.8 of the reference manual) */
        }
        fc_->MCR |= (CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK); /* Request freeze mode entry */

        /* Block for freeze mode entry waiting for about 740 nominal CAN bits (assuming 160Mhz CPU) */
        for (std::uint32_t nominal_bits = 0x1CE80; nominal_bits > 0; --nominal_bits)
        {
            if (fc_->MCR & CAN_MCR_FRZACK_MASK)
            {
                return;
            }
#if defined(LIBCYPHAL_S32K_WDREFRESH_WHILE_WAITING_FOR_FREEZE_MODE) && \
    (LIBCYPHAL_S32K_WDREFRESH_WHILE_WAITING_FOR_FREEZE_MODE)
            if (WDOG->CS & WDOG_CS_EN_MASK)
            {
                DISABLE_INTERRUPTS();
                if (WDOG->CS & WDOG_CS_CMD32EN_MASK)
                {
                    WDOG->CNT = 0xB480A602;
                }
                else
                {
                    WDOG->CNT = 0xA602;
                    WDOG->CNT = 0xB480;
                }
                ENABLE_INTERRUPTS();
            }
#endif
        }
        // timeout waiting for freeze-mode entry.
        // Per section 53.1.8.1, soft-reset the driver.
        fc_->MCR |= CAN_MCR_SOFTRST_MASK;
        while (fc_->MCR & CAN_MCR_SOFTRST_MASK)
        {
            // wait for soft-reset acknowledge.
        }

        if (fc_->MCR & CAN_MCR_MDIS_MASK)
        {
            fc_->MCR &=
                ~CAN_MCR_MDIS_MASK; /* Unset disable bit (per procedure in section 53.1.8 of the reference manual) */
        }
        // According to the datasheet, after a soft-reset you don't have to wait for MCR_FRZACK the second time?
        // This might be a misinterpretation but I'm not sure how to test this branch.
        fc_->MCR |= (CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK); /* Request freeze mode entry */
    }

    void exitFreezeMode()
    {
        /* Exit from freeze mode */
        fc_->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);
    }

    /*
     * Helper function for resolving the timestamp of a received frame from FlexCAN'S 16-bit overflowing timer.
     * Based on Pycyphal's SourceTimeResolver class from which the terms source and target are used. Note: A maximum
     * of 820 microseconds is allowed for the reception ISR to reach this function starting from a successful frame
     * reception. The computation relies in that no more than a full period from the 16-bit timestamping timer
     * running at 80Mhz have passed, this could occur in deadlocks or priority inversion scenarios since 820 uSecs
     * constitute a significant amount of cycles, if this happens, timestamps would stop being monotonic. param
     * frame_timestamp Source clock read from the FlexCAN's peripheral timer. param  instance        The interface
     * instance number used by the ISR return time::Monotonic 64-bit timestamp resolved from 16-bit Flexcan's timer
     * samples.
     */
    time::Monotonic resolveTimestamp(std::uint64_t frame_timestamp_ticks)
    {
#if defined(LIBCYPHAL_S32K_NO_TIME) && (LIBCYPHAL_S32K_NO_TIME)
        return time::Monotonic::fromMicrosecond(0);
#else
        /* Harvest the peripheral's current timestamp, this is the 16-bit overflowing source clock */
        const std::uint64_t flexCAN_timestamp_ticks = fc_->TIMER;

        /* Get an non-overflowing 64-bit timestamp, this is the target clock source */
        const std::uint64_t target_source_micros = libcyphal_media_s32k_get_monotonic_time_micros_isr_safe();

        /* Compute the delta of time that occurred in the source clock */
        const std::uint64_t source_delta_ticks = flexCAN_timestamp_ticks > frame_timestamp_ticks
                                                     ? flexCAN_timestamp_ticks - frame_timestamp_ticks
                                                     : frame_timestamp_ticks - flexCAN_timestamp_ticks;

        /* Resolve the received frame's absolute timestamp and divide by 80 due the 80Mhz clock source
         * of both the source and target timers for converting them into the desired microseconds resolution */
        const std::uint64_t source_delta_micros       = (source_delta_ticks / 80);
        const std::uint64_t resolved_timestamp_micros = target_source_micros - source_delta_micros;

        /* Instantiate the required Monotonic object from the resolved timestamp */
        return time::Monotonic::fromMicrosecond(resolved_timestamp_micros);
#endif
    }

    /**
     * Helper function for an immediate transmission through an available message buffer
     *
     * @param [in]     frame            The individual frame being transmitted.
     * @param [inout]  inout_tx_buffer  An available message buffer to utilize.
     * @return libcyphal::Result:Success after a successful transmission request.
     */
    Result messageBufferTransmit(const InterfaceGroup::FrameType& frame, volatile MessageBuffer& inout_tx_buffer) const
    {
        // TODO: revisit this when libcyphal issue #309 is fixed. In general this logic can probably be
        //       more optimal.
        const std::uint_fast8_t data_len               = frame.getDataLength();
        const std::uint_fast8_t bytes_in_last_word     = (data_len % 4);
        const std::uint_fast8_t last_byte_in_last_word = (bytes_in_last_word == 0) ? 3 : bytes_in_last_word - 1;
        const std::uint_fast8_t word_count             = (data_len / 4) + ((bytes_in_last_word == 0) ? 0 : 1);
        std::uint_fast8_t       byte_it                = 0;
        std::uint_fast8_t       last_byte_in_src       = 3;
        std::uint_fast8_t       word_it                = 0;

        while (word_count > 0 && byte_it < data_len)
        {
            if (word_it == word_count - 1)
            {
                last_byte_in_src = last_byte_in_last_word;
            }
            else
            {
                last_byte_in_src = 3;
            }
            inout_tx_buffer.data.words[word_it] = 0;
            for (std::uint8_t i = 0; byte_it + last_byte_in_src < data_len && i <= last_byte_in_src; ++i)
            {
                inout_tx_buffer.data.bytes[byte_it + i] = frame.data[byte_it + (last_byte_in_src - i)];
            }
            word_it += 1;
            byte_it += 4;
        }

        std::fill(&inout_tx_buffer.data.words[word_count], &inout_tx_buffer.data.words[MessageBuffer::MTUWords], 0u);

        inout_tx_buffer.byte1.fields.id_extended = frame.id;

        /* Fill up word 0 of frame and transmit it
         * Extended Data Length       (EDL) = 1
         * Bit Rate Switch            (BRS) = 1
         * Error State Indicator      (ESI) = 0
         * Message Buffer Code       (CODE) = 12 ( Transmit data frame )
         * Substitute Remote Request  (SRR) = 1 must be 1 for extended frames
         * ID Extended Bit            (IDE) = 1
         * Remote Tx Request          (RTR) = 0
         * Data Length Code           (DLC) = frame's dlc
         * Counter Time Stamp  (TIME STAMP) = 0 ( Handled by hardware )
         */
        inout_tx_buffer.byte0.fields.edl = 1;
        inout_tx_buffer.byte0.fields.brs = 1;
        inout_tx_buffer.byte0.fields.esi = 0;
        inout_tx_buffer.byte0.fields.srr = 1;
        inout_tx_buffer.byte0.fields.ide = 1;
        inout_tx_buffer.byte0.fields.rtr = 0;
        inout_tx_buffer.byte0.fields.dlc =
            static_cast<std::underlying_type<libcyphal::media::CAN::FrameDLC>::type>(frame.getDLC());
        // Do this last so the hardware doesn't try to use this mailbox while we're writing to it.
        inout_tx_buffer.byte0.fields.mb_code = 12;

        /* Return successful transmission request status */
        return Result::Success;
    }

    /* Index in the FlexCAN array for this peripheral. */
    const unsigned index_;

    /* Pointer into the FlexCAN array for this peripheral. */
    CAN_Type* const fc_;

    /* Structured access to the embedded RAM for this peripheral. */
    volatile MessageBuffer* const buffers_;

    /* Various statistics maintained for the peripheral. */
    volatile InterfaceGroup::Statistics statistics_;

    /* Fifo buffer between ISR and the main thread. */
    FifoBuffer<LIBCYPHAL_S32K_RX_FIFO_LENGTH> fifo_buffer_;
};

// +--------------------------------------------------------------------------+
// | S32KInterfaceGroupImpl
// +--------------------------------------------------------------------------+

/**
 * Concrete type held internally and returned to the system via
 * libcyphal::media::S32K::InterfaceManager::startInterfaceGroup
 */
template <std::size_t InterfaceCount>
class S32KInterfaceGroupImpl : public InterfaceGroup
{
public:
    static_assert(InterfaceCount > 0, "Must have at least one CAN interface to define this type.");

    S32KInterfaceGroupImpl()
        : peripheral_storage_{}
    {}

    Result start(const typename InterfaceManager::InterfaceGroupType::FrameType::Filter* filter_config,
                 std::size_t                                                             filter_config_length)
    {
        bool did_one_succeed = false;
        bool did_any_fail    = false;
        /* FlexCAN instances initialization */
        for (std::size_t i = 0; i < InterfaceCount; ++i)
        {
            S32KFlexCan* interface = new (&peripheral_storage_[i]) S32KFlexCan(i);
            if (isSuccess(interface->start(filter_config, filter_config_length)))
            {
                did_one_succeed = true;
            }
            else
            {
                did_any_fail = true;
            }
        }

        /* Clock gating and multiplexing for the pins used */
        PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK; /* Clock gating to PORT E */
        PORTE->PCR[4] |= PORT_PCR_MUX(5);                /* CAN0_RX at PORT E pin 4 */
        PORTE->PCR[5] |= PORT_PCR_MUX(5);                /* CAN0_TX at PORT E pin 5 */

#if defined(MCU_S32K146) || defined(MCU_S32K148)

        PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK; /* Clock gating to PORT A */
        PORTA->PCR[12] |= PORT_PCR_MUX(3);               /* CAN1_RX at PORT A pin 12 */
        PORTA->PCR[13] |= PORT_PCR_MUX(3);               /* CAN1_TX at PORT A pin 13 */

        /* Set to LOW the standby (STB) pin in both transceivers of the UCANS32K146 node board */
#    if defined(LIBCYPHAL_S32K_RDDRONE_BOARD_USED) && (LIBCYPHAL_S32K_RDDRONE_BOARD_USED)
        PORTE->PCR[11] |= PORT_PCR_MUX(1); /* MUX to GPIO */
        PTE->PDDR |= 1 << 11;              /* Set direction as output */
        PTE->PCOR |= 1 << 11;              /* Set the pin LOW */

        PORTE->PCR[10] |= PORT_PCR_MUX(1); /* Same as above */
        PTE->PDDR |= 1 << 10;
        PTE->PCOR |= 1 << 10;
#    endif
#endif

#if defined(MCU_S32K148)
        PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK; /* Clock gating to PORT B */
        PORTB->PCR[12] |= PORT_PCR_MUX(4);               /* CAN2_RX at PORT B pin 12 */
        PORTB->PCR[13] |= PORT_PCR_MUX(4);               /* CAN2_TX at PORT B pin 13 */
#endif
        if (did_any_fail)
        {
            return (did_one_succeed) ? Result::SuccessPartial : Result::Failure;
        }
        else
        {
            return Result::Success;
        }
    }

    virtual ~S32KInterfaceGroupImpl()
    {
        /* FlexCAN module deinitialization */
        for (std::size_t i = 0; i < InterfaceCount; ++i)
        {
            reinterpret_cast<S32KFlexCan*>(&peripheral_storage_[i])->~S32KFlexCan();
        }
    }

    virtual std::uint_fast8_t getInterfaceCount() const override
    {
        return TARGET_S32K_CANFD_COUNT;
    }

    virtual Result write(std::uint_fast8_t interface_index,
                         const FrameType (&frames)[TxFramesLen],
                         std::size_t  frames_len,
                         std::size_t& out_frames_written) override
    {
        /* Input validation */
        if (interface_index > InterfaceCount || interface_index == 0)
        {
            return Result::BadArgument;
        }
        else
        {
            return get_interface(interface_index - 1).write(frames, frames_len, out_frames_written);
        }
    }

    virtual Result read(std::uint_fast8_t interface_index,
                        FrameType (&out_frames)[RxFramesLen],
                        std::size_t& out_frames_read) override
    {
        out_frames_read = 0;

        /* Input validation */
        if (interface_index > InterfaceCount || interface_index == 0)
        {
            return Result::BadArgument;
        }
        else
        {
            return get_interface(interface_index - 1).read(out_frames, out_frames_read);
        }
    }

    virtual Result reconfigureFilters(const typename FrameType::Filter* filter_config,
                                      std::size_t                       filter_config_length) override
    {
        Result result = Result::Success;

        for (std::size_t i = 0; i < InterfaceCount; ++i)
        {
            result = get_interface(i).reconfigureFilters(filter_config, filter_config_length);
            if (isFailure(result))
            {
                return result;
            }
        }
        return result;
    }

    virtual Result select(duration::Monotonic timeout, bool ignore_write_available) override
    {
#if defined(LIBCYPHAL_S32K_NO_TIME) && (LIBCYPHAL_S32K_NO_TIME)
        return Result::NotImplemented;
#else
        /* Obtain timeout from object */
        const std::uint64_t timeout_micros =
            (timeout.toMicrosecond() < 0) ? 0u : static_cast<std::uint64_t>(timeout.toMicrosecond());

        /* Initialization of delta variable for comparison */
        const std::uint64_t start_wait_micros = libcyphal_media_s32k_get_monotonic_time_micros_isr_safe();

        /* Start of timed block */
        for (;;)
        {
            /* Poll in each of the available interfaces */
            for (std::size_t i = 0; i < InterfaceCount; ++i)
            {
                /* Poll for available frames in RX FIFO */
                if (get_interface(i).isReady(ignore_write_available))
                {
                    return Result::Success;
                }
            }

            /* Get current value of delta */
            const std::uint64_t delta_micros =
                libcyphal_media_s32k_get_monotonic_time_micros_isr_safe() - start_wait_micros;
            if (delta_micros > timeout_micros)
            {
                break;
            }
            // TODO: enter a low-power mode and wait for interrupts.
            for (std::size_t i = 0; i < 12000; ++i)
            {
                NOP();
            }
        }

        /* If this section is reached, means timeout occurred and return timeout status */
        return Result::SuccessTimeout;
#endif
    }

    /*
     * FlexCAN ISR for frame reception, implements a walkaround to the S32K1 FlexCAN's lack of a RX FIFO neither a
     * DMA triggering mechanism for CAN-FD frames in hardware. Completes in at max 7472 cycles when compiled with
     * g++ at -O3 param instance The FlexCAN peripheral instance number in which the ISR will be executed, starts at
     * 0. differing form this library's interface indexes that start at 1.
     */
    void isrHandler(std::uint8_t instance)
    {
        get_interface(instance).isrHandler();
    }

    Result get_statistics(std::uint_fast8_t interface_index, Statistics& out_statistics) const
    {
        if (interface_index > InterfaceCount || interface_index == 0)
        {
            return Result::BadArgument;
        }
        else
        {
            return get_interface(interface_index).get_statistics(out_statistics);
        }
    }

private:
    S32KFlexCan& get_interface(std::size_t index)
    {
        return *reinterpret_cast<S32KFlexCan*>(&peripheral_storage_[index]);
    }

    typename std::aligned_storage<sizeof(S32KFlexCan), alignof(S32KFlexCan)>::type peripheral_storage_[InterfaceCount];
};

/* aligned storage allocated statically to store our single InterfaceGroup for this system. */
std::aligned_storage<sizeof(S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>),
                     alignof(S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>)>::type _group_storage;

S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>* _group_singleton = nullptr;

}  // end anonymous namespace

// +--------------------------------------------------------------------------+
// | InterfaceManager
// +--------------------------------------------------------------------------+

Result InterfaceManager::startInterfaceGroup(const typename InterfaceGroupType::FrameType::Filter* filter_config,
                                             std::size_t                                           filter_config_length,
                                             InterfaceGroupPtrType&                                out_group)
{
    /* Initialize return values */
    out_group = nullptr;

    /* Input validation */
    if (filter_config_length > Filter_Count)
    {
        return Result::BadArgument;
    }

    S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>* singleton_group = _group_singleton;

    if (singleton_group != nullptr)
    {
        // Called twice or called before stopInterfaceGroup.
        return Result::Failure;
    }

    /* If function ended successfully, return address of object member of type InterfaceGroup */
    S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>* initialized_group =
        new (&_group_storage) S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>();
    Result status = initialized_group->start(filter_config, filter_config_length);

    _group_singleton = initialized_group;
    out_group        = initialized_group;

    /* Return code for start of InterfaceGroup */
    return status;
}

Result InterfaceManager::stopInterfaceGroup(InterfaceGroupPtrType& inout_group)
{
    // TODO: implement stopping.
    //       1. turn off the ISRs - read the datasheet here. There are neat edge cases where you can get one more
    //          interrupt after the ISR is disabled in some edge cases.
    //       2. stop the driver
    //       3. delete the interface group.
    (void) inout_group;
    return Result::NotImplemented;
}

std::size_t InterfaceManager::getMaxFrameFilters() const
{
    return Filter_Count;
}

}  // namespace S32K
}  // namespace media
}  // namespace libcyphal

extern "C"
{
    /*
     * Interrupt service routines handled by hardware in each frame reception, they are installed by the linker
     * in function of the number of instances available in the target MCU, the names match the ones from the defined
     * interrupt vector table from the startup code located in the startup_S32K14x.S file.
     */
    void CAN0_ORed_0_15_MB_IRQHandler()
    {
        libcyphal::media::S32K::S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>* singleton_group =
            libcyphal::media::S32K::_group_singleton;
        if (singleton_group != nullptr)
        {
            singleton_group->isrHandler(0u);
        }
    }

#if defined(MCU_S32K146) || defined(MCU_S32K148)
    /* Interrupt for the 1st FlexCAN instance if available */
    void CAN1_ORed_0_15_MB_IRQHandler()
    {
        libcyphal::media::S32K::S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>* singleton_group =
            libcyphal::media::S32K::_group_singleton;
        if (singleton_group != nullptr)
        {
            singleton_group->isrHandler(1u);
        }
    }
#endif

#if defined(MCU_S32K148)
    /* Interrupt for the 2nd FlexCAN instance if available */
    void CAN2_ORed_0_15_MB_IRQHandler()
    {
        libcyphal::media::S32K::S32KInterfaceGroupImpl<TARGET_S32K_CANFD_COUNT>* singleton_group =
            libcyphal::media::S32K::_group_singleton;
        if (singleton_group != nullptr)
        {
            singleton_group->isrHandler(2u);
        }
    }
#endif
}

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
