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
static constexpr uint32_t kMaxDescriptorSets = 8;
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

inline BindPointIndex ToIndex(VkPipelineBindPoint bp)
{
    return (bp == VK_PIPELINE_BIND_POINT_COMPUTE) ? BindPointIndex::Compute : BindPointIndex::Graphics;
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
    std::vector<uint32_t> DynamicOffsets;
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
    VkPipelineLayout CurrentPipelineLayout = VK_NULL_HANDLE; // Last layout seen in BindDescriptorSets/Push

    std::array<DescriptorBinding, kMaxDescriptorSets> Sets {};

    // Reserve capacity to avoid reallocations during recording
    std::vector<PushConstantEntry> PushConstantHistory;

    BindPointState()
    {
        // Most games use 1-4 push constant updates per frame
        PushConstantHistory.reserve(8);
    }
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

    std::unordered_map<VkImage, VkImageLayout> ImageLayouts;

    bool InRenderPass = false;
    VkRenderPass ActiveRenderPass = VK_NULL_HANDLE;
    VkFramebuffer ActiveFramebuffer = VK_NULL_HANDLE;

    std::array<BindPointState, static_cast<uint32_t>(BindPointIndex::Count)> BP {};
    DynamicState Dyn {};
    VertexInputState VI {};

    void ResetForNewRecording(uint32_t flags)
    {
        *this = CommandBufferState {};
        Recording = true;
        HasBegun = true;
        BeginFlags = flags;
    }

    void ResetAll() { *this = CommandBufferState {}; }
};

struct ReplayParams
{
    bool ReplayGraphicsPipeline = true;

