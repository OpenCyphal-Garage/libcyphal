/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_MEMORY_RESOURCE_MOCK_HPP_INCLUDED
#define LIBCYPHAL_MEMORY_RESOURCE_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/memory.hpp>

#include <gmock/gmock.h>

class MemoryResourceMock : public cetl::pmr::memory_resource
{
public:
    MOCK_METHOD(void*, do_allocate, (std::size_t, std::size_t), (final));
    MOCK_METHOD(void, do_deallocate, (void*, std::size_t, std::size_t));
    // NOLINTNEXTLINE(bugprone-exception-escape)
    MOCK_METHOD(bool, do_is_equal, (const memory_resource&), (const, noexcept, final));

#if (__cplusplus < CETL_CPP_STANDARD_17)
    // NOLINTNEXTLINE(bugprone-exception-escape)
    MOCK_METHOD(std::size_t, do_max_size, (), (const, noexcept, final));
    MOCK_METHOD(void*, do_reallocate, (void*, std::size_t, std::size_t, std::size_t), (final));
#endif

    void redirectExpectedCallsTo(cetl::pmr::memory_resource& mr)
    {
        using ::testing::_;

        EXPECT_CALL(*this, do_allocate(_, _))
            .WillRepeatedly([&mr](std::size_t size_bytes, std::size_t alignment) -> void* {
                return mr.allocate(size_bytes, alignment);
            });
        EXPECT_CALL(*this, do_deallocate(_, _, _))
            .WillRepeatedly([&mr](void* p, std::size_t size_bytes, std::size_t alignment) {
                mr.deallocate(p, size_bytes, alignment);
            });
        EXPECT_CALL(*this, do_is_equal(_)).WillRepeatedly([&mr](const memory_resource& rhs) {
            return mr.is_equal(rhs);
        });

#if (__cplusplus < CETL_CPP_STANDARD_17)
        EXPECT_CALL(*this, do_max_size()).WillRepeatedly([&mr]() { return mr.max_size(); });
        EXPECT_CALL(*this, do_reallocate(_, _, _, _))
            .WillRepeatedly(
                [&mr](void* ptr, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t alignment) {
                    return mr.reallocate(ptr, old_size_bytes, new_size_bytes, alignment);
                });
#endif
    }

};  // MemoryResourceMock

#endif  // LIBCYPHAL_MEMORY_RESOURCE_MOCK_HPP_INCLUDED
