#include "PrefixSumKernel.h"
#include <bit>
#include <cassert>
#include "Utils.h"

const char* prefixSumSource = R"(
@group(0) @binding(0) var<storage, read_write> items: array<u32>;
@group(0) @binding(1) var<storage, read_write> blockSums: array<u32>;

override WORKGROUP_SIZE_X: u32;
override WORKGROUP_SIZE_Y: u32;
override THREADS_PER_WORKGROUP: u32;
override ITEMS_PER_WORKGROUP: u32;
override ELEMENT_COUNT: u32;

var<workgroup> temp: array<u32, ITEMS_PER_WORKGROUP*2>;

@compute @workgroup_size(WORKGROUP_SIZE_X, WORKGROUP_SIZE_Y, 1)
fn reduce_downsweep(
    @builtin(workgroup_id) w_id: vec3<u32>,
    @builtin(num_workgroups) w_dim: vec3<u32>,
    @builtin(local_invocation_index) TID: u32, // Local thread ID
) {
    let WORKGROUP_ID = w_id.x + w_id.y * w_dim.x;
    let WID = WORKGROUP_ID * THREADS_PER_WORKGROUP;
    let GID = WID + TID; // Global thread ID
    
    let ELM_TID = TID * 2; // Element pair local ID
    let ELM_GID = GID * 2; // Element pair global ID
    
    // Load input to shared memory
    temp[ELM_TID]     = select(items[ELM_GID], 0, ELM_GID >= ELEMENT_COUNT);
    temp[ELM_TID + 1] = select(items[ELM_GID + 1], 0, ELM_GID + 1 >= ELEMENT_COUNT);

    var offset: u32 = 1;

    // Up-sweep (reduce) phase
    for (var d: u32 = ITEMS_PER_WORKGROUP >> 1; d > 0; d >>= 1) {
        workgroupBarrier();

        if (TID < d) {
            var ai: u32 = offset * (ELM_TID + 1) - 1;
            var bi: u32 = offset * (ELM_TID + 2) - 1;
            temp[bi] += temp[ai];
        }

        offset *= 2;
    }

    // Save workgroup sum and clear last element
    if (TID == 0) {
        let last_offset = ITEMS_PER_WORKGROUP - 1;

        blockSums[WORKGROUP_ID] = temp[last_offset];
        temp[last_offset] = 0;
    }

    // Down-sweep phase
    for (var d: u32 = 1; d < ITEMS_PER_WORKGROUP; d *= 2) {
        offset >>= 1;
        workgroupBarrier();

        if (TID < d) {
            var ai: u32 = offset * (ELM_TID + 1) - 1;
            var bi: u32 = offset * (ELM_TID + 2) - 1;

            let t: u32 = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] += t;
        }
    }
    workgroupBarrier();

    // Copy result from shared memory to global memory
    if (ELM_GID >= ELEMENT_COUNT) {
        return;
    }
    items[ELM_GID] = temp[ELM_TID];

    if (ELM_GID + 1 >= ELEMENT_COUNT) {
        return;
    }
    items[ELM_GID + 1] = temp[ELM_TID + 1];
}

@compute @workgroup_size(WORKGROUP_SIZE_X, WORKGROUP_SIZE_Y, 1)
fn add_block_sums(
    @builtin(workgroup_id) w_id: vec3<u32>,
    @builtin(num_workgroups) w_dim: vec3<u32>,
    @builtin(local_invocation_index) TID: u32, // Local thread ID
) {
    let WORKGROUP_ID = w_id.x + w_id.y * w_dim.x;
    let WID = WORKGROUP_ID * THREADS_PER_WORKGROUP;
    let GID = WID + TID; // Global thread ID

    let ELM_ID = GID * 2;

    if (ELM_ID >= ELEMENT_COUNT) {
        return;
    }

    let blockSum = blockSums[WORKGROUP_ID];

    items[ELM_ID] += blockSum;

    if (ELM_ID + 1 >= ELEMENT_COUNT) {
        return;
    }

    items[ELM_ID + 1] += blockSum;
}
)";

