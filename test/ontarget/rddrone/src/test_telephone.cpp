/* test of libcyphal v1 media driver layer for the NXP S32K14x family
 * of aumototive-grade MCU's, running CANFD at 4Mbit/s in data phase
 * based on work by Abraham Rodriguez <abraham.rodriguez@nxp.com>
 *
 * Description:
 * Two rddrone_uavcan boards exchange messages with each other.
 */

/* Include media layer driver for NXP S32K MCU */
#include "libcyphal/media/S32K/canfd.hpp"
#include "device_registers.h"
#include "clocks_and_modes.h"
#include "LPUART.h"
#include "FTM.h"

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

extern "C"
{
    char data = 0;
    void PORT_init(void)
    {
        /*!
         * Pins definitions
         * ===================================================
         *
         * Pin number        | Function
         * ----------------- |------------------
         * PTC6              | UART1 TX
         * PTC7              | UART1 RX
         */
        PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK; /* Enable clock for PORTC        */
        PORTC->PCR[6] |= PORT_PCR_MUX(2);                /* Port C6: MUX = ALT2, UART1 TX */
        PORTC->PCR[7] |= PORT_PCR_MUX(2);                /* Port C7: MUX = ALT2, UART1 RX */
    }

    void WDOG_disable(void)
    {
        WDOG->CNT   = 0xD928C520; /* Unlock watchdog         */
        WDOG->TOVAL = 0x0000FFFF; /* Maximum timeout value   */
        WDOG->CS    = 0x00002100; /* Disable watchdog        */
    }
}  // extern "C"

namespace
{
#if !defined(LIBCYPHAL_TEST_NODE_ID)
/* ID for the current UAVCAN node */
constexpr std::uint32_t NodeID = 1u;

#else
/* ID and for the current UAVCAN node */
constexpr std::uint32_t NodeID = LIBCYPHAL_TEST_NODE_ID;

#endif

constexpr std::uint32_t NodeMask         = 0xF0; /* All care bits mask for frame filtering */
constexpr std::uint32_t NodeMessageShift = 4u;
constexpr std::size_t   NodeFrameCount   = 1u; /* Frames transmitted each time */
constexpr std::uint32_t TestMessageId    = NodeID | (NodeMask & (1 << NodeMessageShift));
/* Size of the payload in bytes of the frame to be transmitted */
constexpr std::uint16_t payload_length = libcyphal::media::S32K::InterfaceGroup::FrameType::MTUBytes;
// TODO: make the waitstates relative to the CPU speed and the data rate. We need enough to allow
//       lower priority messages access to the bus.
/* Number of CPU ticks to insert between a message transmissions. */
constexpr unsigned int message_wait_states = 0xF2;

static_assert(payload_length % 4 == 0, "we're lazy and only handle 4-byte aligned MTU transports for this test.");

struct Statistics
{
    unsigned int tx_failures = 0;
    unsigned int rx_failures = 0;
    unsigned int rx_messages = 0;
    unsigned int cycle       = 0;
} stats;

void greenLEDInit(void)
{
    PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK; /* Enable clock for PORTD */
    PORTD->PCR[16] = PORT_PCR_MUX(1);                /* Port D16: MUX = GPIO              */
    PTD->PDDR |= 1 << 16;                            /* Port D16: Data direction = output  */
}

libcyphal::Result doTelephone(std::uint_fast8_t                       interface_index,
                              libcyphal::media::S32K::InterfaceGroup& interface_group,
                              libcyphal::media::S32K::InterfaceGroup::FrameType (
                                  &inout_telephone_frames)[libcyphal::media::S32K::InterfaceGroup::TxFramesLen],
                              unsigned int& tx_wait_states_remaining)
{
    std::size_t frames_read    = 0;
    std::size_t frames_written = 0;

    /* Modify and transmit back */
    for (std::size_t i = 0; i < libcyphal::media::S32K::InterfaceGroup::TxFramesLen; ++i)
    {
        static_assert(libcyphal::media::S32K::InterfaceGroup::TxFramesLen ==
                          libcyphal::media::S32K::InterfaceGroup::RxFramesLen,
                      "We've made the assumption that the read and write frame buffers are the same length.");

        /* Changed frame's ID for returning it back */
        inout_telephone_frames[i].id = TestMessageId;

        std::uint64_t last_two_words = 0;

        // TODO: when libcyphal issue #309 is fixed rework this. For now this implementation avoids
        //       any potential unaligned operations at the cost of being really, really ugly.
        for (std::size_t x = 0; x < 8; ++x)
        {
            last_two_words |= inout_telephone_frames[i].data[56 + x] << (8 * x);
        }
        last_two_words += 1;
        for (std::size_t x = 0; x < 8; ++x)
        {
            inout_telephone_frames[i].data[56 + x] = 0xFF & (last_two_words >> (8 * x));
        }
    }
    if (tx_wait_states_remaining == 0)
    {
        const libcyphal::Result write_status =
            interface_group.write(interface_index,
                                  inout_telephone_frames,
                                  libcyphal::media::S32K::InterfaceGroup::TxFramesLen,
                                  frames_written);
        if (libcyphal::isFailure(write_status))
        {
            stats.tx_failures += libcyphal::media::S32K::InterfaceGroup::TxFramesLen;
        }
        else
        {
            tx_wait_states_remaining = message_wait_states;
        }
    }
    else
    {
        tx_wait_states_remaining -= 1;
    }

    const libcyphal::Result read_status = interface_group.read(interface_index, inout_telephone_frames, frames_read);
    if (libcyphal::isFailure(read_status))
    {
        stats.rx_failures += 1;
    }

    if (read_status != libcyphal::Result::SuccessNothing)
    {
        stats.rx_messages += frames_read;
        stats.cycle += 1;
        if (stats.cycle % 1000)
        {
            PTD->PTOR |= 1 << 16; /* toggle output port D16 (Green LED) */
            stats.cycle = 0;
        }
    }
    return read_status;
}

/* Allocate the interface manager off of the stack. */
typename std::aligned_storage<sizeof(libcyphal::media::S32K::InterfaceManager),
                              alignof(libcyphal::media::S32K::InterfaceManager)>::type interface_manager_storage;

}  // end anonymous namespace

