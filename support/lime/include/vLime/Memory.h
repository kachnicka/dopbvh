#pragma once

#include <format>
#include <list>
#include <memory>
#include <numeric>
#include <ranges>
#include <utility>
#include <vLime/DebugUtils.h>
#include <vLime/Flags.h>
#include <vLime/UtilImage.h>
#include <vLime/vLime.h>

namespace lime {
namespace memory {

inline vk::DeviceSize align(vk::DeviceSize address, vk::DeviceSize const alignment)
{
    if (alignment == 0)
        return address;

    if (auto const align = address % alignment; align != 0)
        address += (alignment - align);
    return address;
}

struct Chunk {
    vk::DeviceSize address = 0;
    vk::DeviceSize size = 0;
    bool isLinear = true;
};
inline bool operator==(Chunk const& lhs, Chunk const& rhs)
{
    return lhs.size == rhs.size && lhs.isLinear == rhs.isLinear;
}
inline bool operator!=(Chunk const& lhs, Chunk const& rhs)
{
    return !(lhs == rhs);
}

class Allocator {
public:
    virtual ~Allocator() = default;
    virtual std::optional<Chunk> alloc(vk::MemoryRequirements const& req, vk::DeviceSize additionalAlignment = 0, bool isLinear = true)
    {
        additionalAlignment = additionalAlignment ? std::lcm(req.alignment, additionalAlignment) : req.alignment;
        return alloc(req.size, additionalAlignment, isLinear);
    };
    virtual std::optional<Chunk> alloc(vk::DeviceSize size, vk::DeviceSize alignment = 1, bool isLinear = true) = 0;
    virtual void free(vk::DeviceSize chunkAddress) = 0;
    [[nodiscard]] virtual bool canHold(vk::DeviceSize allocSize) const = 0;
    virtual void coalesce() = 0;
    virtual void reset() = 0;

    struct Dump {
        std::vector<std::pair<Chunk, bool>> chunks;
        vk::DeviceSize size;
    };

    [[nodiscard]] virtual Dump dumpInternalState() const = 0;
};

class FirstFit final : public Allocator {
    vk::DeviceSize bufferImageGranularity;
    vk::DeviceSize size;

    std::list<Chunk> allChunks;
    std::list<std::list<Chunk>::iterator> freeChunks;
    bool shouldCoalesce = false;

public:
    explicit FirstFit(vk::DeviceSize size, vk::DeviceSize bufferImageGranularity = 1)
        : bufferImageGranularity(bufferImageGranularity)
        , size(size)
    {
        reset();
    }

    std::optional<Chunk> alloc(vk::DeviceSize const _size, vk::DeviceSize const alignment = 1, bool const isLinear = true) override
    {
        if (_size == 0)
            return {};

        for (auto it = freeChunks.begin(); it != freeChunks.end(); ++it) {
            auto const itCurr = *it;
            auto const itNext = std::next(itCurr);
            auto const haveNext = itNext != allChunks.end();
            auto const havePrev = itCurr != allChunks.begin();
            auto const itPrev { havePrev ? std::prev(itCurr) : itCurr };

            auto const linearityChangedPrev = havePrev && itPrev->isLinear != isLinear;
            auto const linearityChangedNext = haveNext && itNext->isLinear != isLinear;

            auto const chunkAlignment = linearityChangedPrev ? std::lcm(bufferImageGranularity, alignment) : alignment;
            auto const chunkAddressUpper = itCurr->address + itCurr->size;

            auto const chunkAddressLowerBound = align(itCurr->address, chunkAlignment);
            auto const chunkAddressUpperBound = linearityChangedNext ? align(chunkAddressUpper - bufferImageGranularity + 1, bufferImageGranularity) : chunkAddressUpper;
            auto const chunkAvailableSize = chunkAddressUpperBound - chunkAddressLowerBound;

            // might happen due to alignment requirements
            if (chunkAddressLowerBound >= chunkAddressUpperBound)
                continue;

            if (_size <= chunkAvailableSize) {
                Chunk chunkAllocated { chunkAddressLowerBound, _size, isLinear };
                it = freeChunks.erase(it);

                if (auto const fragmentationBefore = chunkAddressLowerBound - itCurr->address)
                    itPrev->size += fragmentationBefore;

                if (auto const fragmentationAfter = chunkAvailableSize - _size) {
                    if (fragmentationAfter < 16)
                        chunkAllocated.size += fragmentationAfter;
                    else {
                        Chunk chunkFree { chunkAllocated.address + _size, fragmentationAfter, isLinear };
                        auto const itFree = allChunks.emplace(itNext, chunkFree);
                        freeChunks.emplace(it, itFree);
                    }
                }
                *itCurr = chunkAllocated;
                return std::make_optional(chunkAllocated);
            }
        }
        return {};
    }