const char* prefixSumSource_NoBankConflict = R"(
@group(0) @binding(0) var<storage, read_write> items: array<u32>;
@group(0) @binding(1) var<storage, read_write> blockSums: array<u32>;

override WORKGROUP_SIZE_X: u32;
override WORKGROUP_SIZE_Y: u32;
override THREADS_PER_WORKGROUP: u32;
override ITEMS_PER_WORKGROUP: u32;
override ELEMENT_COUNT: u32;

const NUM_BANKS: u32 = 32;
const LOG_NUM_BANKS: u32 = 5;

fn get_offset(offset: u32) -> u32 {
    // return offset >> LOG_NUM_BANKS; // Conflict-free
    return (offset >> NUM_BANKS) + (offset >> (2 * LOG_NUM_BANKS)); // Zero bank conflict
}

var<workgroup> temp: array<u32, ITEMS_PER_WORKGROUP*2>;

@compute @workgroup_size(WORKGROUP_SIZE_X, WORKGROUP_SIZE_Y, 1)
fn reduce_downsweep(
    @builtin(workgroup_id) w_id: vec3<u32>,
    @builtin(num_workgroups) w_dim: vec3<u32>,
    @builtin(local_invocation_index) TID: u32, // Local thread ID
) {
    let WORKGROUP_ID = w_id.x + w_id.y * w_dim.x;
    let WID = WORKGROUP_ID * THREADS_PER_WORKGROUP;
    let GID = WID + TID; // Global thread ID
    
    let ELM_TID = TID * 2; // Element pair local ID
    let ELM_GID = GID * 2; // Element pair global ID
    
    // Load input to shared memory
    let ai: u32 = TID;
    let bi: u32 = TID + (ITEMS_PER_WORKGROUP >> 1);
    let s_ai = ai + get_offset(ai);
    let s_bi = bi + get_offset(bi);
    let g_ai = ai + WID * 2;
    let g_bi = bi + WID * 2;
    temp[s_ai] = select(items[g_ai], 0, g_ai >= ELEMENT_COUNT);
    temp[s_bi] = select(items[g_bi], 0, g_bi >= ELEMENT_COUNT);

    var offset: u32 = 1;

    // Up-sweep (reduce) phase
    for (var d: u32 = ITEMS_PER_WORKGROUP >> 1; d > 0; d >>= 1) {
        workgroupBarrier();

        if (TID < d) {
            var ai: u32 = offset * (ELM_TID + 1) - 1;
            var bi: u32 = offset * (ELM_TID + 2) - 1;
            ai += get_offset(ai);
            bi += get_offset(bi);
            temp[bi] += temp[ai];
        }

        offset *= 2;
    }

    // Save workgroup sum and clear last element
    if (TID == 0) {
        var last_offset = ITEMS_PER_WORKGROUP - 1;
        last_offset += get_offset(last_offset);

        blockSums[WORKGROUP_ID] = temp[last_offset];
        temp[last_offset] = 0;
    }

    // Down-sweep phase
    for (var d: u32 = 1; d < ITEMS_PER_WORKGROUP; d *= 2) {
        offset >>= 1;
        workgroupBarrier();

        if (TID < d) {
            var ai: u32 = offset * (ELM_TID + 1) - 1;
            var bi: u32 = offset * (ELM_TID + 2) - 1;
            ai += get_offset(ai);
            bi += get_offset(bi);

            let t: u32 = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] += t;
        }
    }
    workgroupBarrier();

    // Copy result from shared memory to global memory
    if (g_ai < ELEMENT_COUNT) {
        items[g_ai] = temp[s_ai];
    }
    if (g_bi < ELEMENT_COUNT) {
        items[g_bi] = temp[s_bi];
    }
}