    // Bitmask of sets to replay (1 << setIndex)
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
    void OnBegin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo* pBeginInfo)
    {
        const uint32_t flags = (pBeginInfo) ? pBeginInfo->flags : 0;
        std::scoped_lock lock(_mtx);

        // Create new state or reset existing
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->ResetForNewRecording(flags);
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

    // Generic pool reset handler
    // template <typename PoolToCmdsFunc> void OnResetPool(VkCommandPool pool, PoolToCmdsFunc getCmdsForPool)
    //{
    //    std::scoped_lock lock(_mtx);
    //    auto cmds = getCmdsForPool(pool);
    //    for (auto c : cmds)
    //    {
    //        auto it = _states.find(c);
    //        if (it != _states.end() && it->second)
    //            it->second->ResetAll();
    //    }
    //}

    // Remove the template version and add this simple version:
    void OnResetPool(VkCommandPool pool)
    {
        std::scoped_lock lock(_mtx);

        // Reset ALL tracked command buffers (less efficient but simpler)
        // This works because resetting a pool implicitly resets all its command buffers
        for (auto& [cmd, statePtr] : _states)
        {
            if (statePtr)
                statePtr->ResetAll();
        }

        LOG_WARN("Pool {:X} reset - cleared ALL command buffer states (no pool tracking)", (size_t) pool);
    }

    void OnBindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipeline pipeline)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        statePtr->BP[static_cast<uint32_t>(ToIndex(bindPoint))].Pipeline = pipeline;
    }

    void OnBindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
                              uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets,
                              uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        auto& bp = statePtr->BP[static_cast<uint32_t>(ToIndex(bindPoint))];

        bp.CurrentPipelineLayout = layout;

        uint32_t dynOffsetIndex = 0;

        for (uint32_t i = 0; i < descriptorSetCount; ++i)
        {
            uint32_t setIdx = firstSet + i;
            if (setIdx >= kMaxDescriptorSets)
                continue;

            bp.Sets[setIdx].Bound = true;
            bp.Sets[setIdx].Set = pDescriptorSets ? pDescriptorSets[i] : VK_NULL_HANDLE;
            bp.Sets[setIdx].BoundWithLayout = layout;
            bp.Sets[setIdx].DynamicOffsets.clear();

            if (pDynamicOffsets && dynOffsetIndex < dynamicOffsetCount)
            {
                if (descriptorSetCount == 1)
                {
                    bp.Sets[setIdx].DynamicOffsets.assign(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount);
                }
            }
        }
    }

    void OnPushConstants(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
                         VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
    {
        std::scoped_lock lock(_mtx);
        auto& statePtr = _states[cmd];
        if (!statePtr)
            statePtr = std::make_shared<CommandBufferState>();

        auto& bp = statePtr->BP[static_cast<uint32_t>(ToIndex(bindPoint))]; // Use bindPoint parameter

        bp.CurrentPipelineLayout = layout;

        // Create new entry for this push constant update
        PushConstantEntry entry;
        entry.Layout = layout;
        entry.Stages = stageFlags;
        entry.Offset = offset;
        entry.Size = size;

        // Copy data into the entry
        if (size > 0 && offset + size <= kMaxPushConstantBytes)
        {
            std::memcpy(&entry.Data[offset], pValues, size);
        }
        else if (offset + size > kMaxPushConstantBytes)
        {
            // Clamp to maximum size
            uint32_t clampedSize = kMaxPushConstantBytes - offset;
            if (clampedSize > 0)
            {
                std::memcpy(&entry.Data[offset], pValues, clampedSize);
                entry.Size = clampedSize;
            }
        }

        bp.PushConstantHistory.push_back(entry);
    }

    void OnSetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport* pViewports)
    {
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
        }
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
        std::shared_ptr<CommandBufferState> snapshot;
        if (!TryGetSnapshot(srcCmd, snapshot) || !snapshot)
            return false;

        // 1. Pipeline
        if (params.ReplayGraphicsPipeline)
        {
            auto& gfx = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Graphics)];
            if (gfx.Pipeline && fns.CmdBindPipeline)
                fns.CmdBindPipeline(dstCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx.Pipeline);
        }

        // 2. Descriptor Sets
        {
            auto& gfx = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Graphics)];

            if (fns.CmdBindDescriptorSets)
            {
                for (uint32_t i = 0; i < kMaxDescriptorSets; ++i)
                {
                    if (!((params.RequiredGraphicsSetMask >> i) & 1))
                        continue;

                    const auto& sb = gfx.Sets[i];
                    if (!sb.Bound || sb.Set == VK_NULL_HANDLE)
                        continue;

                    // Use override layout if explicitly provided, otherwise use the original layout
                    // that the descriptor set was bound with to ensure layout compatibility
                    VkPipelineLayout layoutToUse =
                        params.OverrideGraphicsLayout ? params.OverrideGraphicsLayout : sb.BoundWithLayout;
                    if (!layoutToUse)
                        continue;

                    fns.CmdBindDescriptorSets(dstCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutToUse, i, 1, &sb.Set,
                                              (uint32_t) sb.DynamicOffsets.size(), sb.DynamicOffsets.data());
                }
            }
        }

        // 3. Push Constants
        if (params.ReplayPushConstants)
        {
            auto& gfx = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Graphics)];

            for (const auto& entry : gfx.PushConstantHistory)
            {
                VkPipelineLayout layoutToUse =
                    params.OverrideGraphicsLayout ? params.OverrideGraphicsLayout : entry.Layout;

                if (layoutToUse && entry.Size > 0 && fns.CmdPushConstants)
                {
                    fns.CmdPushConstants(dstCmd, layoutToUse, entry.Stages, entry.Offset, entry.Size,
                                         &entry.Data[entry.Offset]);
                }
            }
        }

        // 4. Dynamic State
        if (params.ReplayViewportScissor)
        {
            ReplayViewports(fns, dstCmd, snapshot->Dyn);
            ReplayScissors(fns, dstCmd, snapshot->Dyn);
        }

        // 4.5. Extended Dynamic State
        if (params.ReplayExtendedDynamicState)
        {
            ReplayExtendedDynamicState(fns, dstCmd, snapshot->Dyn);
        }

        // 5. Vertex/Index
        if (params.ReplayVertexIndex)
        {
            ReplayVertexBuffers(fns, dstCmd, snapshot->VI);
            if (snapshot->VI.IndexBufferValid && fns.CmdBindIndexBuffer)
            {
                fns.CmdBindIndexBuffer(dstCmd, snapshot->VI.IndexBuffer, snapshot->VI.IndexOffset,
                                       snapshot->VI.IndexType);
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

    bool TryGetSnapshot(VkCommandBuffer cmd, std::shared_ptr<CommandBufferState>& out) const
    {
        std::scoped_lock lock(_mtx);
        auto it = _states.find(cmd);
        if (it == _states.end())
            return false;
        out = it->second;
        return true;
    }

    bool HasFunctionTable() const { return _hasCachedFns; }

    bool CaptureAndReplay(VkCommandBuffer srcCmd, VkCommandBuffer dstCmd, const VulkanCmdFns& fns,
                          const ReplayParams& params) const
    {
        std::shared_ptr<CommandBufferState> snapshot;

        // Capture state from source command buffer (fast pointer copy under lock)
        {
            std::scoped_lock lock(_mtx);

            auto it = _states.find(srcCmd);

            if (it == _states.end())
            {
                LOG_WARN("Can't found captured state for command buffer {:p}", (void*) srcCmd);
                return false;
            }

            snapshot = it->second;
        }

        if (!snapshot)
        {
            LOG_WARN("Captured state is empty for command buffer {:p}", (void*) srcCmd);
            return false;
        }

        // Replay to destination (no lock needed - reading from immutable snapshot)
        return ReplayFromSnapshot(fns, snapshot, dstCmd, params);
    }

    bool ReplayFromSnapshot(const VulkanCmdFns& fns, std::shared_ptr<CommandBufferState> snapshot,
                            VkCommandBuffer dstCmd, const ReplayParams& params) const
    {
        if (!snapshot)
        {
            LOG_WARN("Snapshot is null for command buffer {:p}", (void*) dstCmd);
            return false;
        }

        // 1. Graphics Pipeline
        if (params.ReplayGraphicsPipeline)
        {
            auto& gfx = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Graphics)];
            if (gfx.Pipeline && fns.CmdBindPipeline)
                fns.CmdBindPipeline(dstCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx.Pipeline);
        }

        // 1.5. Compute Pipeline (if requested)
        if (params.ReplayComputeToo)
        {
            auto& comp = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Compute)];
            if (comp.Pipeline && fns.CmdBindPipeline)
                fns.CmdBindPipeline(dstCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comp.Pipeline);
        }

        // 2. Descriptor Sets - Graphics
        {
            auto& gfx = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Graphics)];

            if (fns.CmdBindDescriptorSets)
            {
                for (uint32_t i = 0; i < kMaxDescriptorSets; ++i)
                {
                    if (!((params.RequiredGraphicsSetMask >> i) & 1))
                        continue;

                    const auto& sb = gfx.Sets[i];
                    if (!sb.Bound || sb.Set == VK_NULL_HANDLE)
                        continue;

                    // Use override layout if explicitly provided, otherwise use the original layout
                    // that the descriptor set was bound with to ensure layout compatibility
                    VkPipelineLayout layoutToUse =
                        params.OverrideGraphicsLayout ? params.OverrideGraphicsLayout : sb.BoundWithLayout;
                    if (!layoutToUse)
                        continue;

                    fns.CmdBindDescriptorSets(dstCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutToUse, i, 1, &sb.Set,
                                              (uint32_t) sb.DynamicOffsets.size(), sb.DynamicOffsets.data());
                }
            }
        }

        // 2.5. Descriptor Sets - Compute (if requested)
        if (params.ReplayComputeToo)
        {
            auto& comp = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Compute)];

            if (fns.CmdBindDescriptorSets)
            {
                for (uint32_t i = 0; i < kMaxDescriptorSets; ++i)
                {
                    const auto& sb = comp.Sets[i];
                    if (!sb.Bound || sb.Set == VK_NULL_HANDLE)
                        continue;

                    VkPipelineLayout layoutToUse = sb.BoundWithLayout;
                    if (!layoutToUse)
                        continue;

                    fns.CmdBindDescriptorSets(dstCmd, VK_PIPELINE_BIND_POINT_COMPUTE, layoutToUse, i, 1, &sb.Set,
                                              (uint32_t) sb.DynamicOffsets.size(), sb.DynamicOffsets.data());
                }
            }
        }

        // 3. Push Constants - Graphics
        if (params.ReplayPushConstants)
        {
            auto& gfx = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Graphics)];

            for (const auto& entry : gfx.PushConstantHistory)
            {
                VkPipelineLayout layoutToUse =
                    params.OverrideGraphicsLayout ? params.OverrideGraphicsLayout : entry.Layout;

                if (layoutToUse && entry.Size > 0 && fns.CmdPushConstants)
                {
                    fns.CmdPushConstants(dstCmd, layoutToUse, entry.Stages, entry.Offset, entry.Size,
                                         &entry.Data[entry.Offset]);
                }
            }
        }

        // 3.5. Push Constants - Compute (if requested)
        if (params.ReplayComputeToo && params.ReplayPushConstants)
        {
            auto& comp = snapshot->BP[static_cast<uint32_t>(BindPointIndex::Compute)];

            for (const auto& entry : comp.PushConstantHistory)
            {
                if (entry.Layout && entry.Size > 0 && fns.CmdPushConstants)
                {
                    fns.CmdPushConstants(dstCmd, entry.Layout, entry.Stages, entry.Offset, entry.Size,
                                         &entry.Data[entry.Offset]);
                }
            }
        }

        // 4. Dynamic State
        if (params.ReplayViewportScissor)
        {
            ReplayViewports(fns, dstCmd, snapshot->Dyn);
            ReplayScissors(fns, dstCmd, snapshot->Dyn);
        }

        // 4.5. Extended Dynamic State
        if (params.ReplayExtendedDynamicState)
        {
            ReplayExtendedDynamicState(fns, dstCmd, snapshot->Dyn);
        }

        // 5. Vertex/Index
        if (params.ReplayVertexIndex)
        {
            ReplayVertexBuffers(fns, dstCmd, snapshot->VI);
            if (snapshot->VI.IndexBufferValid && fns.CmdBindIndexBuffer)
            {
                fns.CmdBindIndexBuffer(dstCmd, snapshot->VI.IndexBuffer, snapshot->VI.IndexOffset,
                                       snapshot->VI.IndexType);
            }
        }

        return true;
    }

    mutable std::mutex _mtx;
    std::unordered_map<VkCommandBuffer, std::shared_ptr<CommandBufferState>> _states;

    VulkanCmdFns _cachedFns {};
    bool _hasCachedFns = false;
};
} // namespace vk_state
