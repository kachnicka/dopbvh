#include <catch2/catch_test_macros.hpp>

#include <vLime/Memory.h>
#include <vLime/Transfer.h>

TEST_CASE("First fit allocator basic usage", "[allocator]")
{
    lime::memory::FirstFit allocator { 1024 };

    SECTION("trivial preconditions")
    {
        REQUIRE(allocator.canHold(0));
        REQUIRE(allocator.canHold(1));
        REQUIRE(allocator.canHold(1024));
        REQUIRE_FALSE(allocator.canHold(1025));
        REQUIRE_FALSE(allocator.canHold(999999999));
    }

    SECTION("allocation: simple")
    {
        REQUIRE_FALSE(allocator.alloc(0));

        auto const chunk { allocator.alloc(1024) };
        REQUIRE(chunk);
        REQUIRE(chunk->address == 0);
        REQUIRE(chunk->size == 1024);

        REQUIRE_FALSE(allocator.canHold(0));
        REQUIRE_FALSE(allocator.canHold(1));
        REQUIRE_FALSE(allocator.alloc(1024));
        REQUIRE_FALSE(allocator.alloc(1));
    }

    SECTION("allocation: simple unaligned")
    {
        std::array chunks {
            allocator.alloc(100).value(),
            allocator.alloc(100).value(),
            allocator.alloc(100).value(),
        };
        for (auto const& chunk : chunks)
            REQUIRE(chunk.size == 100);

        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 100);
        REQUIRE(chunks[2].address == 200);

        REQUIRE(allocator.canHold(724));
        REQUIRE_FALSE(allocator.canHold(725));
    }

    SECTION("allocation: simple aligned")
    {
        std::array chunks {
            allocator.alloc(100, 128).value(),
            allocator.alloc(100, 256).value(),
            allocator.alloc(100, 512).value(),
            allocator.alloc(100, 128).value(),
        };
        for (auto const& chunk : chunks)
            REQUIRE(chunk.size == 100);

        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 256);
        REQUIRE(chunks[2].address == 512);
        REQUIRE(chunks[3].address == 640);

        REQUIRE(allocator.canHold(284));
        REQUIRE_FALSE(allocator.canHold(285));
    }

    SECTION("allocation + free: simple unaligned")
    {
        {
            auto const chunk { allocator.alloc(1024).value() };
            allocator.free(chunk.address);
            REQUIRE(allocator.canHold(1024));
        }
        {
            auto const chunk { allocator.alloc(512).value() };
            allocator.free(chunk.address);
            REQUIRE(allocator.canHold(512));
            REQUIRE_FALSE(allocator.canHold(1024));
            allocator.coalesce();
            REQUIRE(allocator.canHold(1024));
        }
        {
            std::array chunks {
                allocator.alloc(256).value(),
                allocator.alloc(256).value(),
                allocator.alloc(512).value(),
            };
            REQUIRE_FALSE(allocator.canHold(1));

            allocator.free(chunks[1].address);
            REQUIRE(allocator.canHold(256));

            auto chunk { allocator.alloc(256) };
            REQUIRE(chunk);
            chunks[1] = chunk.value();
            REQUIRE_FALSE(allocator.canHold(1));

            for (auto const& c : chunks)
                allocator.free(c.address);
            REQUIRE(allocator.canHold(512));
            REQUIRE_FALSE(allocator.canHold(1024));

            allocator.coalesce();
            REQUIRE(allocator.canHold(1024));
        }
    }
}

TEST_CASE("First fit allocator, default granularity", "[allocator]")
{
    lime::memory::FirstFit allocator { 1024 };
    SECTION("All resources are linear")
    {
        std::array chunks {
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, true).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 128);
        REQUIRE(chunks[2].address == 256);
    }

    SECTION("All resources are non-linear")
    {
        std::array chunks {
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, false).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 128);
        REQUIRE(chunks[2].address == 256);
    }

    SECTION("Resources are interleaved")
    {
        std::array chunks {
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, true).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 128);
        REQUIRE(chunks[2].address == 256);
    }

    SECTION("Resources are interleaved")
    {
        std::array chunks {
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, false).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 128);
        REQUIRE(chunks[2].address == 256);
    }
}

TEST_CASE("First fit allocator, custom granularity", "[allocator]")
{
    lime::memory::FirstFit allocator { 1024, 256 };
    SECTION("All resources are linear")
    {
        std::array chunks {
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, true).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 128);
        REQUIRE(chunks[2].address == 256);
    }

    SECTION("All resources are non-linear")
    {
        std::array chunks {
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, false).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 128);
        REQUIRE(chunks[2].address == 256);
    }

    SECTION("Resources are interleaved")
    {
        std::array chunks {
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, true).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 256);
        REQUIRE(chunks[2].address == 512);
    }

    SECTION("Resources are interleaved")
    {
        std::array chunks {
            allocator.alloc(100, 64, false).value(),
            allocator.alloc(100, 64, true).value(),
            allocator.alloc(100, 64, false).value(),
        };
        REQUIRE(chunks[0].address == 0);
        REQUIRE(chunks[1].address == 256);
        REQUIRE(chunks[2].address == 512);
    }
}