@compute @workgroup_size(WORKGROUP_SIZE_X, WORKGROUP_SIZE_Y, 1)
fn add_block_sums(
    @builtin(workgroup_id) w_id: vec3<u32>,
    @builtin(num_workgroups) w_dim: vec3<u32>,
    @builtin(local_invocation_index) TID: u32, // Local thread ID
) {
    let WORKGROUP_ID = w_id.x + w_id.y * w_dim.x;
    let WID = WORKGROUP_ID * THREADS_PER_WORKGROUP;
    let GID = WID + TID; // Global thread ID

    let ELM_ID = GID * 2;

    if (ELM_ID >= ELEMENT_COUNT) {
        return;
    }

    let blockSum = blockSums[WORKGROUP_ID];

    items[ELM_ID] += blockSum;

    if (ELM_ID + 1 >= ELEMENT_COUNT) {
        return;
    }

    items[ELM_ID + 1] += blockSum;
}
)";

PrefixSumKernel::PrefixSumKernel(wgpu::Device device,
                                 wgpu::Buffer data,
                                 int count,
                                 std::pair<int, int> workgroupSize,
                                 bool avoidBankConflicts)
{
    mDevice                = device;
    mPreviousAvoidConflict = avoidBankConflicts;
    LoadShader(avoidBankConflicts);
    Reset(data, count, workgroupSize, avoidBankConflicts);
}

void PrefixSumKernel::Dispatch(wgpu::ComputePassEncoder& pass,
                               wgpu::Buffer dispatchSizeBuffer,
                               int offset)
{
    for (int i = 0; i < mPipelines.size(); ++i)
    {
        auto& pipeline = mPipelines[i];

        pass.SetPipeline(pipeline.pipeline);
        pass.SetBindGroup(0, pipeline.bindgroup, 0, nullptr);

        if (dispatchSizeBuffer == nullptr)
        {
            pass.DispatchWorkgroups(pipeline.dispatchSize.first, pipeline.dispatchSize.second, 1);
        }
        else
        {
            pass.DispatchWorkgroupsIndirect(dispatchSizeBuffer, offset + i * 3 * 4);
        }
    }
}

void PrefixSumKernel::Reset(wgpu::Buffer data,
                            int count,
                            std::pair<int, int> workgroupSize,
                            bool avoidBankConflicts)
{
    mPipelines.clear();

    mWorkGroupSize       = workgroupSize;
    mThreadsPerWorkgroup = mWorkGroupSize.first * mWorkGroupSize.second;
    mItemsPerWorkgroup   = 2 * mThreadsPerWorkgroup;

    assert(std::has_single_bit((unsigned int)mThreadsPerWorkgroup));

    if (mPreviousAvoidConflict != avoidBankConflicts)
    {
        LoadShader(avoidBankConflicts);
    }

    CreatePassRecursive(data, count);
}

