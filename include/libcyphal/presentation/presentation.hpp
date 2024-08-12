/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED

#include "libcyphal/transport/transport.hpp"

#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace presentation
{

class Presentation final
{
public:
    Presentation(cetl::pmr::memory_resource& memory, transport::ITransport& transport) noexcept
        : memory_{memory}
        , transport_{transport}
    {
    }

    ~Presentation() = default;

    Presentation(const Presentation&)                = delete;
    Presentation(Presentation&&) noexcept            = delete;
    Presentation& operator=(const Presentation&)     = delete;
    Presentation& operator=(Presentation&&) noexcept = delete;

private:
    // MARK: Data members:

    cetl::pmr::memory_resource& memory_;
    transport::ITransport&      transport_;

};  // Presentation

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