TEST_CASE("First fit allocator, custom granularity, re-allocation", "[allocator]")
{
    lime::memory::FirstFit allocator { 1024, 256 };

    SECTION("Insertion of non-linear before linear, success")
    {
        auto const chunk1 { allocator.alloc(256, 64, true).value() };
        auto const chunk2 { allocator.alloc(100, 64, true).value() };
        allocator.free(chunk1.address);

        auto const chunk3 { allocator.alloc(100, 64, false).value() };
        REQUIRE(chunk3.address == 0);
        REQUIRE(allocator.canHold(668));
    }
    SECTION("Insertion of non-linear before linear, success")
    {
        auto const chunk1 { allocator.alloc(256, 64, true).value() };
        auto const chunk2 { allocator.alloc(100, 64, true).value() };
        allocator.free(chunk1.address);

        auto const chunk3 { allocator.alloc(500, 64, false).value() };
        REQUIRE(chunk3.address == 512);
        REQUIRE(allocator.canHold(24));
    }
    SECTION("Insertion of non-linear before linear, failure")
    {
        auto const chunk1 { allocator.alloc(400, 1, true).value() };
        auto const chunk2 { allocator.alloc(400, 1, true).value() };
        REQUIRE(allocator.canHold(224));
        allocator.free(chunk1.address);
        REQUIRE(allocator.canHold(400));

        auto const chunk3 { allocator.alloc(400, 1, false) };
        REQUIRE_FALSE(chunk3);
    }

    SECTION("Insertion of linear before non-linear, success")
    {
        auto const chunk1 { allocator.alloc(256, 64, false).value() };
        auto const chunk2 { allocator.alloc(100, 64, false).value() };
        allocator.free(chunk1.address);

        auto const chunk3 { allocator.alloc(100, 64, true).value() };
        REQUIRE(chunk3.address == 0);
        REQUIRE(allocator.canHold(668));
    }
    SECTION("Insertion of linear before non-linear, success")
    {
        auto const chunk1 { allocator.alloc(256, 64, false).value() };
        auto const chunk2 { allocator.alloc(100, 64, false).value() };
        allocator.free(chunk1.address);

        auto const chunk3 { allocator.alloc(500, 64, true).value() };
        REQUIRE(chunk3.address == 512);
        REQUIRE(allocator.canHold(24));
    }
    SECTION("Insertion of linear before non-linear, failure")
    {
        auto const chunk1 { allocator.alloc(400, 1, false).value() };
        auto const chunk2 { allocator.alloc(400, 1, false).value() };
        REQUIRE(allocator.canHold(224));
        allocator.free(chunk1.address);
        REQUIRE(allocator.canHold(400));

        auto const chunk3 { allocator.alloc(400, 1, true) };
        REQUIRE_FALSE(chunk3);
    }

    SECTION("Insertion of non-linear between linears, success")
    {
        auto const chunk1 { allocator.alloc(250, 64, true).value() };
        auto const chunk2 { allocator.alloc(250, 64, true).value() };
        auto const chunk3 { allocator.alloc(256, 64, true).value() };
        allocator.free(chunk2.address);

        auto const chunk4 { allocator.alloc(100, 64, false).value() };
        REQUIRE(chunk4.address == 256);
        REQUIRE(allocator.canHold(256));
        REQUIRE_FALSE(allocator.canHold(257));
    }
    SECTION("Insertion of non-linear between linears, success")
    {
        auto const chunk1 { allocator.alloc(120, 64, true).value() };
        auto const chunk2 { allocator.alloc(120, 64, true).value() };
        auto const chunk3 { allocator.alloc(120, 64, true).value() };
        allocator.free(chunk2.address);

        auto const chunk4 { allocator.alloc(100, 64, false).value() };
        REQUIRE(chunk4.address == 512);
        REQUIRE(allocator.canHold(412));
        REQUIRE_FALSE(allocator.canHold(413));
    }
    SECTION("Insertion of non-linear between linears, failure")
    {
        auto const chunk1 { allocator.alloc(300, 1, true).value() };
        auto const chunk2 { allocator.alloc(300, 1, true).value() };
        auto const chunk3 { allocator.alloc(300, 1, true).value() };
        REQUIRE(allocator.canHold(124));
        REQUIRE_FALSE(allocator.canHold(300));
        allocator.free(chunk2.address);
        REQUIRE(allocator.canHold(300));

        auto const chunk4 { allocator.alloc(100, 1, false) };
        REQUIRE_FALSE(chunk4);
    }

    SECTION("Insertion of linear between non-linears, success")
    {
        auto const chunk1 { allocator.alloc(250, 64, false).value() };
        auto const chunk2 { allocator.alloc(250, 64, false).value() };
        auto const chunk3 { allocator.alloc(256, 64, false).value() };
        allocator.free(chunk2.address);

        auto const chunk4 { allocator.alloc(100, 64, true).value() };
        REQUIRE(chunk4.address == 256);
        REQUIRE(allocator.canHold(256));
        REQUIRE_FALSE(allocator.canHold(257));
    }
    SECTION("Insertion of linear between non-linears, success")
    {
        auto const chunk1 { allocator.alloc(120, 64, false).value() };
        auto const chunk2 { allocator.alloc(120, 64, false).value() };
        auto const chunk3 { allocator.alloc(120, 64, false).value() };
        allocator.free(chunk2.address);

        auto const chunk4 { allocator.alloc(100, 64, true).value() };
        REQUIRE(chunk4.address == 512);
        REQUIRE(allocator.canHold(412));
        REQUIRE_FALSE(allocator.canHold(413));
    }
    SECTION("Insertion of linear between non-linears, failure")
    {
        auto const chunk1 { allocator.alloc(300, 1, false).value() };
        auto const chunk2 { allocator.alloc(300, 1, false).value() };
        auto const chunk3 { allocator.alloc(300, 1, false).value() };
        REQUIRE(allocator.canHold(124));
        REQUIRE_FALSE(allocator.canHold(300));
        allocator.free(chunk2.address);
        REQUIRE(allocator.canHold(300));

        auto const chunk4 { allocator.alloc(100, 1, true) };
        REQUIRE_FALSE(chunk4);
    }
}