    // bool validateChunks() const
    // {
    //     size_t total = 0;
    //     u32 id = 0;
    //     for (auto const& chunk : allChunks) {
    //         if (total != chunk.address)
    //             return false;
    //         if (chunk.size > size)
    //             return false;
    //         total += chunk.size;
    //         id++;
    //     }
    //     if (total != size)
    //         return false;
    //
    //     total = 0;
    //     for (auto const& chunk : freeChunks) {
    //         if (chunk->address < total)
    //             return false;
    //         if (chunk->size > size)
    //             return false;
    //         total = chunk->address + chunk->size;
    //     }
    //     if (total > size)
    //         return false;
    //     return true;
    // }

    void free(vk::DeviceSize chunkAddress) override
    {
        auto chunkIt = std::find_if(allChunks.begin(), allChunks.end(),
            [=](auto const& listAddress) { return listAddress.address == chunkAddress; });

        // find position in free list, keep chunks sorted by address
        auto emplaceIt = freeChunks.end();
        for (auto it = freeChunks.begin(); it != freeChunks.end(); ++it)
            if ((*it)->address > chunkAddress) {
                emplaceIt = it;
                break;
            }
        freeChunks.emplace(emplaceIt, chunkIt);
        shouldCoalesce = true;
    }

    [[nodiscard]] bool canHold(vk::DeviceSize allocSize) const override
    {
        return std::any_of(freeChunks.begin(), freeChunks.end(), [allocSize](auto const& chunk) { return chunk->size >= allocSize; });
    }

    void coalesce() override
    {
        if (!shouldCoalesce)
            return;

        for (auto it = freeChunks.begin(); it != freeChunks.end(); ++it) {
            for (auto nextIt = std::next(it); nextIt != freeChunks.end(); nextIt = std::next(it)) {
                auto& chunk = **it;
                if (auto const& nextChunk = **nextIt; nextChunk.address == chunk.address + chunk.size) {
                    chunk.size += nextChunk.size;
                    allChunks.erase(*nextIt);
                    freeChunks.erase(nextIt);
                } else
                    break;
            }
        }

        shouldCoalesce = false;
    }

    void reset() override
    {
        allChunks.clear();
        freeChunks.clear();
        allChunks.emplace_front(0, size, true);
        freeChunks.push_front(allChunks.begin());
    }

    void testPrint() const
    {
        log::debug("All chunks");
        for (auto const& chunk : allChunks)
            log::debug(std::format("  chunk: address: {}, size: {}, isLinear: {}\n", chunk.address, chunk.size, chunk.isLinear));
        log::debug("Free chunks");
        for (auto const& chunk : freeChunks)
            log::debug(std::format("  chunk: address: {}, size: {}, isLinear: {}\n", chunk->address, chunk->size, chunk->isLinear));
        log::debug("\n");
    }

    [[nodiscard]] Dump dumpInternalState() const override
    {
        Dump dump;
        dump.chunks.reserve(allChunks.size());
        auto it = freeChunks.begin();

        for (auto const& chunk : allChunks) {
            auto isOccupied = true;
            if (it != freeChunks.end() && **it == chunk) {
                isOccupied = false;
                ++it;
            }
            dump.chunks.emplace_back(chunk, isOccupied);
        }
        dump.size = size;
        return dump;
    }
};

struct Binding {
    vk::DeviceMemory memory;
    vk::DeviceSize offset { 0 };
    vk::DeviceSize size { 0 };
    void* mapping { nullptr };

    Allocator* allocator { nullptr };

    void reset()
    {
        // assert(allocator);
        if (allocator)
            allocator->free(offset);
        *this = {};
    }