int main()
{
    WDOG_disable();        /* Disable WDOG to allow debugging. A watchdog is recommended for production systems. */
    SOSC_init_8MHz();      /* Initialize system oscillator for 8 MHz xtal */
    SPLL_init_160MHz();    /* Initialize SPLL to 160 MHz with 8 MHz SOSC */
    NormalRUNmode_80MHz(); /* Init clocks: 80 MHz sysclk & core, 40 MHz bus, 20 MHz flash */
    PORT_init();           /* Configure ports */

    FTM0_init(); /* Configure FTM0. */

    LPUART1_init();                                                /* Initialize LPUART @ 115200*/
    LPUART1_transmit_string("Running CAN telephone example.\n\r"); /* Transmit char string */
    LPUART1_transmit_string("My node id is ");
    {
        char node_id_string[7];
        snprintf(node_id_string, 7, "%" PRIu32 "\n\r", NodeID);
        node_id_string[6] = 0;
        LPUART1_transmit_string(node_id_string);
    }

    /* Frame's Data Length Code in function of it's payload length in bytes */
    libcyphal::media::CAN::FrameDLC test_message_dlc =
        libcyphal::media::S32K::InterfaceGroup::FrameType::lengthToDlc(payload_length);

    /* prototype for the 64-byte payload that will be exchanged between the nodes */
    static constexpr std::uint32_t test_payload[] = {0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0xDDCCBBAA,
                                                     0x00000000,
                                                     0x00000000};

    static_assert(sizeof(test_payload) == 16 * 4, "test_payload is supposed to be 64-bytes.");

    /* Instantiate factory object */
    libcyphal::media::S32K::InterfaceManager* interface_manager =
        new (&interface_manager_storage) libcyphal::media::S32K::InterfaceManager();

    /* Create pointer to Interface object */
    libcyphal::media::S32K::InterfaceManager::InterfaceGroupPtrType interface_group = nullptr;

    /* Create the message that will bounce between nodes. */
    libcyphal::media::S32K::InterfaceGroup::FrameType
        telephone_messages[libcyphal::media::S32K::InterfaceGroup::TxFramesLen] = {
            libcyphal::media::S32K::InterfaceGroup::FrameType(TestMessageId,
                                                              reinterpret_cast<const std::uint8_t*>(test_payload),
                                                              test_message_dlc)};

    /* Instantiate the filter object that the current node will apply to receiving frames */
    libcyphal::media::S32K::InterfaceGroup::FrameType::Filter test_filter(TestMessageId, NodeMask);

    /* Initialize the node with the previously defined filtering using factory method */
    libcyphal::Result status = interface_manager->startInterfaceGroup(&test_filter, 1, interface_group);

    greenLEDInit();

    if (libcyphal::isFailure(status))
    {
        LPUART1_transmit_string("Failed to start the interface group.");
        while (true)
            ;
    }

    unsigned int tx_wait_states_remaining = 0;

    /* Loop for retransmission of the frame */
    for (;;)
    {
        for (std::uint_fast8_t i = 1; i <= interface_group->getInterfaceCount(); ++i)
        {
            doTelephone(i, *interface_group, telephone_messages, tx_wait_states_remaining);
        }
        // TODO: define success criteria using the stats global and emit proper signal
        //       for the test controller to evaluate.
    }
}

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