TEST_CASE("Basic global memory allocator opreations")
{
    lime::LogSetCallback(nullptr);
    lime::LoadVulkan();
    lime::Capabilities cappa;
    auto const instance { lime::Instance(cappa) };
    auto const pds { lime::ListAllPhysicalDevicesInGroups(instance.get()) };
    auto const device { lime::Device(pds[0][0], cappa) };

    struct VCtx {
        vk::Instance i;
        vk::PhysicalDevice pd;
        vk::Device d;
    } const ctx { instance.get(), pds[0][0], device.get() };

    lime::MemoryManager memory { ctx.d, ctx.pd };

    lime::Buffer buffer;
    REQUIRE_FALSE(buffer.isValid());
    REQUIRE_FALSE(buffer.getMapping());
    REQUIRE(memory.empty());

    SECTION("buffer alloc and free")
    {
        buffer = memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eHostToDevice }, { .size = sizeof(i32), .usage = vk::BufferUsageFlagBits::eStorageBuffer });
        REQUIRE(buffer.isValid());
        REQUIRE(buffer.getMapping());
        REQUIRE_FALSE(memory.empty());

        buffer.reset();
        REQUIRE_FALSE(buffer.isValid());
        REQUIRE_FALSE(buffer.getMapping());
        REQUIRE_FALSE(memory.empty());

        memory.cleanUp();
        REQUIRE(memory.empty());
    }

    SECTION("host memory buffer data")
    {
        buffer = memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eHostToDevice }, { .size = sizeof(i32), .usage = vk::BufferUsageFlagBits::eStorageBuffer });
        lime::Transfer transfer { device.queues.transfer, memory };

        i32 const i = -10;
        i32 j = 0;

        transfer.ToDeviceSync(i, buffer);
        REQUIRE_FALSE(i == j);
        transfer.FromDevice(buffer, j);
        REQUIRE(i == j);

        buffer.reset();
        memory.cleanUp();
    }

    SECTION("device memory buffer data")
    {
        buffer = memory.alloc(
            {
                .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal,
            },
            {
                .size = sizeof(i32),
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
            });
        lime::Transfer transfer { device.queues.graphics, memory };

        // enforce staging even when the device memory can be mapped
        auto bDetail { static_cast<lime::Buffer::Detail>(buffer) };
        bDetail.mapping = nullptr;

        i32 const i = -10;
        i32 j = 0;

        transfer.ToDeviceSync(i, bDetail);
        REQUIRE_FALSE(i == j);
        transfer.FromDevice(bDetail, j);
        REQUIRE(i == j);

        buffer.reset();
        memory.cleanUp();
    }

    SECTION("device memory buffer big data")
    {
        auto constexpr count { 22'222'222u };

        buffer = memory.alloc(
            {
                .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal,
            },
            {
                .size = sizeof(u64) * count,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
            });
        lime::Transfer transfer { device.queues.graphics, memory };

        // enforce staging even when the device memory can be mapped
        auto bDetail { static_cast<lime::Buffer::Detail>(buffer) };
        bDetail.mapping = nullptr;

        u64 const first = 420u;
        u64 const last = 69u;

        {
            std::vector<u64> data(count, 0u);
            data[0] = first;
            data[count - 1] = last;
            transfer.ToDeviceSync(data, bDetail);
        }

        std::vector<u64> data;
        data.resize(count);

        transfer.FromDevice(bDetail, data);
        u64 a = data[0];
        u64 b = data[count - 1];

        REQUIRE(a == first);
        REQUIRE(b == last);

        buffer.reset();
        memory.cleanUp();
    }
}