    [[nodiscard]] bool isValid() const { return memory != vk::DeviceMemory {}; }
};

}

enum class DeviceMemoryUsage {
    // GPU exclusive resources (e.g. render pass attachments)
    // resources occasionally copied form cpu, read on gpu (e.g. VBO, textures)
    eDeviceOptimal,
    // resources frequently copied from cpu, read on gpu (e.g. transformation matrices)
    eHostToDevice,
    // resources read back on cpu, after gpu access (e.g. renderImage data)
    eDeviceToHost,
    // staging or resizable bar memory
    eHostToDeviceOptimal,
};
enum class AllocationFlagBits : u32 {
    // force to use dedicated memory allocation exclusive to resource
    eDedicated = 1 << 0,
    // allow to generate external memory handle
    eExported = 1 << 1,
};

using AllocationFlags = Flags<AllocationFlagBits>;
GENERATE_FLAG_OPERATOR_OVERLOADS(AllocationFlag);

struct AllocRequirements {
    DeviceMemoryUsage memoryUsage { DeviceMemoryUsage::eDeviceOptimal };
    AllocationFlags allocFlags {};
    vk::DeviceSize additionalAlignment { 0 };
};

static constexpr unsigned MAX_FALLBACK = 4;
static constexpr vk::DeviceSize MB = 1024 * 1024;

class Buffer {
    friend class MemoryManager;

    vk::UniqueBuffer buffer;
    vk::DeviceSize size = 0;
    memory::Binding binding;

public:
    Buffer() = default;
    Buffer(vk::Device d, vk::BufferCreateInfo const& cInfo)
        : buffer(check(d.createBufferUnique(cInfo)))
        , size(cInfo.size)
    {
    }
    ~Buffer()
    {
        reset();
    }

    Buffer(Buffer&& rhs) noexcept
        : buffer(std::move(rhs.buffer))
        , size(rhs.size)
        , binding(rhs.binding)
    {
        rhs.size = 0;
        rhs.binding = {};
    }

    Buffer& operator=(Buffer&& rhs) noexcept
    {
        if (this != &rhs) {
            buffer = std::move(rhs.buffer);
            size = rhs.size;
            binding = rhs.binding;

            rhs.size = 0;
            rhs.binding = {};
        }
        return *this;
    }

    Buffer(Buffer const& rhs) = delete;
    Buffer& operator=(Buffer const& rhs) = delete;

    struct Detail {
        vk::Buffer resource;
        vk::DeviceSize offset { 0 };
        vk::DeviceSize size { 0 };
        void* mapping { nullptr };

        void reset()
        {
            resource = vk::Buffer {};
            offset = 0;
            size = 0;
            mapping = nullptr;
        }

        [[nodiscard]] void* getMapping() const
        {
            return mapping;
        }

        [[nodiscard]] vk::DeviceSize getSizeInBytes() const
        {
            return size;
        }

        [[nodiscard]] vk::DeviceAddress getDeviceAddress(vk::Device d) const
        {
            return d.getBufferAddress({ .buffer = resource }) + offset;
        }

        [[nodiscard]] std::pair<vk::DeviceAddress, vk::DeviceSize> getTransferableRegion(vk::DeviceSize regionAddress, vk::DeviceSize limitMaxSize) const
        {
            static_cast<void>(regionAddress);
            return { 0, limitMaxSize };
        }

        void CopyBufferToMe(vk::CommandBuffer commandBuffer, Detail const& src, vk::DeviceAddress dstOffsetShift) const
        {
            vk::BufferCopy copyRegion;
            copyRegion.srcOffset = src.offset;
            copyRegion.dstOffset = offset + dstOffsetShift;
            copyRegion.size = src.size;

            commandBuffer.copyBuffer(src.resource, resource, 1, &copyRegion);
        }

        void CopyMeToBuffer(vk::CommandBuffer commandBuffer, Detail const& dst, vk::DeviceAddress srcOffsetShift) const
        {
            vk::BufferCopy copyRegion;
            copyRegion.srcOffset = offset + srcOffsetShift;
            copyRegion.dstOffset = dst.offset;
            copyRegion.size = dst.size;

            commandBuffer.copyBuffer(resource, dst.resource, 1, &copyRegion);
        }
    };

    void reset()
    {
        buffer.reset();
        size = 0;
        binding.reset();
    }

    [[nodiscard]] bool isValid() const
    {
        return buffer.get() != vk::Buffer {};
    }

    [[nodiscard]] void* getMapping() const
    {
        return binding.mapping;
    }

    [[nodiscard]] vk::DeviceSize getBackingMemorySize() const
    {
        return binding.size;
    }

    [[nodiscard]] auto getMemoryRequirements(vk::Device d) const
    {
        return d.getBufferMemoryRequirements(buffer.get());
    }

    [[nodiscard]] vk::DeviceAddress getDeviceAddress(vk::Device d) const
    {
        return d.getBufferAddress({ .buffer = buffer.get() });
    }

    [[nodiscard]] vk::Buffer get() const
    {
        return buffer.get();
    }

    operator Detail() const
    {
        return { buffer.get(), 0, size, binding.mapping };
    }

    [[nodiscard]] vk::DeviceSize getSizeInBytes() const
    {
        return size;
    }

    [[nodiscard]] bool fits(vk::DeviceSize allocSize) const
    {
        return allocSize <= size;
    }

    [[nodiscard]] std::pair<vk::DeviceAddress, vk::DeviceSize> getTransferableRegion(vk::DeviceSize regionAddress, vk::DeviceSize limitMaxSize) const
    {
        static_cast<void>(regionAddress);
        return { 0, limitMaxSize };
    }

    void CopyBufferToMe(vk::CommandBuffer commandBuffer, Detail const& src, vk::DeviceAddress dstOffset) const
    {
        if (src.size == 0)
            return;

        vk::BufferCopy copyRegion;
        copyRegion.srcOffset = src.offset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = src.size;

        commandBuffer.copyBuffer(src.resource, buffer.get(), 1, &copyRegion);
    }

    void CopyMeToBuffer(vk::CommandBuffer commandBuffer, Detail const& dst, vk::DeviceAddress srcOffset) const
    {
        vk::BufferCopy copyRegion;
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dst.offset;
        copyRegion.size = dst.size;

        commandBuffer.copyBuffer(buffer.get(), dst.resource, 1, &copyRegion);
    }
};

class Image {
    friend class MemoryManager;

    vk::Device d;
    vk::ImageCreateInfo cInfo;
    vk::UniqueImage image;
    vk::UniqueImageView imageView;

    memory::Binding binding;

public:
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout nextLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    struct Detail {
        vk::Image image;
        vk::ImageView imageView;
        vk::Extent3D extent;
        vk::Format format { vk::Format::eUndefined };
    };

    Image() = default;
    Image(vk::Device d, vk::ImageCreateInfo const& cInfo)
        : d(d)
        , cInfo(cInfo)
        , image(check(d.createImageUnique(cInfo, nullptr)))
    {
    }
    ~Image()
    {
        reset();
    }
    Image(Image&& rhs) noexcept
        : d(rhs.d)
        , cInfo(rhs.cInfo)
        , image(std::move(rhs.image))
        , imageView(std::move(rhs.imageView))
        , binding(rhs.binding)
        , layout(rhs.layout)
        , nextLayout(rhs.nextLayout)
    {
        rhs.d = vk::Device {};
        rhs.cInfo = vk::ImageCreateInfo {};
        rhs.binding = {};
        rhs.layout = vk::ImageLayout::eUndefined;
        rhs.nextLayout = vk::ImageLayout::eUndefined;
    }
    Image& operator=(Image&& rhs) noexcept
    {
        if (this != &rhs) {
            d = rhs.d;
            cInfo = rhs.cInfo;
            image = std::move(rhs.image);
            imageView = std::move(rhs.imageView);
            binding = rhs.binding;
            layout = rhs.layout;
            nextLayout = rhs.nextLayout;

            rhs.d = vk::Device {};
            rhs.cInfo = vk::ImageCreateInfo {};
            rhs.binding = {};
            rhs.layout = vk::ImageLayout::eUndefined;
            rhs.nextLayout = vk::ImageLayout::eUndefined;
        }
        return *this;
    }
    Image(Image const& rhs) = delete;
    Image& operator=(Image const& rhs) = delete;

    void reset()
    {
        image.reset();
        imageView.reset();
        binding.reset();

        cInfo = vk::ImageCreateInfo {};
        layout = vk::ImageLayout::eUndefined;
        nextLayout = vk::ImageLayout::eUndefined;
    }

    [[nodiscard]] bool isValid() const
    {
        return (image.get() != vk::Image {});
    }

    [[nodiscard]] void* getMapping() const
    {
        return binding.mapping;
    }

    [[nodiscard]] vk::DeviceSize getBackingMemorySize() const
    {
        return binding.size;
    }

    [[nodiscard]] auto getMemoryRequirements() const
    {
        return d.getImageMemoryRequirements(image.get());
    }

    [[nodiscard]] vk::Image get() const
    {
        return image.get();
    }

    [[nodiscard]] vk::DeviceSize getSizeInBytes() const
    {
        return img_util::Size(cInfo.extent, cInfo.format);
    }

    [[nodiscard]] Detail getDetail() const
    {
        return { image.get(), imageView.get(), cInfo.extent, cInfo.format };
    }

    [[nodiscard]] vk::ImageView getView() const
    {
        return imageView.get();
    }

    [[nodiscard]] std::pair<vk::DeviceAddress, vk::DeviceSize> getTransferableRegion(vk::DeviceSize regionAddress, vk::DeviceSize limitMaxSize) const
    {
        auto const texelSize = img_util::BytesPerPixel(cInfo.format);
        auto const newOffset = memory::align(regionAddress, texelSize);
        auto const offsetShift = newOffset - regionAddress;

        // integral division abuse
        auto const rowsToFit = (limitMaxSize / texelSize) / cInfo.extent.width;
        auto const transferableSize = (rowsToFit >= cInfo.extent.height ? limitMaxSize : rowsToFit * cInfo.extent.width * texelSize);

        return { offsetShift, transferableSize };
    }

    void CopyBufferToMe(vk::CommandBuffer commandBuffer, Buffer::Detail const& src, vk::DeviceAddress dstOffset)
    {
        // noShader layout only
        if (src.getSizeInBytes() == 0) {
            LayoutTransition(commandBuffer, vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eFragmentShader);
            return;
        }
        bool regionBegins { false };
        bool regionEnds { false };

        // TODO: should also correctly copy mipmaps if they exist
        // tightly packed according to imageExtent
        vk::BufferImageCopy copyRegion {
            .bufferOffset = src.offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        // correct addressing between buffer and image content depends on current segment
        auto const texelSize = img_util::BytesPerPixel(cInfo.format);
        auto const Y = static_cast<i32>((dstOffset / texelSize) / cInfo.extent.width);
        copyRegion.imageOffset = vk::Offset3D(0, Y, 0);
        if (Y == 0)
            regionBegins = true;

        auto extY = static_cast<u32>((src.size / texelSize) / cInfo.extent.width);
        if ((extY + Y) >= cInfo.extent.height) {
            extY = cInfo.extent.height - Y;
            regionEnds = true;
        }
        copyRegion.imageExtent = vk::Extent3D(cInfo.extent.width, std::max(extY, 1u), 1);

        if (regionBegins) {
            auto const expectedLayout { nextLayout };
            LayoutTransition(commandBuffer, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::ImageLayout::eTransferDstOptimal);
            nextLayout = expectedLayout;
        }
        commandBuffer.copyBufferToImage(src.resource, image.get(), vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

        if (regionEnds)
            LayoutTransition(commandBuffer, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);
    }

    void CopyMeToBuffer(vk::CommandBuffer commandBuffer, Buffer::Detail const& dst, vk::DeviceAddress srcOffset) const
    {
        // TODO: should also correctly copy mipmaps if they exist
        // tightly packed according to imageExtent
        vk::BufferImageCopy copyRegion {
            .bufferOffset = dst.offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        // correct addressing between buffer and image content depends on current segment
        auto const texelSize = img_util::BytesPerPixel(cInfo.format);
        auto const Y = static_cast<i32>((srcOffset / texelSize) / cInfo.extent.width);
        copyRegion.imageOffset = vk::Offset3D(0, Y, 0);

        auto extY = static_cast<u32>((dst.size / texelSize) / cInfo.extent.width);
        if ((extY + Y) >= cInfo.extent.height)
            extY = cInfo.extent.height - Y;
        extY = std::max(extY, 1u);

        copyRegion.imageExtent = vk::Extent3D(cInfo.extent.width, extY, 1);
        commandBuffer.copyImageToBuffer(image.get(), vk::ImageLayout::eTransferSrcOptimal, dst.resource, 1, &copyRegion);
    }

    void CopyMeToImage(vk::CommandBuffer commandBuffer, Image const& dst) const
    {
        // TODO: should also correctly copy mipmaps if they exist
        vk::ImageSubresourceLayers imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        vk::ImageCopy imageCopy {
            .srcSubresource = imageSubresource,
            .dstSubresource = imageSubresource,
            .extent = cInfo.extent,
        };

        commandBuffer.copyImage(image.get(), vk::ImageLayout::eTransferSrcOptimal, dst.image.get(), vk::ImageLayout::eTransferDstOptimal, 1, &imageCopy);
    }

    vk::ImageMemoryBarrier GetMemoryBarrier(vk::ImageLayout finalLayout = vk::ImageLayout::eUndefined)
    {
        if (finalLayout != vk::ImageLayout::eUndefined)
            nextLayout = finalLayout;

        auto const& [srcMask, dstMask] { img_util::DefaultAccessFlags(layout, nextLayout) };
        return vk::ImageMemoryBarrier {
            .srcAccessMask = srcMask,
            .dstAccessMask = dstMask,
            .oldLayout = layout,
            .newLayout = nextLayout,
            .image = image.get(),
            .subresourceRange = {
                .aspectMask = img_util::Aspect(cInfo.format),
                .baseMipLevel = 0,
                .levelCount = cInfo.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = cInfo.arrayLayers,
            },
        };
    }

    void LayoutTransition(vk::CommandBuffer commandBuffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::ImageLayout finalLayout = vk::ImageLayout::eUndefined)
    {
        std::vector memoryBarrier { GetMemoryBarrier(finalLayout) };
        if (layout == nextLayout)
            return;

        commandBuffer.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, memoryBarrier);
        layout = nextLayout;
    }

    void CreateImageView()
    {
        vk::ImageViewCreateInfo cInfoView {
            .image = image.get(),
            .viewType = vk::ImageViewType::e2D,
            .format = cInfo.format,
            .subresourceRange = {
                .aspectMask = img_util::Aspect(cInfo.format),
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        imageView = check(d.createImageViewUnique(cInfoView));
    }
};

class MemoryDump;

class MemoryManager {
    class DeviceMemory;

public:
    vk::Device d;
    vk::PhysicalDevice pd;

private:
    vk::PhysicalDeviceMemoryProperties properties;
    vk::DeviceSize bufferImageGranularity = 0;

    std::vector<std::vector<std::unique_ptr<DeviceMemory>>> deviceMemoryPerType;

    class MemoryTypeIdCache {
    public:
        static constexpr u32 INVALID_MEMORY_ID { std::numeric_limits<u32>::max() };

    private:
        // size of enum class DeviceMemoryUsage
        static constexpr u32 MEMORY_USAGE_COUNT { 4 };
        static constexpr u32 FALLBACK_LEVEL_COUNT { 3 };
        using MemoryTypeId = u32;
        using MemoryTypeIds = std::array<MemoryTypeId, FALLBACK_LEVEL_COUNT>;

        bool resizableBar { false };
        std::array<MemoryTypeIds, MEMORY_USAGE_COUNT> memTypes {};

    public:
        explicit MemoryTypeIdCache(MemoryManager const& memory)
            : resizableBar(checkResizableBar(memory))
        {
            using flag = vk::MemoryPropertyFlagBits;
            std::array<MemoryTypeId, 3> memId {
                getMemoryTypeForFlags(memory.properties, flag::eDeviceLocal),
                getMemoryTypeForFlags(memory.properties, flag::eHostVisible | flag::eHostCoherent),
                getMemoryTypeForFlags(memory.properties, flag::eDeviceLocal | flag::eHostVisible | flag::eHostCoherent),
                // TODO: cache flushing, cache invalidation, memory barriers
                // getMemoryTypeForFlags(memory.properties, flag::eHostVisible | flag::eHostCached),
                // getMemoryTypeForFlags(memory.properties, flag::eHostVisible),
            };
            using DMU = DeviceMemoryUsage;
            memTypes[std::to_underlying(DMU::eHostToDeviceOptimal)] = { memId[2], memId[1] };

            if (resizableBar) {
                memTypes[std::to_underlying(DMU::eDeviceOptimal)] = { memId[2], memId[0], memId[1] };
                memTypes[std::to_underlying(DMU::eHostToDevice)] = { memId[2], memId[1] };
                memTypes[std::to_underlying(DMU::eDeviceToHost)] = { memId[2], memId[1] };
            } else {
                memTypes[std::to_underlying(DMU::eDeviceOptimal)] = { memId[0], memId[1] };
                memTypes[std::to_underlying(DMU::eHostToDevice)] = { memId[1] };
                memTypes[std::to_underlying(DMU::eDeviceToHost)] = { memId[1] };
            }
        }

        MemoryTypeIds GetMemoryTypes(DeviceMemoryUsage usage, u32 memoryTypeFilter)
        {
            MemoryTypeIds result { INVALID_MEMORY_ID, INVALID_MEMORY_ID, INVALID_MEMORY_ID };

            u32 i { 0 };
            for (auto const& memoryTypeId : memTypes[std::to_underlying(usage)])
                if (isMemoryTypeValidForFilter(memoryTypeId, memoryTypeFilter))
                    result[i++] = memoryTypeId;

            return result;
        }

        [[nodiscard]] bool haveResizableBar() const
        {
            return resizableBar;
        }

    private:
        static bool checkResizableBar(MemoryManager const& memory)
        {
            auto const dummy { memory.dummyBufferRequirements() };
            auto const memTypeDeviceOptimal { findMemoryTypeIndex(dummy.memoryTypeBits, memory.properties, vk::MemoryPropertyFlagBits::eDeviceLocal) };
            auto const memTypeHostToDeviceOptimal { findMemoryTypeIndex(dummy.memoryTypeBits, memory.properties, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent) };
            return memory.properties.memoryTypes[memTypeDeviceOptimal.value()].heapIndex == memory.properties.memoryTypes[memTypeHostToDeviceOptimal.value()].heapIndex;
        }

        [[nodiscard, maybe_unused]] static std::optional<u32>
        findMemoryTypeIndex(u32 memoryTypeFilter, vk::PhysicalDeviceMemoryProperties const& memoryProperties, vk::MemoryPropertyFlags propertyFlags)
        {
            for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++)
                if ((memoryTypeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
                    return i;
            return {};
        }

        [[nodiscard]] static u32 getMemoryTypeForFlags(vk::PhysicalDeviceMemoryProperties const& memoryProperties, vk::MemoryPropertyFlags propertyFlags)
        {
            for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++)
                if ((memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
                    return i;
            return INVALID_MEMORY_ID;
        }

        [[nodiscard]] static bool isMemoryTypeValidForFilter(u32 memoryTypeId, u32 memoryTypeFilter)
        {
            return memoryTypeFilter & (1 << memoryTypeId);
        }
    } memoryTypeIdCache;

public:
    // subset of relevant device features requiring runtime checks
    struct ActiveDeviceFeatures {
        bool bufferDeviceAddress { false };
    } features;

    explicit MemoryManager(vk::Device d, vk::PhysicalDevice pd)
        : d(d)
        , pd(pd)
        , properties(pd.getMemoryProperties())
        , bufferImageGranularity(pd.getProperties().limits.bufferImageGranularity)
        , memoryTypeIdCache(*this)
    {
        deviceMemoryPerType.reserve(properties.memoryTypeCount);
    }

    [[nodiscard]] Buffer alloc(AllocRequirements const& allocRequirements, vk::BufferCreateInfo const& cInfo, char const* debugName = nullptr)
    {
        Buffer buffer { d, cInfo };
        if (!buffer.isValid())
            return {};

        buffer.binding = getBackingMemory(allocRequirements, buffer.getMemoryRequirements(d));
        if (!buffer.binding.isValid())
            return {};

        check(d.bindBufferMemory(buffer.get(), buffer.binding.memory, buffer.binding.offset));
        if (debugName)
            debug::SetObjectName(buffer.get(), debugName, d);

        return buffer;
    }

    [[nodiscard]] Image alloc(AllocRequirements const& allocRequirements, vk::ImageCreateInfo const& cInfo, char const* debugName = nullptr)
    {
        Image image { d, cInfo };
        if (!image.isValid())
            return {};

        image.binding = getBackingMemory(allocRequirements, image.getMemoryRequirements());
        if (!image.binding.isValid())
            return {};

        check(d.bindImageMemory(image.get(), image.binding.memory, image.binding.offset));
        if (debugName)
            debug::SetObjectName(image.get(), debugName, d);

        return image;
    }

    [[nodiscard]] bool empty() const
    {
        for (auto const& heap : deviceMemoryPerType)
            if (!heap.empty())
                return false;
        return true;
    }

    void cleanUp()
    {
        for (auto& heaps : deviceMemoryPerType) {
            std::vector<u32> emptyAllocations;
            for (u32 i { 0 }; i < heaps.size(); i++) {
                heaps[i]->CleanUp();
                if (heaps[i]->empty())
                    emptyAllocations.push_back(i);
            }
            for (auto const& i : std::ranges::views::reverse(emptyAllocations)) {
                heaps.erase(heaps.begin() + i);
                log::debug("MemoryManager::CleanUp: erasing empty allocation");
            }
        }
    }

    [[nodiscard]] vk::MemoryRequirements dummyBufferRequirements(vk::BufferUsageFlags flags = { vk::BufferUsageFlagBits::eStorageBuffer }) const
    {
        auto const dummy { check(d.createBufferUnique({ .flags = {}, .size = 1, .usage = flags })) };
        return d.getBufferMemoryRequirements(dummy.get());
    }

private:
    [[nodiscard]] bool checkHostVisibility(u32 const memTypeId) const
    {
        return (properties.memoryTypes[memTypeId].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) == vk::MemoryPropertyFlagBits::eHostVisible;
    }

    memory::Binding getBackingMemory(AllocRequirements const& allocRequirements, vk::MemoryRequirements const& memoryRequirements)
    {
        // TODO: recover from out of device memory?
        auto const memoryTypeId { memoryTypeIdCache.GetMemoryTypes(allocRequirements.memoryUsage, memoryRequirements.memoryTypeBits)[0] };

        if (deviceMemoryPerType.size() <= memoryTypeId)
            deviceMemoryPerType.resize(memoryTypeId + 1);

        auto const dedicated { allocRequirements.allocFlags.checkFlags(AllocationFlagBits::eDedicated) };
        // bind resource to one of existing allocations
        if (!dedicated)
            for (auto const& deviceMemory : deviceMemoryPerType[memoryTypeId])
                if (auto binding { deviceMemory->alloc(memoryRequirements, allocRequirements.additionalAlignment) }; binding.isValid())
                    return binding;

        vk::MemoryAllocateFlagsInfo allocFlags;
        if (features.bufferDeviceAddress)
            allocFlags.flags |= vk::MemoryAllocateFlagBits::eDeviceAddress;

        vk::MemoryAllocateInfo allocInfo {
            .pNext = &allocFlags,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = memoryTypeId,
        };
        // alloc more memory if feasible
        if (!dedicated && (memoryTypeIdCache.haveResizableBar() || allocRequirements.memoryUsage != DeviceMemoryUsage::eHostToDeviceOptimal))
            allocInfo.allocationSize = std::max(memoryRequirements.size, 256 * MB);

        auto& deviceMemory { deviceMemoryPerType[memoryTypeId].emplace_back(std::make_unique<DeviceMemory>(*this, allocInfo, bufferImageGranularity)) };
        if (auto binding { deviceMemory->alloc(memoryRequirements, allocRequirements.additionalAlignment) }; binding.isValid())
            return binding;

        log::error(std::format("Failed to allocate memory of size {} for memory type {}.", memoryRequirements.size, memoryTypeId));
        return {};
    }

    class DeviceMemory {
        friend class MemoryDump;

        vk::Device d;
        vk::UniqueDeviceMemory memory;
        vk::DeviceSize size;
        std::unique_ptr<memory::Allocator> allocator;

        void* mapping = nullptr;

    public:
        DeviceMemory(MemoryManager const& memMan, vk::MemoryAllocateInfo const allocInfo, vk::DeviceSize bufferImageGranularity)
            : d(memMan.d)
            , size(allocInfo.allocationSize)
            , allocator(std::make_unique<memory::FirstFit>(size, bufferImageGranularity))
        {
            memory = check(d.allocateMemoryUnique(allocInfo));
            if (memMan.checkHostVisibility(allocInfo.memoryTypeIndex))
                mapping = check(d.mapMemory(*memory, 0, size));
        }

        memory::Binding alloc(vk::MemoryRequirements const& req, vk::DeviceAddress additionalAlignment = 0)
        {
            memory::Binding binding;
            if (auto const chunk { allocator->alloc(req, additionalAlignment) }) {
                binding.memory = memory.get();
                binding.offset = chunk->address;
                binding.size = chunk->size;
                binding.mapping = mapping ? static_cast<char*>(mapping) + chunk->address : nullptr;
                binding.allocator = allocator.get();
            }
            return binding;
        }

        void CleanUp() const
        {
            allocator->coalesce();
        }

        [[nodiscard]] bool empty() const
        {
            return allocator->canHold(size);
        }
    };
};

class LinearAllocator {
private:
    Buffer::Detail buffer;
    vk::DeviceSize freeAddress { 0 };

public:
    LinearAllocator() = default;
    explicit LinearAllocator(Buffer::Detail const& detail)
        : buffer(detail)
        , freeAddress(detail.offset)
    {
    }

    void reset()
    {
        freeAddress = buffer.offset;
    }

    Buffer::Detail alloc(vk::DeviceSize allocSize, vk::DeviceSize alignment = 0)
    {
        auto const allocAddress { memory::align(freeAddress, alignment) };

        if (allocAddress + allocSize > buffer.size)
            return {};

        freeAddress = allocAddress + allocSize;
        void* memoryMapping = buffer.getMapping() ? static_cast<char*>(buffer.getMapping()) + allocAddress : nullptr;
        return { buffer.resource, allocAddress, allocSize, memoryMapping };
    }
};

}