void PrefixSumKernel::CreatePassRecursive(wgpu::Buffer data, int count)
{
    // Find best dispatch x and y dimensions to minimize unused threads
    int workgroupCount = (int)std::ceil(count / (float)mItemsPerWorkgroup);
    auto dispatchSize  = RadixSortUtils::FindOptimalDispatchSize(mDevice, workgroupCount);

    // Create buffer for block sums
    wgpu::BufferDescriptor bufferDesc {
        .label = wgpu::StringView("prefix-sum-block-sum"),
        .size  = (uint64_t)(workgroupCount * 4),
        .usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst,
    };
    wgpu::Buffer blockSumBuffer = mDevice.CreateBuffer(&bufferDesc);

    // Create bind group and pipeline layout
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(2);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout0 = bindingLayoutEentries[0];
    RadixSortUtils::SetDefaultBindGroupLayout(bindingLayout0);
    bindingLayout0.binding     = 0;
    bindingLayout0.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout0.buffer.type = wgpu::BufferBindingType::Storage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout1 = bindingLayoutEentries[1];
    RadixSortUtils::SetDefaultBindGroupLayout(bindingLayout1);
    bindingLayout1.binding     = 1;
    bindingLayout1.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout1.buffer.type = wgpu::BufferBindingType::Storage;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    auto bindGroupLayout           = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // bind group
    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = data;
    bindings[0].offset  = 0;
    bindings[0].size    = data.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = blockSumBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = blockSumBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = wgpu::StringView("prefix-sum-bind-group"),
        .layout     = bindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    auto bindGroup = mDevice.CreateBindGroup(&bindGroupDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &bindGroupLayout;
    auto pipelineLayout             = mDevice.CreatePipelineLayout(&layoutDesc);

    // Constants
    std::vector<wgpu::ConstantEntry> constantsEntries(5);
    constantsEntries[0].key   = wgpu::StringView("WORKGROUP_SIZE_X");
    constantsEntries[0].value = mWorkGroupSize.first;
    constantsEntries[1].key   = wgpu::StringView("WORKGROUP_SIZE_Y");
    constantsEntries[1].value = mWorkGroupSize.second;
    constantsEntries[2].key   = wgpu::StringView("THREADS_PER_WORKGROUP");
    constantsEntries[2].value = mThreadsPerWorkgroup;
    constantsEntries[3].key   = wgpu::StringView("ITEMS_PER_WORKGROUP");
    constantsEntries[3].value = mItemsPerWorkgroup;
    constantsEntries[4].key   = wgpu::StringView("ELEMENT_COUNT");
    constantsEntries[4].value = count;

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = wgpu::StringView("prefix-sum-bind-group"),
        .layout = pipelineLayout,
        .compute =
            {
                .module        = mShaderModule,
                .entryPoint    = "reduce_downsweep",
                .constantCount = static_cast<size_t>(constantsEntries.size()),
                .constants     = constantsEntries.data(),
            },
    };

    // Per-workgroup (block) prefix sum
    auto scanPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);

    mPipelines.emplace_back(scanPipeline, bindGroup, dispatchSize);

    if (workgroupCount > 1)
    {
        // Prefix sum on block sums
        CreatePassRecursive(blockSumBuffer, workgroupCount);

        // Constants
        std::vector<wgpu::ConstantEntry> blockSumConstantsEntries(4);
        constantsEntries[0].key   = wgpu::StringView("WORKGROUP_SIZE_X");
        constantsEntries[0].value = mWorkGroupSize.first;
        constantsEntries[1].key   = wgpu::StringView("WORKGROUP_SIZE_Y");
        constantsEntries[1].value = mWorkGroupSize.second;
        constantsEntries[2].key   = wgpu::StringView("THREADS_PER_WORKGROUP");
        constantsEntries[2].value = mThreadsPerWorkgroup;
        constantsEntries[3].key   = wgpu::StringView("ELEMENT_COUNT");
        constantsEntries[3].value = count;

        // Add block sums to local prefix sums
        wgpu::ComputePipelineDescriptor blockSumPipelineDesc {
            .label  = wgpu::StringView("prefix-sum-add-block-pipeline"),
            .layout = pipelineLayout,
            .compute =
                {
                    .module        = mShaderModule,
                    .entryPoint    = "add_block_sums",
                    .constantCount = static_cast<size_t>(blockSumConstantsEntries.size()),
                    .constants     = blockSumConstantsEntries.data(),
                },
        };
        auto blockSumPipeline = mDevice.CreateComputePipeline(&blockSumPipelineDesc);

        mPipelines.emplace_back(blockSumPipeline, bindGroup, dispatchSize);
    }
}

void PrefixSumKernel::LoadShader(bool avoidBankConflicts)
{
    wgpu::ShaderSourceWGSL shaderCodeDesc {};
    shaderCodeDesc.nextInChain = nullptr;
    shaderCodeDesc.sType       = wgpu::SType::ShaderSourceWGSL;
    shaderCodeDesc.code = avoidBankConflicts ? wgpu::StringView(prefixSumSource_NoBankConflict)
                                             : wgpu::StringView(prefixSumSource);

    wgpu::ShaderModuleDescriptor shaderDesc {
        .label       = wgpu::StringView("prefix-sum"),
        .nextInChain = &shaderCodeDesc,
    };

    mShaderModule = mDevice.CreateShaderModule(&shaderDesc);
}
