
#include "posix/libcyphal/udp.hpp"
#include "posix/libcyphal/can.hpp"

#include "cetl/pf17/sys/memory_resource.hpp"

libcyphal::UDPBroadcaster udp_broadcaster_{libcyphal::transport::ip::v4::Address::addressFromString("127.0.0.1"),
                                           127,
                                           cetl::pf17::pmr::new_delete_resource()};

libcyphal::CANBroadcaster can_broadcaster_{"can0", 127, cetl::pf17::pmr::new_delete_resource()};

int main()
{
    const libcyphal::Status udp_result = udp_broadcaster_.initialize();
    const libcyphal::Status can_result = can_broadcaster_.initialize();
    return (udp_result.isSuccess() && can_result.isSuccess() ? 0 : -1);
}
