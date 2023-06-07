
#include "posix/libcyphal/udp.hpp"

libcyphal::UDPBroadcaster udp_broadcaster_{libcyphal::transport::ip::v4::Address::addressFromString("127.0.0.1"), 127};

int main()
{
    const libcyphal::Status result = udp_broadcaster_.initialize();
    return (result.isSuccess() ? 0 : -1);
}
