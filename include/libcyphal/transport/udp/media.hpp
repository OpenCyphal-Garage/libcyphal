/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED

namespace libcyphal
{
namespace transport
{
namespace udp
{

class IMedia
{
public:
    virtual ~IMedia() = default;

    // TODO: Add methods here

protected:
    IMedia()                         = default;
    IMedia(IMedia&&)                 = default;
    IMedia(const IMedia&)            = default;
    IMedia& operator=(IMedia&&)      = default;
    IMedia& operator=(const IMedia&) = default;

};  // IMedia

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED
