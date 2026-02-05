#pragma once

#include <pch.h>

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <optional>
#include <algorithm>
#include <bitset>

namespace vk_state
{
static constexpr uint32_t kMaxDescriptorSets =
    32; // Increased from 8 to support more pipelines (Vulkan spec minimum is 4, typical is 32)
static constexpr uint32_t kMaxViewports = 16;
static constexpr uint32_t kMaxScissors = 16;
static constexpr uint32_t kMaxVertexBuffers = 32;      // Standard limit is often 32
static constexpr uint32_t kMaxPushConstantBytes = 256; // 128 is min-spec, 256 covers most

enum class BindPointIndex : uint32_t
{
    Graphics = 0,
    Compute = 1,
    Count
};

inline std::optional<BindPointIndex> ToIndex(VkPipelineBindPoint bp)
{
    switch (bp)
    {
    case VK_PIPELINE_BIND_POINT_GRAPHICS:
        return BindPointIndex::Graphics;
    case VK_PIPELINE_BIND_POINT_COMPUTE:
        return BindPointIndex::Compute;
    case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        // Ray tracing bind point - not tracked by this system
        LOG_DEBUG("Ray tracing bind point not tracked by state tracker");
        return std::nullopt;
    default:
        // Unknown bind point - don't track to avoid corrupting other state
        LOG_WARN("Unknown pipeline bind point {} - ignoring to avoid state corruption", (uint32_t) bp);
        return std::nullopt;
    }
}

// Function table for replay
struct VulkanCmdFns
{
    PFN_vkCmdBindPipeline CmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets = nullptr;
    PFN_vkCmdPushConstants CmdPushConstants = nullptr;
    PFN_vkCmdSetViewport CmdSetViewport = nullptr;
    PFN_vkCmdSetScissor CmdSetScissor = nullptr;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer = nullptr;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;

    // Extended dynamic state
    PFN_vkCmdSetCullMode CmdSetCullMode = nullptr;
    PFN_vkCmdSetFrontFace CmdSetFrontFace = nullptr;
    PFN_vkCmdSetPrimitiveTopology CmdSetPrimitiveTopology = nullptr;
    PFN_vkCmdSetDepthTestEnable CmdSetDepthTestEnable = nullptr;
    PFN_vkCmdSetDepthWriteEnable CmdSetDepthWriteEnable = nullptr;
    PFN_vkCmdSetDepthCompareOp CmdSetDepthCompareOp = nullptr;
    PFN_vkCmdSetDepthBoundsTestEnable CmdSetDepthBoundsTestEnable = nullptr;
    PFN_vkCmdSetStencilTestEnable CmdSetStencilTestEnable = nullptr;
    PFN_vkCmdSetStencilOp CmdSetStencilOp = nullptr;
};

struct DescriptorBinding
{
    bool Bound = false;
    VkDescriptorSet Set = VK_NULL_HANDLE;
    VkPipelineLayout BoundWithLayout = VK_NULL_HANDLE; // Layout used when this set was bound
    uint32_t BindCallIndex = 0;                        // Index into DescriptorBindCalls that established this set
};

// NEW: Verbatim recording of vkCmdBindDescriptorSets calls
struct DescriptorBindCall
{
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    uint32_t FirstSet = 0;
    uint32_t DescriptorSetCount = 0;
    std::vector<VkDescriptorSet> Sets;    // Store sets dynamically to handle any valid count
    std::vector<uint32_t> DynamicOffsets; // All dynamic offsets for this call
};

struct PushConstantEntry
{
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkShaderStageFlags Stages = 0;
    uint32_t Offset = 0;
    uint32_t Size = 0;
    std::array<uint8_t, kMaxPushConstantBytes> Data {};
};

struct BindPointState
{
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout CurrentPipelineLayout = VK_NULL_HANDLE; // Last layout seen in BindDescriptorSets

    std::array<DescriptorBinding, kMaxDescriptorSets> Sets {};

    // NEW: Timeline of descriptor bind calls
    std::vector<DescriptorBindCall> DescriptorBindCalls;
};

struct DynamicState
{
    std::bitset<kMaxViewports> ViewportValidMask;
    std::array<VkViewport, kMaxViewports> Viewports {};

    std::bitset<kMaxScissors> ScissorValidMask;
    std::array<VkRect2D, kMaxScissors> Scissors {};

    bool CullModeSet = false;
    VkCullModeFlags CullMode = VK_CULL_MODE_NONE;

    bool FrontFaceSet = false;
    VkFrontFace FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    bool PrimitiveTopologySet = false;
    VkPrimitiveTopology PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    bool DepthTestEnableSet = false;
    VkBool32 DepthTestEnable = VK_FALSE;

    bool DepthWriteEnableSet = false;
    VkBool32 DepthWriteEnable = VK_TRUE;

    bool DepthCompareOpSet = false;
    VkCompareOp DepthCompareOp = VK_COMPARE_OP_LESS;

    bool DepthBoundsTestEnableSet = false;
    VkBool32 DepthBoundsTestEnable = VK_FALSE;

    bool StencilTestEnableSet = false;
    VkBool32 StencilTestEnable = VK_FALSE;

    bool StencilOpSet = false;
    VkStencilFaceFlags StencilOpFaceMask = 0;
    VkStencilOp StencilFailOp = VK_STENCIL_OP_KEEP;
    VkStencilOp StencilPassOp = VK_STENCIL_OP_KEEP;
    VkStencilOp StencilDepthFailOp = VK_STENCIL_OP_KEEP;
    VkCompareOp StencilCompareOp = VK_COMPARE_OP_ALWAYS;
};

struct VertexInputState
{
    std::bitset<kMaxVertexBuffers> BufferValid;
    std::array<VkBuffer, kMaxVertexBuffers> Buffers {};
    std::array<VkDeviceSize, kMaxVertexBuffers> Offsets {};

    bool IndexBufferValid = false;
    VkBuffer IndexBuffer = VK_NULL_HANDLE;
    VkDeviceSize IndexOffset = 0;
    VkIndexType IndexType = VK_INDEX_TYPE_UINT16;
};

struct CommandBufferState
{
    bool Recording = false;
    bool HasBegun = false;
    uint32_t BeginFlags = 0;
    uint64_t BeginEpoch = 0; // Epoch when this command buffer was last begun

    std::unordered_map<VkImage, VkImageLayout> ImageLayouts;

    bool InRenderPass = false;
    VkRenderPass ActiveRenderPass = VK_NULL_HANDLE;
    VkFramebuffer ActiveFramebuffer = VK_NULL_HANDLE;

    std::array<BindPointState, static_cast<uint32_t>(BindPointIndex::Count)> BP {};
    DynamicState Dyn {};
    VertexInputState VI {};

    // Push constants are NOT per-bind-point state - they affect the command buffer globally
    // Store them in timeline order to replay correctly in mixed compute/graphics sequences
    std::vector<PushConstantEntry> PushConstantHistory;

    CommandBufferState()
    {
        // Most games use 1-4 push constant updates per frame
        PushConstantHistory.reserve(8);
    }

    void ResetForNewRecording(uint32_t flags, uint64_t epoch)
    {
        *this = CommandBufferState {};
        Recording = true;
        HasBegun = true;
        BeginFlags = flags;
        BeginEpoch = epoch;
    }

    void ResetAll() { *this = CommandBufferState {}; }
};

struct ReplayParams
{
    bool ReplayGraphicsPipeline = true;

    // Bitmask of sets to replay (1 << setIndex)
    // LIMITATION: 32-bit mask means only sets [0..31] can be requested for replay
    // Sets >= 32 are recorded in DescriptorBindCalls but cannot be selectively replayed
    // For graphics, this is typically sufficient as most pipelines use sets 0-3
    uint32_t RequiredGraphicsSetMask = 0x1;

    VkPipelineLayout OverrideGraphicsLayout = VK_NULL_HANDLE;

    bool ReplayPushConstants = true;
    bool ReplayViewportScissor = true;
    bool ReplayExtendedDynamicState = true;
    bool ReplayVertexIndex = false;
    bool ReplayComputeToo = false;
};

class CommandBufferStateTracker
{
  public:
    // Call this when command buffers are allocated from a pool
    void OnAllocateCommandBuffers(VkCommandPool pool, uint32_t count, const VkCommandBuffer* pCommandBuffers)
    {
        std::scoped_lock lock(_mtx);

        // Initialize pool epoch ONLY if this is a truly new pool (not seen before)
        // Don't overwrite existing epochs - OnResetPool may have already incremented it
        if (_poolEpochs.find(pool) == _poolEpochs.end())
        {
            _poolEpochs[pool] = _globalEpochCounter;
            LOG_DEBUG("Pool {:X} initialized with epoch {}", (size_t) pool, _globalEpochCounter);
        }

        // Map each command buffer to its pool
        for (uint32_t i = 0; i < count; ++i)
        {
            _cmdBufferToPool[pCommandBuffers[i]] = pool;
        }

        LOG_DEBUG("Allocated {} command buffers from pool {:X} (current pool epoch: {})", count, (size_t) pool,
                  _poolEpochs[pool]);
    }

    void OnBegin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo* pBeginInfo)
    {
        const uint32_t flags = (pBeginInfo) ? pBeginInfo->flags : 0;
        std::scoped_lock lock(_mtx);

        // Check if this command buffer is tracked in a pool
        auto poolIt = _cmdBufferToPool.find(cmd);
        if (poolIt == _cmdBufferToPool.end())
        {
            // Command buffer not mapped to any pool - allocation hook may have been missed
            // We'll allow recording but log a warning since this affects epoch validation accuracy
            LOG_WARN("Command buffer {:p} not tracked in any pool (allocation hook missed?). "
                     "Recording will proceed but epoch validation will be disabled for safety.",
                     (void*) cmd);

            // Create/reset state but mark with epoch 0 to signal "untrusted" status
            auto& statePtr = _states[cmd];
            if (!statePtr)
                statePtr = std::make_shared<CommandBufferState>();

            statePtr->ResetForNewRecording(flags, 0); // epoch 0 = untrusted/unvalidated
            return;
        }

        // Get the current epoch for this command buffer's pool
        VkCommandPool pool = poolIt->second;
        auto epochIt = _poolEpochs.find(pool);

        uint64_t currentEpoch;
        if (epochIt == _poolEpochs.end())
        {
            // Pool not in epoch map - should have been initialized during allocation
            // Initialize it now with current global epoch
            currentEpoch = _globalEpochCounter;
            _poolEpochs[pool] = currentEpoch;
            LOG_WARN("Pool {:X} had no epoch - initializing to {} during vkBeginCommandBuffer", (size_t) pool,
                     currentEpoch);
        }
        else
        {
            currentEpoch = epochIt->second;
        }

        // Create new state or reset existing
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->ResetForNewRecording(flags, currentEpoch);
    }

    void OnEnd(VkCommandBuffer cmd)
    {
        std::scoped_lock lock(_mtx);
        auto it = _states.find(cmd);
        if (it != _states.end() && it->second)
            it->second->Recording = false;
    }

    void OnReset(VkCommandBuffer cmd)
    {
        std::scoped_lock lock(_mtx);
        auto it = _states.find(cmd);
        if (it != _states.end() && it->second)
            it->second->ResetAll();
    }

    void OnResetPool(VkCommandPool pool)
    {
        // Invalidate only command buffers from this specific pool by incrementing its epoch
        std::scoped_lock lock(_mtx);

        _globalEpochCounter++;
        _poolEpochs[pool] = _globalEpochCounter;

        LOG_DEBUG("Pool {:X} reset - pool epoch set to {} - command buffers from THIS POOL invalidated until next "
                  "vkBeginCommandBuffer",
                  (size_t) pool, _globalEpochCounter);
    }

    void OnBindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipeline pipeline)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        auto idx = ToIndex(bindPoint);
        if (!idx.has_value())
            return; // Unsupported bind point - ignore

        statePtr->BP[static_cast<uint32_t>(*idx)].Pipeline = pipeline;
    }

    void OnBindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
                              uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets,
                              uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        auto idx = ToIndex(bindPoint);
        if (!idx.has_value())
            return; // Unsupported bind point - ignore

        auto& bp = statePtr->BP[static_cast<uint32_t>(*idx)];
        bp.CurrentPipelineLayout = layout;

        // Record the bind call verbatim
        DescriptorBindCall bindCall;
        bindCall.Layout = layout;
        bindCall.FirstSet = firstSet;
        bindCall.DescriptorSetCount = descriptorSetCount;

        // Copy descriptor sets into vector - validate pointer is non-null when count > 0
        if (descriptorSetCount > 0)
        {
            if (pDescriptorSets)
            {
                bindCall.Sets.assign(pDescriptorSets, pDescriptorSets + descriptorSetCount);
            }
            else
            {
                // Invalid: non-zero count but null pointer - log and don't record this bind
                LOG_ERROR("vkCmdBindDescriptorSets called with descriptorSetCount={} but pDescriptorSets=nullptr",
                          descriptorSetCount);
                return;
            }
        }
        else
        {
            bindCall.Sets.resize(descriptorSetCount, VK_NULL_HANDLE);
        }

        // Copy dynamic offsets - validate pointer is non-null when count > 0
        if (dynamicOffsetCount > 0)
        {
            if (pDynamicOffsets)
            {
                bindCall.DynamicOffsets.assign(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount);
            }
            else
            {
                // Invalid: non-zero count but null pointer - log and don't record this bind
                LOG_ERROR("vkCmdBindDescriptorSets called with dynamicOffsetCount={} but pDynamicOffsets=nullptr",
                          dynamicOffsetCount);
                return;
            }
        }
        // Note: zero count with non-null pointer is benign per Vulkan spec - pointer is ignored

        uint32_t bindCallIndex = static_cast<uint32_t>(bp.DescriptorBindCalls.size());
        bp.DescriptorBindCalls.push_back(std::move(bindCall));

        // Update per-set tracking for quick queries
        for (uint32_t i = 0; i < descriptorSetCount; ++i)
        {
            uint32_t setIdx = firstSet + i;
            if (setIdx >= kMaxDescriptorSets)
                continue;

            auto& binding = bp.Sets[setIdx];
            binding.Bound = true;
            binding.Set = pDescriptorSets[i]; // Safe now - we validated pDescriptorSets above
            binding.BoundWithLayout = layout;
            binding.BindCallIndex = bindCallIndex;
        }
    }

    void OnPushConstants(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
                         VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        auto idx = ToIndex(bindPoint);
        if (!idx.has_value())
            return; // Unsupported bind point - ignore

        // Update the current layout for this bind point (for descriptor set tracking)
        auto& bp = statePtr->BP[static_cast<uint32_t>(*idx)];
        bp.CurrentPipelineLayout = layout;

        // Store push constant in global timeline (NOT per-bind-point)
        // Push constants affect the command buffer state globally, keyed by (layout, stages, range)
        PushConstantEntry entry;
        entry.Layout = layout;
        entry.Stages = stageFlags;
        entry.Offset = offset;
        entry.Size = size;

        // Copy data to Data[0..size) - the offset is only used during replay
        // Defensive: check pValues is non-null when size > 0
        if (size > 0 && pValues && size <= kMaxPushConstantBytes)
        {
            std::memcpy(&entry.Data[0], pValues, size);
        }
        else if (size > kMaxPushConstantBytes && pValues)
        {
            // Clamp to maximum size
            std::memcpy(&entry.Data[0], pValues, kMaxPushConstantBytes);
            entry.Size = kMaxPushConstantBytes;
        }
        else if (size > 0 && !pValues)
        {
            LOG_ERROR("vkCmdPushConstants called with size={} but pValues=nullptr", size);
            return; // Don't store invalid entry
        }

        statePtr->PushConstantHistory.push_back(entry);
    }

    void OnSetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport* pViewports)
    {
        // Defensive: validate pointer when count > 0
        if (count > 0 && !pViewports)
        {
            LOG_ERROR("vkCmdSetViewport called with count={} but pViewports=nullptr", count);
            return;
        }

        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t idx = first + i;
            if (idx < kMaxViewports)
            {
                statePtr->Dyn.Viewports[idx] = pViewports[i];
                statePtr->Dyn.ViewportValidMask.set(idx);
            }
        }
    }

    void OnSetScissor(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkRect2D* pScissors)
    {
        // Defensive: validate pointer when count > 0
        if (count > 0 && !pScissors)
        {
            LOG_ERROR("vkCmdSetScissor called with count={} but pScissors=nullptr", count);
            return;
        }

        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t idx = first + i;
            if (idx < kMaxScissors)
            {
                statePtr->Dyn.Scissors[idx] = pScissors[i];
                statePtr->Dyn.ScissorValidMask.set(idx);
            }
        }
    }

    void OnBindVertexBuffers(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkBuffer* pBuffers,
                             const VkDeviceSize* pOffsets)
    {
        // Defensive: validate pointers when count > 0
        if (count > 0 && (!pBuffers || !pOffsets))
        {
            LOG_ERROR("vkCmdBindVertexBuffers called with count={} but pBuffers={} or pOffsets={}", count,
                      (void*) pBuffers, (void*) pOffsets);
            return;
        }

        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t idx = first + i;
            if (idx < kMaxVertexBuffers)
            {
                statePtr->VI.Buffers[idx] = pBuffers[i];
                statePtr->VI.Offsets[idx] = pOffsets[i];
                statePtr->VI.BufferValid.set(idx);
            }
        }
    }

    void OnBindIndexBuffer(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->VI.IndexBufferValid = true;
        statePtr->VI.IndexBuffer = buffer;
        statePtr->VI.IndexOffset = offset;
        statePtr->VI.IndexType = indexType;
    }

    void OnPipelineBarrier(VkCommandBuffer cmd, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                           VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount,
                           const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount,
                           const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount,
                           const VkImageMemoryBarrier* pImageMemoryBarriers)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        // Track image layout transitions
        for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i)
        {
            const auto& barrier = pImageMemoryBarriers[i];
            statePtr->ImageLayouts[barrier.image] = barrier.newLayout;
        }
    }

    void OnSetCullMode(VkCommandBuffer cmd, VkCullModeFlags cullMode)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.CullMode = cullMode;
        statePtr->Dyn.CullModeSet = true;
    }

    void OnSetFrontFace(VkCommandBuffer cmd, VkFrontFace frontFace)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.FrontFace = frontFace;
        statePtr->Dyn.FrontFaceSet = true;
    }

    void OnSetPrimitiveTopology(VkCommandBuffer cmd, VkPrimitiveTopology primitiveTopology)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.PrimitiveTopology = primitiveTopology;
        statePtr->Dyn.PrimitiveTopologySet = true;
    }

    void OnSetDepthTestEnable(VkCommandBuffer cmd, VkBool32 depthTestEnable)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.DepthTestEnable = depthTestEnable;
        statePtr->Dyn.DepthTestEnableSet = true;
    }

    void OnSetDepthWriteEnable(VkCommandBuffer cmd, VkBool32 depthWriteEnable)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.DepthWriteEnable = depthWriteEnable;
        statePtr->Dyn.DepthWriteEnableSet = true;
    }

    void OnSetDepthCompareOp(VkCommandBuffer cmd, VkCompareOp depthCompareOp)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.DepthCompareOp = depthCompareOp;
        statePtr->Dyn.DepthCompareOpSet = true;
    }

    void OnSetDepthBoundsTestEnable(VkCommandBuffer cmd, VkBool32 depthBoundsTestEnable)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.DepthBoundsTestEnable = depthBoundsTestEnable;
        statePtr->Dyn.DepthBoundsTestEnableSet = true;
    }

    void OnSetStencilTestEnable(VkCommandBuffer cmd, VkBool32 stencilTestEnable)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.StencilTestEnable = stencilTestEnable;
        statePtr->Dyn.StencilTestEnableSet = true;
    }

    void OnSetStencilOp(VkCommandBuffer cmd, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp,
                        VkStencilOp depthFailOp, VkCompareOp compareOp)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->Dyn.StencilOpFaceMask = faceMask;
        statePtr->Dyn.StencilFailOp = failOp;
        statePtr->Dyn.StencilPassOp = passOp;
        statePtr->Dyn.StencilDepthFailOp = depthFailOp;
        statePtr->Dyn.StencilCompareOp = compareOp;
        statePtr->Dyn.StencilOpSet = true;
    }

    // NEW: Render pass tracking
    void OnBeginRenderPass(VkCommandBuffer cmd, const VkRenderPassBeginInfo* pRenderPassBegin)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->InRenderPass = true;
        statePtr->ActiveRenderPass = pRenderPassBegin ? pRenderPassBegin->renderPass : VK_NULL_HANDLE;
        statePtr->ActiveFramebuffer = pRenderPassBegin ? pRenderPassBegin->framebuffer : VK_NULL_HANDLE;
    }

    void OnEndRenderPass(VkCommandBuffer cmd)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->InRenderPass = false;
    }

    void OnCommandBufferDestroyed(VkCommandBuffer cmd)
    {
        std::scoped_lock lock(_mtx);
        _states.erase(cmd);
    }

    void OnFreeCommandBuffers(VkCommandPool pool, uint32_t count, const VkCommandBuffer* pCommandBuffers)
    {
        std::scoped_lock lock(_mtx);
        for (uint32_t i = 0; i < count; ++i)
        {
            _states.erase(pCommandBuffers[i]);
            _cmdBufferToPool.erase(pCommandBuffers[i]);
        }

        LOG_DEBUG("Freed {} command buffers from pool {:X}", count, (size_t) pool);
    }

    // Call this when a command pool is destroyed
    void OnDestroyPool(VkCommandPool pool)
    {
        std::scoped_lock lock(_mtx);

        // Remove all command buffers allocated from this pool
        for (auto it = _cmdBufferToPool.begin(); it != _cmdBufferToPool.end();)
        {
            if (it->second == pool)
            {
                _states.erase(it->first);
                it = _cmdBufferToPool.erase(it);
            }
            else
            {
                ++it;
            }
        }

        _poolEpochs.erase(pool);
        LOG_DEBUG("Pool {:X} destroyed - removed all associated command buffers", (size_t) pool);
    }

    bool CaptureAndReplay(VkCommandBuffer srcCmd, VkCommandBuffer dstCmd, const ReplayParams& params) const
    {
        if (!_hasCachedFns)
        {
            LOG_ERROR("Function table not initialized! Call SetFunctionTable() first.");
            return false;
        }

        return CaptureAndReplay(srcCmd, dstCmd, _cachedFns, params);
    }

    bool ReplayForGraphicsDraw(const VulkanCmdFns& fns, VkCommandBuffer srcCmd, VkCommandBuffer dstCmd,
                               const ReplayParams& params) const
    {
        CommandBufferState snapshot;
        if (!TryGetSnapshot(srcCmd, snapshot))
        {
            LOG_WARN("Failed to get snapshot for command buffer {:p} - may have been invalidated by pool reset",
                     (void*) srcCmd);
            return false;
        }

        // 1. Pipeline
        if (params.ReplayGraphicsPipeline)
        {
            auto& gfx = snapshot.BP[static_cast<uint32_t>(BindPointIndex::Graphics)];
            if (gfx.Pipeline && fns.CmdBindPipeline)
                fns.CmdBindPipeline(dstCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx.Pipeline);
        }

        // 2. Descriptor Sets - use unified helper with slicing
        {
            auto& gfx = snapshot.BP[static_cast<uint32_t>(BindPointIndex::Graphics)];
            ReplayDescriptorSets(fns, dstCmd, gfx, VK_PIPELINE_BIND_POINT_GRAPHICS, params.RequiredGraphicsSetMask,
                                 params.OverrideGraphicsLayout);
        }

        // 3. Push Constants
        if (params.ReplayPushConstants)
        {
            // Replay push constants from global timeline in order
            // Filter by stage mask compatibility (optional) and layout compatibility
            constexpr VkShaderStageFlags graphicsStages =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_GEOMETRY_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;

            for (const auto& entry : snapshot.PushConstantHistory)
            {
                // Optional: filter to graphics-relevant stages (conservative - keeps ALL_GRAPHICS too)
                // Skip only if exclusively compute/ray-tracing stages
                bool hasGraphicsStages = (entry.Stages & graphicsStages) != 0;
                bool hasAllGraphics = (entry.Stages & VK_SHADER_STAGE_ALL_GRAPHICS) != 0;

                if (!hasGraphicsStages && !hasAllGraphics)
                {
                    // This is exclusively compute or ray tracing - skip for graphics replay
                    continue;
                }

                VkPipelineLayout layoutToUse =
                    params.OverrideGraphicsLayout ? params.OverrideGraphicsLayout : entry.Layout;

                if (layoutToUse && entry.Size > 0 && fns.CmdPushConstants)
                {
                    fns.CmdPushConstants(dstCmd, layoutToUse, entry.Stages, entry.Offset, entry.Size, &entry.Data[0]);
                }
            }
        }

        // 4. Dynamic State
        if (params.ReplayViewportScissor)
        {
            ReplayViewports(fns, dstCmd, snapshot.Dyn);
            ReplayScissors(fns, dstCmd, snapshot.Dyn);
        }

        // 4.5. Extended Dynamic State
        if (params.ReplayExtendedDynamicState)
        {
            ReplayExtendedDynamicState(fns, dstCmd, snapshot.Dyn);
        }

        // 5. Vertex/Index
        if (params.ReplayVertexIndex)
        {
            ReplayVertexBuffers(fns, dstCmd, snapshot.VI);
            if (snapshot.VI.IndexBufferValid && fns.CmdBindIndexBuffer)
            {
                fns.CmdBindIndexBuffer(dstCmd, snapshot.VI.IndexBuffer, snapshot.VI.IndexOffset, snapshot.VI.IndexType);
            }
        }

        return true;
    }

    void SetFunctionTable(const VulkanCmdFns& fns)
    {
        _cachedFns = fns;
        _hasCachedFns = true;
    }

  private:
    // Helper method to replay descriptor sets with proper slicing and timeline ordering
    void ReplayDescriptorSets(const VulkanCmdFns& fns, VkCommandBuffer dstCmd, const BindPointState& bindPoint,
                              VkPipelineBindPoint bindPointType, uint32_t requiredSetMask,
                              VkPipelineLayout overrideLayout) const
    {
        if (!fns.CmdBindDescriptorSets)
            return;

        // Track which bind calls contain at least one required set - use dynamic container to handle any number of
        // calls
        const size_t numCalls = bindPoint.DescriptorBindCalls.size();
        if (numCalls == 0)
            return;

        std::vector<bool> callsNeeded(numCalls, false);

        // Pre-compute and cache range end for each call to avoid redundant overflow checks
        std::vector<uint32_t> callEnds(numCalls);
        std::vector<bool> callRangeValid(numCalls, false);

        for (size_t callIdx = 0; callIdx < numCalls; ++callIdx)
        {
            const auto& call = bindPoint.DescriptorBindCalls[callIdx];
            const uint64_t callEnd64 = (uint64_t) call.FirstSet + call.DescriptorSetCount;

            // Validate range with overflow check
            if (call.DescriptorSetCount > 0)
            {
                if (callEnd64 > (uint64_t) UINT32_MAX + 1ull || (uint32_t) callEnd64 < call.FirstSet)
                {
                    LOG_ERROR("Descriptor set call {} range overflow (firstSet={}, count={}) - skipping", callIdx,
                              call.FirstSet, call.DescriptorSetCount);
                    continue;
                }
            }

            callEnds[callIdx] = (uint32_t) callEnd64;
            callRangeValid[callIdx] = true;
        }

        // First pass: identify which calls contain any required set
        for (uint32_t setIdx = 0; setIdx < kMaxDescriptorSets; ++setIdx)
        {
            if (!((requiredSetMask >> setIdx) & 1))
                continue;

            const auto& binding = bindPoint.Sets[setIdx];
            if (!binding.Bound || binding.Set == VK_NULL_HANDLE)
                continue;

            uint32_t callIdx = binding.BindCallIndex;
            if (callIdx >= numCalls)
            {
                LOG_WARN("Set {} references bind call {} but only {} calls exist - skipping", setIdx, callIdx,
                         numCalls);
                continue;
            }

            if (!callRangeValid[callIdx])
                continue; // Already logged error during range validation

            // Validate that this set actually belongs to this call's range
            const auto& call = bindPoint.DescriptorBindCalls[callIdx];
            uint32_t callEnd = callEnds[callIdx];

            if (setIdx < call.FirstSet || setIdx >= callEnd)
            {
                // Only warn if count > 0 (avoid underflow in log message)
                if (call.DescriptorSetCount > 0)
                {
                    LOG_WARN("Set {} tracked with call {} but outside call range [{}..{}] - skipping", setIdx, callIdx,
                             call.FirstSet, callEnd - 1);
                }
                continue;
            }

            // Mark this call as needed
            callsNeeded[callIdx] = true;
        }

        // Second pass: replay needed calls in timeline order
        for (uint32_t callIdx = 0; callIdx < numCalls; ++callIdx)
        {
            if (!callsNeeded[callIdx] || !callRangeValid[callIdx])
                continue;

            const auto& call = bindPoint.DescriptorBindCalls[callIdx];
            uint32_t callEnd = callEnds[callIdx]; // Use cached value

            VkPipelineLayout layoutToUse = overrideLayout ? overrideLayout : call.Layout;

            if (!layoutToUse)
                continue;

            // Validate consistency between DescriptorSetCount and Sets.size()
            if (call.DescriptorSetCount > call.Sets.size())
            {
                LOG_ERROR("Descriptor set call {} has count={} but Sets.size()={} - skipping to avoid driver crash",
                          callIdx, call.DescriptorSetCount, call.Sets.size());
                continue;
            }

            // CRITICAL: If this call has dynamic offsets, we MUST replay it verbatim (no slicing)
            // Dynamic offsets are paired with descriptor sets in a complex way that requires
            // pipeline layout introspection to understand - without that, slicing is unsafe
            if (!call.DynamicOffsets.empty())
            {
                // Additional safety check for verbatim replay path
                const VkDescriptorSet* pSetsToUse = (call.DescriptorSetCount > 0) ? call.Sets.data() : nullptr;

                // Replay the entire original call verbatim
                fns.CmdBindDescriptorSets(dstCmd, bindPointType, layoutToUse, call.FirstSet, call.DescriptorSetCount,
                                          pSetsToUse, (uint32_t) call.DynamicOffsets.size(),
                                          call.DynamicOffsets.data());

                LOG_DEBUG("Replayed descriptor set call {} verbatim (has {} dynamic offsets, firstSet={}, count={})",
                          callIdx, call.DynamicOffsets.size(), call.FirstSet, call.DescriptorSetCount);
                continue;
            }

            // No dynamic offsets - safe to slice to only the required sets
            // Build list of which sets from this call are actually required
            std::vector<uint32_t> requiredSetIndices; // Absolute set indices

            for (uint32_t setIdx = 0; setIdx < kMaxDescriptorSets; ++setIdx)
            {
                if (!((requiredSetMask >> setIdx) & 1))
                    continue;

                const auto& binding = bindPoint.Sets[setIdx];
                if (!binding.Bound || binding.Set == VK_NULL_HANDLE)
                    continue;

                if (binding.BindCallIndex != callIdx)
                    continue;

                // Validate set is within call range (using pre-computed safe callEnd)
                if (setIdx >= call.FirstSet && setIdx < callEnd)
                {
                    requiredSetIndices.push_back(setIdx);
                }
            }

            if (requiredSetIndices.empty())
                continue;

            // requiredSetIndices should already be sorted (we iterate setIdx in order)
            // but sort anyway for robustness
            std::sort(requiredSetIndices.begin(), requiredSetIndices.end());

            // Group into contiguous ranges for efficient binding
            for (size_t i = 0; i < requiredSetIndices.size();)
            {
                uint32_t rangeStart = requiredSetIndices[i];
                uint32_t rangeEnd = rangeStart;

                // Find end of contiguous range
                while (i + 1 < requiredSetIndices.size() && requiredSetIndices[i + 1] == rangeEnd + 1)
                {
                    rangeEnd = requiredSetIndices[++i];
                }
                i++;

                uint32_t rangeCount = rangeEnd - rangeStart + 1;

                // Extract sets for this range from the original call
                std::vector<VkDescriptorSet> setsToRebind;
                setsToRebind.reserve(rangeCount);

                // Convert absolute indices to call-relative indices and extract sets
                bool allValid = true;
                for (uint32_t absoluteSetIdx = rangeStart; absoluteSetIdx <= rangeEnd; ++absoluteSetIdx)
                {
                    if (absoluteSetIdx < call.FirstSet || absoluteSetIdx >= callEnd)
                    {
                        LOG_ERROR("Set {} outside call range [{}, {}) - internal error", absoluteSetIdx, call.FirstSet,
                                  callEnd);
                        allValid = false;
                        break;
                    }

                    uint32_t setIndexInCall = absoluteSetIdx - call.FirstSet;

                    if (setIndexInCall >= call.Sets.size())
                    {
                        LOG_ERROR("Set index {} maps to out-of-bounds call array index {} (size {})", absoluteSetIdx,
                                  setIndexInCall, call.Sets.size());
                        allValid = false;
                        break;
                    }

                    setsToRebind.push_back(call.Sets[setIndexInCall]);
                }

                // Replay this contiguous range if all sets were valid
                if (allValid && !setsToRebind.empty())
                {
                    fns.CmdBindDescriptorSets(dstCmd, bindPointType, layoutToUse, rangeStart,
                                              (uint32_t) setsToRebind.size(), setsToRebind.data(), 0, nullptr);
                }
            }
        }
    }

    void ReplayVertexBuffers(const VulkanCmdFns& fns, VkCommandBuffer dst, const VertexInputState& vi) const
    {
        if (!fns.CmdBindVertexBuffers)
            return;

        uint32_t start = 0;
        while (start < kMaxVertexBuffers)
        {
            if (!vi.BufferValid.test(start))
            {
                start++;
                continue;
            }

            uint32_t count = 0;
            while ((start + count) < kMaxVertexBuffers && vi.BufferValid.test(start + count))
            {
                count++;
            }

            const VkBuffer* pBuf = &vi.Buffers[start];
            const VkDeviceSize* pOff = &vi.Offsets[start];
            fns.CmdBindVertexBuffers(dst, start, count, pBuf, pOff);

            start += count;
        }
    }

    void ReplayViewports(const VulkanCmdFns& fns, VkCommandBuffer dst, const DynamicState& dyn) const
    {
        if (!fns.CmdSetViewport)
            return;

        uint32_t start = 0;
        while (start < kMaxViewports)
        {
            if (!dyn.ViewportValidMask.test(start))
            {
                start++;
                continue;
            }

            uint32_t count = 0;
            while (start + count < kMaxViewports && dyn.ViewportValidMask.test(start + count))
            {
                count++;
            }

            fns.CmdSetViewport(dst, start, count, &dyn.Viewports[start]);
            start += count;
        }
    }

    void ReplayScissors(const VulkanCmdFns& fns, VkCommandBuffer dst, const DynamicState& dyn) const
    {
        if (!fns.CmdSetScissor)
            return;

        uint32_t start = 0;
        while (start < kMaxScissors)
        {
            if (!dyn.ScissorValidMask.test(start))
            {
                start++;
                continue;
            }

            uint32_t count = 0;
            while (start + count < kMaxScissors && dyn.ScissorValidMask.test(start + count))
            {
                count++;
            }

            fns.CmdSetScissor(dst, start, count, &dyn.Scissors[start]);
            start += count;
        }
    }

    void ReplayExtendedDynamicState(const VulkanCmdFns& fns, VkCommandBuffer dst, const DynamicState& dyn) const
    {
        // Only replay if actually set by the app
        if (dyn.CullModeSet && fns.CmdSetCullMode)
            fns.CmdSetCullMode(dst, dyn.CullMode);

        if (dyn.FrontFaceSet && fns.CmdSetFrontFace)
            fns.CmdSetFrontFace(dst, dyn.FrontFace);

        if (dyn.PrimitiveTopologySet && fns.CmdSetPrimitiveTopology)
            fns.CmdSetPrimitiveTopology(dst, dyn.PrimitiveTopology);

        if (dyn.DepthTestEnableSet && fns.CmdSetDepthTestEnable)
            fns.CmdSetDepthTestEnable(dst, dyn.DepthTestEnable);

        if (dyn.DepthWriteEnableSet && fns.CmdSetDepthWriteEnable)
            fns.CmdSetDepthWriteEnable(dst, dyn.DepthWriteEnable);

        if (dyn.DepthCompareOpSet && fns.CmdSetDepthCompareOp)
            fns.CmdSetDepthCompareOp(dst, dyn.DepthCompareOp);

        if (dyn.DepthBoundsTestEnableSet && fns.CmdSetDepthBoundsTestEnable)
            fns.CmdSetDepthBoundsTestEnable(dst, dyn.DepthBoundsTestEnable);

        if (dyn.StencilTestEnableSet && fns.CmdSetStencilTestEnable)
            fns.CmdSetStencilTestEnable(dst, dyn.StencilTestEnable);

        if (dyn.StencilOpSet && fns.CmdSetStencilOp)
        {
            fns.CmdSetStencilOp(dst, dyn.StencilOpFaceMask, dyn.StencilFailOp, dyn.StencilPassOp,
                                dyn.StencilDepthFailOp, dyn.StencilCompareOp);
        }
    }

    bool TryGetSnapshot(VkCommandBuffer cmd, CommandBufferState& out) const
    {
        std::scoped_lock lock(_mtx);
        auto it = _states.find(cmd);
        if (it == _states.end() || !it->second)
            return false;

        // Check if this command buffer is tracked in a pool
        auto poolIt = _cmdBufferToPool.find(cmd);
        if (poolIt == _cmdBufferToPool.end())
        {
            // Command buffer not mapped to any pool - allocation hook may have been missed
            // This is potentially unsafe as we can't validate against pool-specific epochs
            LOG_WARN("Command buffer {:p} not tracked in any pool (allocation hook missed?). "
                     "Cannot validate epoch - refusing replay for safety.",
                     (void*) cmd);
            return false;
        }

        // Get the current epoch for this command buffer's pool
        VkCommandPool pool = poolIt->second;
        auto epochIt = _poolEpochs.find(pool);
        if (epochIt == _poolEpochs.end())
        {
            // Pool exists in mapping but has no epoch - should not happen if properly initialized
            LOG_ERROR("Pool {:X} for command buffer {:p} has no epoch entry. Internal state corruption?", (size_t) pool,
                      (void*) cmd);
            return false;
        }

        uint64_t currentPoolEpoch = epochIt->second;

        // Check if this command buffer's state is stale (invalidated by its pool's reset)
        if (it->second->BeginEpoch < currentPoolEpoch)
        {
            LOG_WARN("Command buffer {:p} has stale state (epoch {} < pool {:X} epoch {}), refusing replay. "
                     "This command buffer was invalidated by vkResetCommandPool and must not be used until "
                     "vkBeginCommandBuffer is called.",
                     (void*) cmd, it->second->BeginEpoch, (size_t) pool, currentPoolEpoch);
            return false;
        }

        // Deep copy the state under lock - this is now a true immutable snapshot
        out = *it->second;
        return true;
    }

    bool HasFunctionTable() const { return _hasCachedFns; }

    bool CaptureAndReplay(VkCommandBuffer srcCmd, VkCommandBuffer dstCmd, const VulkanCmdFns& fns,
                          const ReplayParams& params) const
    {
        CommandBufferState snapshot;

        // Capture state from source command buffer (deep copy under lock)
        {
            std::scoped_lock lock(_mtx);

            auto it = _states.find(srcCmd);

            if (it == _states.end())
            {
                LOG_WARN("Can't found captured state for command buffer {:p}", (void*) srcCmd);
                return false;
            }

            if (!it->second)
            {
                LOG_WARN("Captured state is empty for command buffer {:p}", (void*) srcCmd);
                return false;
            }

            // Check if this command buffer is tracked in a pool
            auto poolIt = _cmdBufferToPool.find(srcCmd);
            if (poolIt == _cmdBufferToPool.end())
            {
                // Command buffer not mapped to any pool - allocation hook may have been missed
                // This is potentially unsafe as we can't validate against pool-specific epochs
                LOG_WARN("Command buffer {:p} not tracked in any pool (allocation hook missed?). "
                         "Cannot validate epoch - refusing replay for safety.",
                         (void*) srcCmd);
                return false;
            }

            // Get the current epoch for this command buffer's pool
            VkCommandPool pool = poolIt->second;
            auto epochIt = _poolEpochs.find(pool);
            if (epochIt == _poolEpochs.end())
            {
                // Pool exists in mapping but has no epoch - should not happen if properly initialized
                LOG_ERROR("Pool {:X} for command buffer {:p} has no epoch entry. Internal state corruption?",
                          (size_t) pool, (void*) srcCmd);
                return false;
            }

            uint64_t currentPoolEpoch = epochIt->second;

            // Check if this command buffer's state is stale (invalidated by pool reset)
            if (it->second->BeginEpoch < currentPoolEpoch)
            {
                LOG_WARN("Command buffer {:p} has stale state (epoch {} < pool {:X} epoch {}), refusing replay. "
                         "This command buffer was invalidated by vkResetCommandPool and must not be used until "
                         "vkBeginCommandBuffer is called.",
                         (void*) srcCmd, it->second->BeginEpoch, (size_t) pool, currentPoolEpoch);
                return false;
            }

            // Deep copy the state - now a true immutable snapshot
            snapshot = *it->second;
        }

        // Replay to destination (no lock needed - working with copied snapshot)
        return ReplayFromSnapshot(fns, snapshot, dstCmd, params);
    }

    bool ReplayFromSnapshot(const VulkanCmdFns& fns, const CommandBufferState& snapshot, VkCommandBuffer dstCmd,
                            const ReplayParams& params) const
    {
        // 1. Graphics Pipeline
        if (params.ReplayGraphicsPipeline)
        {
            auto& gfx = snapshot.BP[static_cast<uint32_t>(BindPointIndex::Graphics)];
            if (gfx.Pipeline && fns.CmdBindPipeline)
                fns.CmdBindPipeline(dstCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx.Pipeline);
        }

        // 1.5. Compute Pipeline (if requested)
        if (params.ReplayComputeToo)
        {
            auto& comp = snapshot.BP[static_cast<uint32_t>(BindPointIndex::Compute)];
            if (comp.Pipeline && fns.CmdBindPipeline)
                fns.CmdBindPipeline(dstCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comp.Pipeline);
        }

        // 2. Descriptor Sets - Graphics
        {
            auto& gfx = snapshot.BP[static_cast<uint32_t>(BindPointIndex::Graphics)];
            ReplayDescriptorSets(fns, dstCmd, gfx, VK_PIPELINE_BIND_POINT_GRAPHICS, params.RequiredGraphicsSetMask,
                                 params.OverrideGraphicsLayout);
        }

        // 2.5. Descriptor Sets - Compute (if requested)
        if (params.ReplayComputeToo)
        {
            auto& comp = snapshot.BP[static_cast<uint32_t>(BindPointIndex::Compute)];

            // For compute, replay all descriptor bind calls verbatim in timeline order
            // This avoids the kMaxDescriptorSets limitation and ensures correctness
            if (fns.CmdBindDescriptorSets)
            {
                for (const auto& call : comp.DescriptorBindCalls)
                {
                    if (!call.Layout || call.DescriptorSetCount == 0)
                        continue;

                    // Validate consistency before replay
                    if (call.DescriptorSetCount > call.Sets.size())
                    {
                        LOG_ERROR("Compute descriptor set call has count={} but Sets.size()={} - skipping",
                                  call.DescriptorSetCount, call.Sets.size());
                        continue;
                    }

                    // Sanity check: validate dynamic offset data consistency
                    if (!call.DynamicOffsets.empty() && call.DescriptorSetCount == 0)
                    {
                        LOG_WARN("Compute bind call has {} dynamic offsets but zero sets (firstSet={}) - possible "
                                 "corruption, skipping",
                                 call.DynamicOffsets.size(), call.FirstSet);
                        continue;
                    }

                    // Additional safety: cap dynamic offset count to avoid pathological driver behavior
                    constexpr uint32_t kMaxSaneDynamicOffsets = 1024; // Generous upper bound
                    if (call.DynamicOffsets.size() > kMaxSaneDynamicOffsets)
                    {
                        LOG_ERROR("Compute bind call has {} dynamic offsets (exceeds sanity limit of {}) - possible "
                                  "corruption, skipping",
                                  call.DynamicOffsets.size(), kMaxSaneDynamicOffsets);
                        continue;
                    }

                    const VkDescriptorSet* pSets = call.Sets.data();
                    const uint32_t* pDynamicOffsets =
                        call.DynamicOffsets.empty() ? nullptr : call.DynamicOffsets.data();

                    fns.CmdBindDescriptorSets(dstCmd, VK_PIPELINE_BIND_POINT_COMPUTE, call.Layout, call.FirstSet,
                                              call.DescriptorSetCount, pSets, (uint32_t) call.DynamicOffsets.size(),
                                              pDynamicOffsets);
                }
            }
        }

        // 3. Push Constants - Graphics
        if (params.ReplayPushConstants)
        {
            // Replay push constants from global timeline in order
            // Filter by stage mask compatibility (optional) and layout compatibility
            constexpr VkShaderStageFlags graphicsStages =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_GEOMETRY_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;

            for (const auto& entry : snapshot.PushConstantHistory)
            {
                // Optional: filter to graphics-relevant stages (conservative - keeps ALL_GRAPHICS too)
                // Skip only if exclusively compute/ray-tracing stages
                bool hasGraphicsStages = (entry.Stages & graphicsStages) != 0;
                bool hasAllGraphics = (entry.Stages & VK_SHADER_STAGE_ALL_GRAPHICS) != 0;

                if (!hasGraphicsStages && !hasAllGraphics)
                {
                    // This is exclusively compute or ray tracing - skip for graphics replay
                    continue;
                }

                VkPipelineLayout layoutToUse =
                    params.OverrideGraphicsLayout ? params.OverrideGraphicsLayout : entry.Layout;

                if (layoutToUse && entry.Size > 0 && fns.CmdPushConstants)
                {
                    fns.CmdPushConstants(dstCmd, layoutToUse, entry.Stages, entry.Offset, entry.Size, &entry.Data[0]);
                }
            }
        }

        // 3.5. Push Constants - Compute (if requested)
        if (params.ReplayComputeToo && params.ReplayPushConstants)
        {
            // Replay compute push constants from global timeline in order
            constexpr VkShaderStageFlags computeStages = VK_SHADER_STAGE_COMPUTE_BIT;

            for (const auto& entry : snapshot.PushConstantHistory)
            {
                // Only replay compute-stage push constants
                if (!(entry.Stages & computeStages))
                    continue;

                if (entry.Layout && entry.Size > 0 && fns.CmdPushConstants)
                {
                    fns.CmdPushConstants(dstCmd, entry.Layout, entry.Stages, entry.Offset, entry.Size, &entry.Data[0]);
                }
            }
        }

        // 4. Dynamic State
        if (params.ReplayViewportScissor)
        {
            ReplayViewports(fns, dstCmd, snapshot.Dyn);
            ReplayScissors(fns, dstCmd, snapshot.Dyn);
        }

        // 4.5. Extended Dynamic State
        if (params.ReplayExtendedDynamicState)
        {
            ReplayExtendedDynamicState(fns, dstCmd, snapshot.Dyn);
        }

        // 5. Vertex/Index
        if (params.ReplayVertexIndex)
        {
            ReplayVertexBuffers(fns, dstCmd, snapshot.VI);
            if (snapshot.VI.IndexBufferValid && fns.CmdBindIndexBuffer)
            {
                fns.CmdBindIndexBuffer(dstCmd, snapshot.VI.IndexBuffer, snapshot.VI.IndexOffset, snapshot.VI.IndexType);
            }
        }

        return true;
    }

    mutable std::mutex _mtx;
    std::unordered_map<VkCommandBuffer, std::shared_ptr<CommandBufferState>> _states;

    VulkanCmdFns _cachedFns {};
    bool _hasCachedFns = false;

    // Per-pool epoch tracking for accurate invalidation
    std::unordered_map<VkCommandBuffer, VkCommandPool> _cmdBufferToPool;
    std::unordered_map<VkCommandPool, uint64_t> _poolEpochs;
    uint64_t _globalEpochCounter = 1;
};
} // namespace vk_state
