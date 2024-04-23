/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CONTIGUOUS_PAYLOAD_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CONTIGUOUS_PAYLOAD_HPP_INCLUDED

#include "defines.hpp"
#include "libcyphal/types.hpp"

namespace libcyphal
{
namespace transport
{
namespace detail
{

/// @breaf Makes a contiguous payload from a list of payload fragments.
///
/// Has optimization for the case when there is only one non-empty fragment -
/// in this case there will be no memory allocation and payload copying.
/// Automatically deallocates memory (if any) when the object is destroyed.
///
/// Probably could be deleted when libcanard will start support fragmented payloads (at `canardTxPush`).
/// See https://github.com/OpenCyphal/libcanard/issues/223
///
class ContiguousPayload final
{
public:
    ContiguousPayload(cetl::pmr::memory_resource& mr, const PayloadFragments payload_fragments)
        : mr_{mr}
    {
        using Fragment = cetl::span<const cetl::byte>;

        // Count fragments skipping empty ones. Also keep tracking of the total payload size
        // and pointer to the last non-empty fragment (which will be in use for the optimization).
        //
        const auto total_non_empty_fragments =
            std::count_if(payload_fragments.begin(), payload_fragments.end(), [this](const Fragment frag) {
                if (frag.empty())
                {
                    return false;
                }
                payload_ = frag.data();
                payload_size_ += frag.size();
                return true;
            });

        if (total_non_empty_fragments > 1)
        {
            allocated_buffer_ = static_cast<cetl::byte*>(mr_.allocate(payload_size_));
            payload_          = allocated_buffer_;
            if (cetl::byte* const buffer = allocated_buffer_)
            {
                std::size_t offset = 0;
                for (const Fragment frag : payload_fragments)
                {
                    std::memcpy(&buffer[offset], frag.data(), frag.size());
                    offset += frag.size();
                }
            }
        }
    }
    ContiguousPayload(const ContiguousPayload&)                = delete;
    ContiguousPayload(ContiguousPayload&&) noexcept            = delete;
    ContiguousPayload& operator=(const ContiguousPayload&)     = delete;
    ContiguousPayload& operator=(ContiguousPayload&&) noexcept = delete;

    ~ContiguousPayload()
    {
        if (allocated_buffer_ != nullptr)
        {
            mr_.deallocate(allocated_buffer_, payload_size_);
        }
    }

    CETL_NODISCARD std::size_t size() const noexcept
    {
        return payload_size_;
    }

    CETL_NODISCARD const cetl::byte* data() const noexcept
    {
        return payload_;
    }

private:
    // MARK: Data members:

    cetl::pmr::memory_resource& mr_;
    const cetl::byte*           payload_{nullptr};
    std::size_t                 payload_size_{0};
    cetl::byte*                 allocated_buffer_{nullptr};

};  // ContiguousBytes

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CONTIGUOUS_PAYLOAD_HPP_INCLUDED
