#include <windows.h>
#include <stdint.h>
#include <float.h>
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vulkan/vulkan_core.h"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_win32.h"
#include "./Renderer.h"
#include "./Renderer.cpp"
//Validation Layer stuff lifted almost straight from Vulkan Tutorial
VkInstance gInstance;
#include "./ValidationLayers.h"

static b32 GlobalRunning;
static b32 GlobalMinimized;
static VulkanData GlobalVulkanData;

//static win32_image_buffer GlobalBackBuffer;
#define APPLICATION_NAME "Vulkan Example"
#define ENGINE_NAME "Vulkan Engine"
void ErrorMessageBox(const char *ErrorMessage)
{
        GlobalRunning=false;
        MessageBox(NULL, ErrorMessage,
                ENGINE_NAME, MB_ICONERROR);
        while(true)
        {

        }
}
File win32ReadEntireFile(char *Filename)
{
    File Result={};
    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ,
            0, OPEN_EXISTING,0,0);
    if(FileHandle !=INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            u32 FileSize32 = (u64) FileSize.QuadPart>0xffffffff?0xffffffff:(u32)FileSize.QuadPart;
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if(Result.Contents)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && 
                        (FileSize32==BytesRead))
                {
                    Result.FileSize=FileSize32;
                }
                else
                {
                    VirtualFree(Result.Contents,0,MEM_RELEASE);
                    Result.Contents=0;
                }

            }
        } 

        CloseHandle(FileHandle);
    }

    return Result;
}


VkImageView CreateImageView(VkDevice LogicalDevice, VkImage Image, 
                            VkFormat Format, VkImageAspectFlags AspectFlags) {
    VkImageViewCreateInfo ViewInfo{};
    ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ViewInfo.image = Image;
    ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewInfo.format = Format;
    ViewInfo.subresourceRange.aspectMask = AspectFlags;
    ViewInfo.subresourceRange.baseMipLevel = 0;
    ViewInfo.subresourceRange.levelCount = 1;
    ViewInfo.subresourceRange.baseArrayLayer = 0;
    ViewInfo.subresourceRange.layerCount = 1;

    VkImageView ImageView;
    if (vkCreateImageView(LogicalDevice, &ViewInfo, NULL, &ImageView) != VK_SUCCESS) {
        ErrorMessageBox("failed to create texture image view!");
    }

    return ImageView;
}

u32 FindMemoryType(u32 TypeFilter, VkMemoryPropertyFlags Properties) {
    VkPhysicalDeviceMemoryProperties MemProperties;
    VkPhysicalDevice PhysicalDevice = GlobalVulkanData.PhysicalDevice;
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemProperties);

    for (u32  MemTypeIndex= 0; MemTypeIndex < MemProperties.memoryTypeCount; MemTypeIndex++) 
    {
        if ((TypeFilter & (1 << MemTypeIndex)) 
                && (MemProperties.memoryTypes[MemTypeIndex].propertyFlags & Properties) == Properties) 
        {
            return MemTypeIndex;
        }
    }
    ErrorMessageBox( "Failed to find suitable memory type!");
    return -1;

}

void CreateVulkanBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Properties,
        VkBuffer *Buffer, VkDeviceMemory *BufferMemory)
{
    VkDevice LogicalDevice = GlobalVulkanData.LogicalDevice;
    VkBufferCreateInfo BufferInfo{};

    BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = Size; 
    BufferInfo.usage = Usage;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(LogicalDevice, &BufferInfo, NULL, Buffer) != VK_SUCCESS)
    {
        ErrorMessageBox("Failed to create Vertex Buffer!");
    }
    VkMemoryRequirements MemRequirements;
    vkGetBufferMemoryRequirements(LogicalDevice, *Buffer, &MemRequirements);

    VkMemoryAllocateInfo BufferAllocInfo{};
    BufferAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    BufferAllocInfo.allocationSize = MemRequirements.size;
    BufferAllocInfo.memoryTypeIndex = 
        FindMemoryType(MemRequirements.memoryTypeBits, 
                Properties);

    if (vkAllocateMemory(LogicalDevice, &BufferAllocInfo, NULL, BufferMemory) != VK_SUCCESS) 
    {
        ErrorMessageBox("Failed to allocate Vertex Buffer memory!");
    }

    vkBindBufferMemory(LogicalDevice, *Buffer, *BufferMemory, 0);
}

VkCommandBuffer BeginSingleTimeCommands() {
    VkDevice LogicalDevice =GlobalVulkanData.LogicalDevice;
    VkCommandPool CommandPool=GlobalVulkanData.CommandPool;

    VkCommandBufferAllocateInfo AllocInfo{};
    AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocInfo.commandPool = CommandPool;
    AllocInfo.commandBufferCount = 1;

    VkCommandBuffer CommandBuffer;
    vkAllocateCommandBuffers(LogicalDevice, &AllocInfo, &CommandBuffer);

    VkCommandBufferBeginInfo BeginInfo{};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(CommandBuffer, &BeginInfo);

    return CommandBuffer;
}

void EndSingleTimeCommands(VkCommandBuffer CommandBuffer) {
    VkDevice LogicalDevice =GlobalVulkanData.LogicalDevice;
    VkQueue GraphicsQueue = GlobalVulkanData.GraphicsQueue;
    VkCommandPool CommandPool=GlobalVulkanData.CommandPool;

    vkEndCommandBuffer(CommandBuffer);

    VkSubmitInfo SubmitInfo{};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CommandBuffer;

    vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(GraphicsQueue);

    vkFreeCommandBuffers(LogicalDevice, CommandPool, 1, &CommandBuffer);
}

void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize Size) 
{
    VkCommandBuffer CommandBuffer = BeginSingleTimeCommands();

    VkBufferCopy CopyRegion{};
    CopyRegion.size = Size;
    vkCmdCopyBuffer(CommandBuffer, srcBuffer, dstBuffer, 1, &CopyRegion);

    EndSingleTimeCommands(CommandBuffer);
}

void CreateVulkanImage(VkDevice LogicalDevice, u32 Width, u32 Height, VkFormat Format, VkImageTiling Tiling, 
        VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties, VkImage& Image, VkDeviceMemory& ImageMemory) {
    VkImageCreateInfo ImageInfo{};
    ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.extent.width = Width;
    ImageInfo.extent.height = Height;
    ImageInfo.extent.depth = 1;
    ImageInfo.mipLevels = 1;
    ImageInfo.arrayLayers = 1;
    ImageInfo.format = Format;
    ImageInfo.tiling = Tiling;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageInfo.usage = Usage;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(LogicalDevice, &ImageInfo, NULL, &Image) != VK_SUCCESS) {
        ErrorMessageBox("Failed to create Image!");
    }

    VkMemoryRequirements MemRequirements;
    vkGetImageMemoryRequirements(LogicalDevice, Image, &MemRequirements);

    VkMemoryAllocateInfo AllocInfo{};
    AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize = MemRequirements.size;
    AllocInfo.memoryTypeIndex = FindMemoryType(MemRequirements.memoryTypeBits, Properties);

    if (vkAllocateMemory(LogicalDevice, &AllocInfo, NULL, &ImageMemory) != VK_SUCCESS) {
        ErrorMessageBox("Failed to Allocate Image Memory!");
    }

    vkBindImageMemory(LogicalDevice, Image, ImageMemory, 0);
}

void TransitionImageLayout(VkImage Image, VkFormat Format, VkImageLayout OldLayout, VkImageLayout NewLayout) {
    VkCommandBuffer CommandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier Barrier{};
    Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    Barrier.oldLayout = OldLayout;
    Barrier.newLayout = NewLayout;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image = Image;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseMipLevel = 0;
    Barrier.subresourceRange.levelCount = 1;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags SourceStage;
    VkPipelineStageFlags DestinationStage;

    if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED && NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        Barrier.srcAccessMask = 0;
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } 
    else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } 
    else {
        ErrorMessageBox("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(CommandBuffer, SourceStage, DestinationStage,0,0, NULL,
                        0, NULL,1, &Barrier);

    EndSingleTimeCommands(CommandBuffer);
}

void CopyBufferToImage(VkBuffer Buffer, VkImage Image, u32 Width, u32 Height) {
    VkCommandBuffer CommandBuffer = BeginSingleTimeCommands();

    VkBufferImageCopy Region{};
    Region.bufferOffset = 0;
    Region.bufferRowLength = 0;
    Region.bufferImageHeight = 0;
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.mipLevel = 0;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount = 1;
    Region.imageOffset = {0, 0, 0};
    Region.imageExtent = {Width,Height,1};

    vkCmdCopyBufferToImage(CommandBuffer, Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    EndSingleTimeCommands(CommandBuffer);
}

void CreateTextureImage(VkDevice LogicalDevice) {
    i32 texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("../data/textures/owl.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize ImageSize = texWidth * texHeight * 4;

    if (!pixels) {
        ErrorMessageBox("Failed to load texture!");
    }

    VkBuffer StagingBuffer;
    VkDeviceMemory StagingBufferMemory;
    CreateVulkanBuffer(ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    &StagingBuffer, &StagingBufferMemory);

    void* Data;
    vkMapMemory(LogicalDevice, StagingBufferMemory, 0, ImageSize, 0, &Data); 
    memcpy(Data, pixels, (size_t)(ImageSize));
    vkUnmapMemory(LogicalDevice, StagingBufferMemory);

    stbi_image_free(pixels);

    CreateVulkanImage(LogicalDevice, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, GlobalVulkanData.TextureImage, GlobalVulkanData.TextureImageMemory);

    TransitionImageLayout(GlobalVulkanData.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, 
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(StagingBuffer, GlobalVulkanData.TextureImage, (u32)(texWidth), (u32)(texHeight));
    TransitionImageLayout(GlobalVulkanData.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, 
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(LogicalDevice, StagingBuffer, NULL);
    vkFreeMemory(LogicalDevice, StagingBufferMemory, NULL);

    //Create Texture Image View
    GlobalVulkanData.TextureImageView=CreateImageView(LogicalDevice, GlobalVulkanData.TextureImage,VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    //Create Texture image Sampler
    VkPhysicalDeviceProperties Properties{};
    vkGetPhysicalDeviceProperties(GlobalVulkanData.PhysicalDevice, &Properties);

    VkSamplerCreateInfo SamplerInfo{};
    SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    //texture filtering params
    SamplerInfo.magFilter = VK_FILTER_LINEAR;
    SamplerInfo.minFilter = VK_FILTER_LINEAR;

    //out of bound addressing
    SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.anisotropyEnable = VK_TRUE;
    SamplerInfo.maxAnisotropy = Properties.limits.maxSamplerAnisotropy;
    SamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    SamplerInfo.unnormalizedCoordinates = VK_FALSE;
    SamplerInfo.compareEnable = VK_FALSE;
    SamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler TextureSampler;
    if (vkCreateSampler(LogicalDevice, &SamplerInfo, NULL, &TextureSampler) != VK_SUCCESS) {
        ErrorMessageBox("failed to create texture sampler!");
    }
    GlobalVulkanData.TextureSampler=TextureSampler;
}


void CreateDepthResources() {
    //Note(Alex): we should do something more sophisticated to select the depth format
    //for example check wether we need a stencil component
    VkFormat DepthFormat=VK_FORMAT_D32_SFLOAT;
    CreateVulkanImage(GlobalVulkanData.LogicalDevice,
                      GlobalVulkanData.Extent.width, GlobalVulkanData.Extent.height, 
                      DepthFormat, VK_IMAGE_TILING_OPTIMAL, 
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                      GlobalVulkanData.DepthImage, GlobalVulkanData.DepthImageMemory);
    GlobalVulkanData.DepthImageView=CreateImageView(GlobalVulkanData.LogicalDevice,
                                                    GlobalVulkanData.DepthImage,
                                                    DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

}

void UpdateUniformBuffer(u32 CurrentImage)
{
    VkDevice LogicalDevice = GlobalVulkanData.LogicalDevice;
    VkDeviceMemory UniformBufferMemory = GlobalVulkanData.UniformBuffersMemory[CurrentImage];
    VkExtent2D Extent = GlobalVulkanData.Extent;
    static f32 angle=0.0f;
    UniformBufferObject ubo={};

    ubo.Model = ZRotationMatrix(0);
    v4 CameraPos={0.0f, 0.0f, 2.0f,1.0f};
    mat4 Camera =XRotationMatrix(-PI32/4.0f,CameraPos);
    /*
    ubo.View= InverseRotationAndTranslationMatrix(&Camera);
    ubo.View.W=CameraPos;
    */
    ubo.View=Camera;

    ubo.Proj=ProjectionMatrix(PI32/2.0f, Extent.width, Extent.height);
    //v4 test={1.0f,1.0f,0.0f,1.0f};
    //test=ubo.Proj*ubo.View*ubo.Model*test;
    //std::cout<<test.x<<" "<<test.y<<" "<<test.z<<" "<<test.w<<std::endl;
    void* Data;
    vkMapMemory(LogicalDevice, UniformBufferMemory, 0, sizeof(ubo), 0, &Data);
    memcpy(Data, &ubo, sizeof(ubo));
    vkUnmapMemory(LogicalDevice, UniformBufferMemory);
    angle+=0.0001f;
    if(angle>2.0f*PI32)
        angle-=2.0f*PI32;

}



void *VulkanMemAlloc(u32 Size)
{
    void *Result = GlobalVulkanData.Free;
    GlobalVulkanData.Free=(u8 *)GlobalVulkanData.Free+Size;

    return Result;
}

void CreateDescriptorSetLayout(VkDevice LogicalDevice)
{
    /*Create Descriptor Set Layout*/
    //uniform buffer object Descriptor set layout
    VkDescriptorSetLayout DescriptorSetLayout;
    VkDescriptorSetLayoutBinding UboLayoutBinding{};
    UboLayoutBinding.binding = 0;
    UboLayoutBinding.descriptorCount = 1;
    UboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    UboLayoutBinding.pImmutableSamplers = NULL;
    UboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    //Sampler Descriptor Set Layout
    //appended to the end
    VkDescriptorSetLayoutBinding SamplerLayoutBinding{};
    SamplerLayoutBinding.binding = 1;
    SamplerLayoutBinding.descriptorCount = 1;
    SamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SamplerLayoutBinding.pImmutableSamplers = NULL;
    SamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding Bindings[2] ={UboLayoutBinding, SamplerLayoutBinding}; 
    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutInfo{};
    DescriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetLayoutInfo.bindingCount = ArrayCount(Bindings); 
    DescriptorSetLayoutInfo.pBindings = Bindings;

    if (vkCreateDescriptorSetLayout(LogicalDevice, &DescriptorSetLayoutInfo, 
                NULL, &DescriptorSetLayout) != VK_SUCCESS)
    {

        ErrorMessageBox("Failed to Create Descriptor Set Layout!");
    }                                   

    GlobalVulkanData.DescriptorSetLayout=DescriptorSetLayout;
}

void CreateDescriptorSet(VkDevice LogicalDevice)
{
    VkDescriptorSetLayout DescriptorSetLayout =GlobalVulkanData.DescriptorSetLayout;
    /*Create Descriptor Pool*/
    VkDescriptorPool DescriptorPool;
    VkDescriptorPoolSize PoolSizes[2]={};
    PoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    PoolSizes[0].descriptorCount = (MAX_FRAMES_IN_FLIGHT);
    PoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    PoolSizes[1].descriptorCount = (MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo PoolInfo{};
    PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolInfo.poolSizeCount = ArrayCount(PoolSizes);
    PoolInfo.pPoolSizes = PoolSizes;
    PoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(LogicalDevice, &PoolInfo, NULL, &DescriptorPool) != VK_SUCCESS) 
    {
        ErrorMessageBox("Failed to create Descriptor Pool!");
    }

    /*Create Descriptor Sets*/
    VkDescriptorSetLayout Layouts[MAX_FRAMES_IN_FLIGHT]={0}; 
    for(u32 LayoutIndex=0;LayoutIndex<MAX_FRAMES_IN_FLIGHT; ++LayoutIndex)
    {
        Layouts[LayoutIndex]=DescriptorSetLayout;
    }
    VkDescriptorSetAllocateInfo DescriptorSetAllocInfo{};
    DescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocInfo.descriptorPool = DescriptorPool;
    DescriptorSetAllocInfo.descriptorSetCount = (MAX_FRAMES_IN_FLIGHT);
    DescriptorSetAllocInfo.pSetLayouts = Layouts;

    VkDescriptorSet DescriptorSets[MAX_FRAMES_IN_FLIGHT]={0};

    if (vkAllocateDescriptorSets(LogicalDevice, &DescriptorSetAllocInfo, DescriptorSets) != VK_SUCCESS) 
    {
        ErrorMessageBox("Failed to allocate Descriptor Sets!");
    }

    for (u32 FrameIndex = 0; FrameIndex < MAX_FRAMES_IN_FLIGHT; FrameIndex++) 
    {
        VkDescriptorBufferInfo DescriptorBufferInfo{};
        DescriptorBufferInfo.buffer = GlobalVulkanData.UniformBuffers[FrameIndex];
        DescriptorBufferInfo.offset = 0;
        DescriptorBufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo ImageInfo{};
        ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ImageInfo.imageView = GlobalVulkanData.TextureImageView;
        ImageInfo.sampler = GlobalVulkanData.TextureSampler;

        VkWriteDescriptorSet DescriptorWrites[2]={};

        DescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWrites[0].dstSet = DescriptorSets[FrameIndex];
        DescriptorWrites[0].dstBinding = 0;
        DescriptorWrites[0].dstArrayElement = 0;
        DescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        DescriptorWrites[0].descriptorCount = 1;
        DescriptorWrites[0].pBufferInfo = &DescriptorBufferInfo;

        DescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWrites[1].dstSet = DescriptorSets[FrameIndex];
        DescriptorWrites[1].dstBinding = 1;
        DescriptorWrites[1].dstArrayElement = 0;
        DescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        DescriptorWrites[1].descriptorCount = 1;
        DescriptorWrites[1].pImageInfo = &ImageInfo;

        vkUpdateDescriptorSets(LogicalDevice, ArrayCount(DescriptorWrites), DescriptorWrites, 0, NULL);

        GlobalVulkanData.DescriptorSets[FrameIndex]=DescriptorSets[FrameIndex];
    }
}

void CreateUniformBuffers()
{
    /*Create Uniform Buffers*/
    VkDeviceSize BufferSize = sizeof(UniformBufferObject);

    VkBuffer UniformBuffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory UniformBuffersMemory[MAX_FRAMES_IN_FLIGHT];

    for (u32 UniformBufferIndex= 0; UniformBufferIndex < MAX_FRAMES_IN_FLIGHT; UniformBufferIndex++) 
    {
        CreateVulkanBuffer(BufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &UniformBuffers[UniformBufferIndex], &UniformBuffersMemory[UniformBufferIndex]);

        GlobalVulkanData.UniformBuffers[UniformBufferIndex] = UniformBuffers[UniformBufferIndex];
        GlobalVulkanData.UniformBuffersMemory[UniformBufferIndex] = UniformBuffersMemory[UniformBufferIndex];
    } 
}

void win32CreateSwapchain(HWND Window)
{
    u32 TotalSize =((u8 *)GlobalVulkanData.SwapchainMemory - (u8 *)GlobalVulkanData.Memory); 
    u32 MemSize = GlobalVulkanData.MemSize;

    u32 GraphicsQueueIndex = GlobalVulkanData.GraphicsQueueIndex;
    u32 PresentQueueIndex = GlobalVulkanData.PresentQueueIndex;
    u32 QueueIndices[] = {GraphicsQueueIndex, PresentQueueIndex}; 
    VkPhysicalDevice PhysicalDevice=GlobalVulkanData.PhysicalDevice;
    VkDevice LogicalDevice=GlobalVulkanData.LogicalDevice ;
    VkSurfaceKHR Surface=GlobalVulkanData.Surface  ;
    VkQueue GraphicsQueue=GlobalVulkanData.GraphicsQueue  ;
    VkQueue PresentQueue=GlobalVulkanData.PresentQueue ;
    VkDescriptorSetLayout DescriptorSetLayout =GlobalVulkanData.DescriptorSetLayout;

    /*Check for swapchain capbilities*/
    VkSurfaceCapabilitiesKHR SwapchainCapabilities;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SwapchainCapabilities);

    u32 FormatCount=0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, NULL);

    TotalSize+=sizeof(VkSurfaceFormatKHR)*FormatCount;
    Assert(TotalSize<MemSize);
    VkSurfaceFormatKHR *SwapchainFormats=(VkSurfaceFormatKHR *) VulkanMemAlloc(sizeof(VkSurfaceFormatKHR)*FormatCount);

    if(FormatCount !=0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, SwapchainFormats);
    }
    u32 PresentModeCount=0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, NULL);

    TotalSize+=sizeof(VkPresentModeKHR)*PresentModeCount;
    Assert(TotalSize<MemSize);
    VkPresentModeKHR *SwapchainPresentModes = (VkPresentModeKHR *)VulkanMemAlloc(sizeof(VkPresentModeKHR)*PresentModeCount);

    if(PresentModeCount !=0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount,SwapchainPresentModes) ;
    }

    if(FormatCount==0 || PresentModeCount==0)
    {
        ErrorMessageBox("Swapchain not suitable");
    }
    /*Create Swapchain*/

    VkSurfaceFormatKHR SurfaceFormat=SwapchainFormats[0];
    for(u32 FormatIndex=0; FormatIndex<FormatCount;++FormatIndex)
    {
        if(SwapchainFormats[FormatIndex].format == VK_FORMAT_B8G8R8A8_SRGB && SwapchainFormats[FormatIndex].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            SurfaceFormat = SwapchainFormats[FormatIndex];
            break;
        }
    }
    VkPresentModeKHR PresentMode=VK_PRESENT_MODE_FIFO_KHR;
    for(u32 PresentModeIndex=0; PresentModeIndex<PresentModeCount; ++PresentModeIndex)
    {
        if(SwapchainPresentModes[PresentModeIndex] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            PresentMode=VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }


    VkExtent2D Extent;
    if(SwapchainCapabilities.currentExtent.width !=-1)
        Extent = SwapchainCapabilities.currentExtent;
    else
    {
        //THIS IS PLATFORM SPECIFIC CODE
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        Extent.width =Clamp(SwapchainCapabilities.minImageExtent.width,
                ClientRect.right - ClientRect.left,
                SwapchainCapabilities.maxImageExtent.width);
        Extent.height = Clamp(SwapchainCapabilities.minImageExtent.height,
                ClientRect.bottom - ClientRect.top,
                SwapchainCapabilities.maxImageExtent.height);
    }

    u32 ImageCount=SwapchainCapabilities.minImageCount +1;
    if(SwapchainCapabilities.maxImageCount > 0 && ImageCount > SwapchainCapabilities.maxImageCount)
    {
        ImageCount = SwapchainCapabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR SwapchainInfo={};
    SwapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    SwapchainInfo.surface = Surface;
    SwapchainInfo.minImageCount = ImageCount;
    SwapchainInfo.imageFormat = SurfaceFormat.format;
    SwapchainInfo.imageColorSpace = SurfaceFormat.colorSpace;
    SwapchainInfo.imageExtent = Extent;
    SwapchainInfo.imageArrayLayers = 1;
    SwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    //NOTE(Alex): We're using a single queue, in practice things may be different
    //and we might need to specify how the swap chain images will be handled across different queues
    if(GraphicsQueueIndex != PresentQueueIndex)
    {
        SwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        SwapchainInfo.queueFamilyIndexCount = 2; 
        SwapchainInfo.pQueueFamilyIndices = QueueIndices; 
    }
    else
    {
        SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        SwapchainInfo.queueFamilyIndexCount = 0; // Optional
        SwapchainInfo.pQueueFamilyIndices = NULL; // Optional
    }

    SwapchainInfo.preTransform = SwapchainCapabilities.currentTransform;
    SwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SwapchainInfo.presentMode = PresentMode;
    SwapchainInfo.clipped = VK_TRUE;
    SwapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR Swapchain;
    if(vkCreateSwapchainKHR(LogicalDevice, &SwapchainInfo, NULL, &Swapchain) != VK_SUCCESS)
        ErrorMessageBox("Couldn't create Swapchain!");



    vkGetSwapchainImagesKHR(LogicalDevice, Swapchain, &ImageCount, NULL);

    TotalSize+=sizeof(VkImage)*ImageCount;
    Assert(TotalSize<MemSize);
    VkImage *SwapchainImages=(VkImage *)VulkanMemAlloc(sizeof(VkImage)*ImageCount);

    vkGetSwapchainImagesKHR(LogicalDevice, Swapchain, &ImageCount, SwapchainImages);
    VkFormat SwapchainImageFormat = SurfaceFormat.format;

    /* Create ImageViews */
    TotalSize +=sizeof(VkImageView) *ImageCount;
    Assert(TotalSize<MemSize);
    VkImageView *SwapchainImageViews=(VkImageView *)VulkanMemAlloc(sizeof(VkImageView) *ImageCount);

    for(u32 ImageViewIndex=0; ImageViewIndex<ImageCount;++ImageViewIndex)
    {
        SwapchainImageViews[ImageViewIndex]=
            CreateImageView(LogicalDevice,SwapchainImages[ImageViewIndex],SwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT); 

    }

    /*Graphics Pipeline*/

    /*Loading Shaders*/
    File VertShaderFile= win32ReadEntireFile("../code/shaders/vert.spv");
    File FragShaderFile= win32ReadEntireFile("../code/shaders/frag.spv");
    if(!VertShaderFile.Contents || !FragShaderFile.Contents)
    {
        ErrorMessageBox("Couldn't open shader file!");
    }
    VkShaderModuleCreateInfo VertShaderModuleInfo={};
    VertShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VertShaderModuleInfo.codeSize = VertShaderFile.FileSize;
    VertShaderModuleInfo.pCode = (u32 *) VertShaderFile.Contents;

    VkShaderModule VertShaderModule;
    if(vkCreateShaderModule(LogicalDevice, &VertShaderModuleInfo, NULL, &VertShaderModule) != VK_SUCCESS)
    {
        ErrorMessageBox("Couldn't create vertex shader module!");
    }

    VkShaderModuleCreateInfo FragShaderModuleInfo={};
    FragShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragShaderModuleInfo.codeSize = FragShaderFile.FileSize;
    FragShaderModuleInfo.pCode = (u32 *) FragShaderFile.Contents;

    VkShaderModule FragShaderModule;
    if(vkCreateShaderModule(LogicalDevice, &FragShaderModuleInfo, NULL, &FragShaderModule) != VK_SUCCESS)
    {
        ErrorMessageBox("Couldn't create fragment shader module!");
    }

    VkPipelineShaderStageCreateInfo VertShaderStageInfo={};
    VertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    VertShaderStageInfo.module = VertShaderModule;
    VertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo FragShaderStageInfo={};
    FragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    FragShaderStageInfo.module = FragShaderModule;
    FragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo ShaderStages[] = {VertShaderStageInfo, FragShaderStageInfo};

    /*Fixed Funtions*/

    /*Vertex Input*/
    VkVertexInputBindingDescription BindingDescritption = GetBindingDescription();
    VkVertexInputAttributeDescription *AttributeDescriptions=
        GetAttributeDescription();

    VkPipelineVertexInputStateCreateInfo VertexInputInfo={};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 1;
    VertexInputInfo.pVertexBindingDescriptions = &BindingDescritption; 
    VertexInputInfo.vertexAttributeDescriptionCount = ATTRIBUTES_COUNT;
    VertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions; 

    /*Input Assembly*/
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo={};
    InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    InputAssemblyInfo.primitiveRestartEnable = VK_FALSE; 

    /*Viewport*/
    VkViewport Viewport{};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    //We use the swapchain extent
    Viewport.width = (f32) Extent.width;
    Viewport.height = (f32) Extent.height;
    //We're using a reverse z-mapping
    //https://developer.nvidia.com/content/depth-precision-visualized
    Viewport.minDepth = 1.0f;
    Viewport.maxDepth = 0.0f;

    VkRect2D Scissor{};
    Scissor.offset = {0, 0};
    Scissor.extent = Extent;

    VkPipelineViewportStateCreateInfo ViewportStateInfo={};
    ViewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportStateInfo.viewportCount = 1;
    ViewportStateInfo.pViewports = &Viewport;
    ViewportStateInfo.scissorCount = 1;
    ViewportStateInfo.pScissors = &Scissor;

    /*Rasterizer*/
    VkPipelineRasterizationStateCreateInfo RasterizerInfo={};
    RasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizerInfo.depthClampEnable = VK_FALSE; 
    RasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizerInfo.lineWidth = 1.0f;
    RasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    RasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    RasterizerInfo.depthBiasEnable = VK_FALSE;
    RasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
    RasterizerInfo.depthBiasClamp = 0.0f; // Optional
    RasterizerInfo.depthBiasSlopeFactor = 0.0f; // Optional

    /*Multisampling*/

    //Disabled for now
    VkPipelineMultisampleStateCreateInfo MultisamplingInfo={};
    MultisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisamplingInfo.sampleShadingEnable = VK_FALSE;
    MultisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MultisamplingInfo.minSampleShading = 1.0f; // Optional
    MultisamplingInfo.pSampleMask = NULL; // Optional
    MultisamplingInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    MultisamplingInfo.alphaToOneEnable = VK_FALSE; // Optional 

    /* Depth and Stencil testing*/
    //come back later

    /*Color Blending*/
    VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_FALSE;
    ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo ColorBlendingInfo={};
    ColorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendingInfo.logicOpEnable = VK_FALSE;
    ColorBlendingInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
    ColorBlendingInfo.attachmentCount = 1;
    ColorBlendingInfo.pAttachments = &ColorBlendAttachment;
    ColorBlendingInfo.blendConstants[0] = 0.0f; // Optional
    ColorBlendingInfo.blendConstants[1] = 0.0f; // Optional
    ColorBlendingInfo.blendConstants[2] = 0.0f; // Optional
    ColorBlendingInfo.blendConstants[3] = 0.0f; // Optional

    /*Dynamic State*/
    //This is just an example 
    //probably not gonna use it for now

    VkDynamicState DynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo DynamicStateInfo={};
    DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicStateInfo.dynamicStateCount = 2;
    DynamicStateInfo.pDynamicStates = DynamicStates; 

    /*Pipeline Layout*/

    VkPipelineLayout PipelineLayout;

    VkPipelineLayoutCreateInfo PipelineLayoutInfo={};
    PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.setLayoutCount = 1; 
    PipelineLayoutInfo.pSetLayouts = &DescriptorSetLayout; 
    PipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    PipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

    if(vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, NULL, &PipelineLayout) != VK_SUCCESS)
    {
        ErrorMessageBox("Failed to create pipeline layout!");
    }


    /*Render Pass*/

    VkAttachmentDescription ColorAttachment={};
    ColorAttachment.format = SwapchainImageFormat;
    ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    //loadOp and storeOp apply to color AND depth data
    ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ColorAttachmentRef={};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription DepthAttachment{};
    DepthAttachment.format = VK_FORMAT_D32_SFLOAT;
    DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthAttachmentRef{};
    DepthAttachmentRef.attachment = 1;
    DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription Subpass={};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorAttachmentRef;    
    Subpass.pDepthStencilAttachment=&DepthAttachmentRef;

    VkAttachmentDescription RenderPassAttachments[2]={ColorAttachment, DepthAttachment};
    VkRenderPass RenderPass;

    VkRenderPassCreateInfo RenderPassInfo={};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = 2;
    RenderPassInfo.pAttachments = RenderPassAttachments; 
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;


    //Subpass dependency
    VkSubpassDependency Dependency={};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = 
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstStageMask = 
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.dstAccessMask = 
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    RenderPassInfo.dependencyCount = 1;
    RenderPassInfo.pDependencies = &Dependency;


    if (vkCreateRenderPass(LogicalDevice, &RenderPassInfo, NULL, &RenderPass) != VK_SUCCESS)
    {
        ErrorMessageBox("Failed to create Render Pass!");
    }
    /*Piepline Depth Stencils*/
    VkPipelineDepthStencilStateCreateInfo DepthStencil{};
    DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencil.depthTestEnable = VK_TRUE;
    DepthStencil.depthWriteEnable = VK_TRUE;
    DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    DepthStencil.depthBoundsTestEnable = VK_FALSE;
    DepthStencil.minDepthBounds = 0.0f; // Optional
    DepthStencil.maxDepthBounds = 1.0f; // Optional
    //Need a compatible depth format before enabling stencils
    DepthStencil.stencilTestEnable = VK_FALSE;
    DepthStencil.front = {}; // Optional
    DepthStencil.back = {}; // Optional

    VkGraphicsPipelineCreateInfo PipelineInfo={};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInputInfo;
    PipelineInfo.pInputAssemblyState = &InputAssemblyInfo;
    PipelineInfo.pViewportState = &ViewportStateInfo;
    PipelineInfo.pRasterizationState = &RasterizerInfo;
    PipelineInfo.pMultisampleState = &MultisamplingInfo;
    PipelineInfo.pColorBlendState = &ColorBlendingInfo;
    PipelineInfo.pDynamicState = NULL; // Optional
    PipelineInfo.layout = PipelineLayout;
    PipelineInfo.renderPass = RenderPass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    PipelineInfo.pDepthStencilState = &DepthStencil;
    
    //PipelineInfo.basePipelineIndex = -1; // Optional

    VkPipeline GraphicsPipeline;
    if (vkCreateGraphicsPipelines(LogicalDevice, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &GraphicsPipeline) != VK_SUCCESS)
    {
        ErrorMessageBox("Failed to create Graphics Pipeline!");
    }
    vkDestroyShaderModule(LogicalDevice, FragShaderModule, NULL);
    vkDestroyShaderModule(LogicalDevice, VertShaderModule, NULL);

    

    /*Create Depth Buffer*/
    //we need that now
    GlobalVulkanData.Extent = Extent;
    CreateDepthResources();

    /* Create FrameBuffers*/

    TotalSize += sizeof(VkFramebuffer)*ImageCount;
    Assert(TotalSize<MemSize);
    VkFramebuffer *SwapchainFramebuffers=(VkFramebuffer *)VulkanMemAlloc(sizeof(VkFramebuffer)*ImageCount);

    for (u32 FrameBufferIndex= 0; FrameBufferIndex < ImageCount; ++FrameBufferIndex) 
    {
        VkImageView Attachments[2] = 
        {SwapchainImageViews[FrameBufferIndex], GlobalVulkanData.DepthImageView};

        VkFramebufferCreateInfo FramebufferInfo={};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = RenderPass;
        FramebufferInfo.attachmentCount = 2;
        FramebufferInfo.pAttachments = Attachments;
        FramebufferInfo.width = Extent.width;
        FramebufferInfo.height = Extent.height;
        FramebufferInfo.layers = 1;
        if(vkCreateFramebuffer(LogicalDevice, &FramebufferInfo, NULL, &SwapchainFramebuffers[FrameBufferIndex]) != VK_SUCCESS)
        {
            ErrorMessageBox("Failed to create Framebuffer!");
        }
    }


    GlobalVulkanData.Swapchain = Swapchain;
    GlobalVulkanData.ImageCount=ImageCount;
    GlobalVulkanData.RenderPass=RenderPass;
    GlobalVulkanData.SwapchainFramebuffers = SwapchainFramebuffers;
    GlobalVulkanData.SwapchainImageViews=SwapchainImageViews;
    GlobalVulkanData.GraphicsPipeline = GraphicsPipeline;
    GlobalVulkanData.Extent = Extent;
    GlobalVulkanData.PipelineLayout = PipelineLayout;


}

void Win32InitVulkan(HWND Window, HINSTANCE hInst)
{
    u32 TotalSize=0;
    //Allocate 1 page for our needs 
    u32 MemSize = 4096;
    void *Memory = VirtualAlloc(0, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    GlobalVulkanData.Memory = Memory;
    GlobalVulkanData.MemSize = MemSize;
    GlobalVulkanData.Free= Memory;

    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        ErrorMessageBox("Validation Layers requested but not available!");
    }
    /*Initialize Vulkan Instance*/

    VkInstance Instance;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = APPLICATION_NAME;
    appInfo.pEngineName = ENGINE_NAME;
    appInfo.apiVersion = VK_API_VERSION_1_0;//VK_MAKE_VERSION(1, 2, 175);

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;

    char *InstanceEnabledExtensions[3]=
    { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    createInfo.enabledExtensionCount=3;
    createInfo.ppEnabledExtensionNames=InstanceEnabledExtensions;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkResult InstanceResult = vkCreateInstance(&createInfo, NULL, &Instance);

    if(InstanceResult != VK_SUCCESS)
    {
        if(InstanceResult == VK_ERROR_INCOMPATIBLE_DRIVER)
            ErrorMessageBox("Incompatible driver. Create Instance failed");
        else
            ErrorMessageBox("Create Instance failed");
        vkDestroyInstance(Instance, NULL);
    }
    gInstance=Instance;
    /*Initialize Surface*/

    VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {};
    SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    SurfaceCreateInfo.pNext = NULL;
    SurfaceCreateInfo.flags = 0;
    SurfaceCreateInfo.hinstance = hInst;
    SurfaceCreateInfo.hwnd = Window;

    VkSurfaceKHR Surface;

    VkResult SurfaceResult =
        vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, NULL, &Surface);
    Assert(SurfaceResult == VK_SUCCESS);

    /*Initialize Physical Device*/
    u32 DeviceCount = 0;
    VkResult PhysicalDeviceResult = vkEnumeratePhysicalDevices(Instance, &DeviceCount, NULL);
    Assert(PhysicalDeviceResult == VK_SUCCESS);
    Assert(DeviceCount>=1);
    TotalSize +=sizeof(VkPhysicalDevice)*DeviceCount;
    Assert(TotalSize<MemSize);
    VkPhysicalDevice *PhysicalDevices = (VkPhysicalDevice *) VulkanMemAlloc(sizeof(VkPhysicalDevice)*DeviceCount);


    PhysicalDeviceResult = vkEnumeratePhysicalDevices(Instance, &DeviceCount, PhysicalDevices);
    Assert(PhysicalDeviceResult == VK_SUCCESS);

    VkPhysicalDeviceProperties PhysicalProperties = {};
    i32 DeviceToUse=0;
    for(i32 DeviceIndex=0; DeviceIndex<DeviceCount;++DeviceIndex)
    {
        vkGetPhysicalDeviceProperties(PhysicalDevices[DeviceIndex], &PhysicalProperties);
        //NOTE(Alex): We should also check for other stuff like swapchain support
        //but given the gpu(gtx 1060 6gb) on my pc we should be fine
        if(PhysicalProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            DeviceToUse=DeviceIndex;
            //MessageBox(NULL, PhysicalProperties.deviceName,
            //         ENGINE_NAME, MB_ICONINFORMATION);
            break;
        }
    }

    VkPhysicalDevice PhysicalDevice = PhysicalDevices[DeviceToUse];
    /* Select Queue*/
    u32 QueueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueCount, NULL);

    Assert(QueueCount >= 1);

    TotalSize += sizeof(VkQueueFamilyProperties)*QueueCount;
    Assert(TotalSize<MemSize);
    VkQueueFamilyProperties *QueueProperties=  (VkQueueFamilyProperties *) 
        VulkanMemAlloc(sizeof(VkQueueFamilyProperties)*QueueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueCount,
            QueueProperties);
    //Technically a QueueFamilyIndex !
    u32 PresentQueueIndex=0xffffffff;
    u32 GraphicsQueueIndex=0xffffffff;

    TotalSize+=sizeof(VkBool32)*QueueCount;
    Assert(TotalSize<MemSize);
    VkBool32 *SupportsPresenting= (VkBool32 *) VulkanMemAlloc(sizeof(VkBool32)*QueueCount);

    //Note(Alex): We're seeking a queue that supports presenting to the surface we created
    //and another onewith Graphics capabilities
    for (u32 QueueIndex = 0;QueueIndex  < QueueCount; QueueIndex++)
    {
        if ((QueueProperties[QueueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) 
        {
            GraphicsQueueIndex=QueueIndex; 
            break;
        }
    }
    for (u32 QueueIndex  = 0; QueueIndex  < QueueCount; QueueIndex++)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, QueueIndex, Surface,
                &SupportsPresenting[QueueIndex]);

        if (SupportsPresenting[QueueIndex] == VK_TRUE) 
        {
            PresentQueueIndex = QueueIndex;
            break;
        }
    }

    Assert(GraphicsQueueIndex != 0xffffffff);
    Assert(PresentQueueIndex != 0xffffffff);

    //Note(Alex): We're checking wether or not we're using a single queue
    //Note(Alex): We probably are.
    u32 QueueIndexCount = GraphicsQueueIndex == PresentQueueIndex ? 1:2;

    TotalSize+=sizeof(VkDeviceQueueCreateInfo)*QueueIndexCount;
    Assert(TotalSize<MemSize);
    VkDeviceQueueCreateInfo *QueueCreateInfos=(VkDeviceQueueCreateInfo *)
        VulkanMemAlloc(sizeof(VkDeviceQueueCreateInfo)*QueueIndexCount);
    u32 QueueIndices[] = {GraphicsQueueIndex, PresentQueueIndex}; 
    f32 Priorities[] = { 1.0f };
    for(u32 QueueIndex=0; QueueIndex<QueueIndexCount; ++QueueIndex)
    {
        VkDeviceQueueCreateInfo QueueInfo={};
        QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueInfo.pNext = NULL;
        QueueInfo.flags = 0;
        QueueInfo.queueFamilyIndex = QueueIndices[QueueIndex];
        QueueInfo.queueCount = 1;
        QueueInfo.pQueuePriorities = &Priorities[0];

        QueueCreateInfos[QueueIndex]=QueueInfo;
    }

    char * DeviceEnabledExtensions[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    VkPhysicalDeviceFeatures DeviceFeatures{};
    DeviceFeatures.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo DeviceInfo={};
    DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DeviceInfo.pNext = NULL;
    DeviceInfo.flags = 0;
    DeviceInfo.queueCreateInfoCount = QueueIndexCount;
    DeviceInfo.pQueueCreateInfos = QueueCreateInfos;
    DeviceInfo.enabledExtensionCount = 1;
    DeviceInfo.ppEnabledExtensionNames = DeviceEnabledExtensions;
    DeviceInfo.pEnabledFeatures = &DeviceFeatures;

    VkDevice LogicalDevice;
    VkResult DeviceResult = vkCreateDevice(PhysicalDevice, &DeviceInfo,
            NULL, &LogicalDevice);
    Assert(DeviceResult == VK_SUCCESS);

    VkQueue GraphicsQueue;
    vkGetDeviceQueue(LogicalDevice, GraphicsQueueIndex,0, &GraphicsQueue);
    VkQueue PresentQueue;
    vkGetDeviceQueue(LogicalDevice, PresentQueueIndex,0, &PresentQueue);


    GlobalVulkanData.PhysicalDevice=PhysicalDevice;
    GlobalVulkanData.LogicalDevice=LogicalDevice;
    GlobalVulkanData.Surface = Surface;
    GlobalVulkanData.GraphicsQueueIndex = GraphicsQueueIndex;
    GlobalVulkanData.PresentQueueIndex=PresentQueueIndex;
    GlobalVulkanData.GraphicsQueue = GraphicsQueue;
    GlobalVulkanData.PresentQueue=PresentQueue;

    CreateDescriptorSetLayout(LogicalDevice);

    //Here starts the swapchain block
    GlobalVulkanData.SwapchainMemory = GlobalVulkanData.Free;

    win32CreateSwapchain(Window);    

    /*
       char DbTextBuffer[256];
       _snprintf_s(DbTextBuffer, sizeof(DbTextBuffer)re
       "Current Swapchain Extent:\nWidth: %d\nHeight:%d",
       SwapchainCapabilities.currentExtent.width,
       SwapchainCapabilities.currentExtent.height);


       MessageBox(NULL, DbTextBuffer,
       ENGINE_NAME, MB_ICONINFORMATION);
       */


    /*Command Buffer*/

    /*Create Command Pool*/

    VkCommandPoolCreateInfo CommandPoolInfo={};
    CommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.queueFamilyIndex = GraphicsQueueIndex;
    CommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 0;Optional

    VkCommandPool CommandPool;
    if(vkCreateCommandPool(LogicalDevice, &CommandPoolInfo, NULL, &CommandPool) != VK_SUCCESS)
    {
        ErrorMessageBox("Failed to create Command Pool!");
    }

    GlobalVulkanData.CommandPool=CommandPool;
    /*Command Buffer Allocation*/
    VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT]={};

    VkCommandBufferAllocateInfo CommandBufferAllocInfo={};
    CommandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocInfo.commandPool = CommandPool;
    CommandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if(vkAllocateCommandBuffers(LogicalDevice, &CommandBufferAllocInfo, CommandBuffers) != VK_SUCCESS)
    {
        ErrorMessageBox("Failed to Allocate Command Buffers!");
    }

    /*Created Texture Image*/
    CreateTextureImage(LogicalDevice);

    /*Create Vertex Buffer*/
    VkBuffer VertexBuffer;
    VkDeviceMemory VertexBufferMemory;
    VkBuffer StagingBuffer;
    VkDeviceMemory StagingBufferMemory;

    VkDeviceSize VertexBufferSize=sizeof(Vertex)*VERTEX_COUNT;
    CreateVulkanBuffer(VertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            &StagingBuffer, &StagingBufferMemory);
    void* Data;
    vkMapMemory(LogicalDevice,StagingBufferMemory, 0, VertexBufferSize, 0, &Data);
    memcpy(Data, Vertices, (size_t) VertexBufferSize);
    vkUnmapMemory(LogicalDevice, StagingBufferMemory);

    CreateVulkanBuffer(VertexBufferSize, 
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            &VertexBuffer, &VertexBufferMemory);

    CopyBuffer(StagingBuffer, VertexBuffer, VertexBufferSize);

    vkDestroyBuffer(LogicalDevice, StagingBuffer, NULL);
    vkFreeMemory(LogicalDevice, StagingBufferMemory, NULL);

    /*Create Index Buffer*/
    VkBuffer IndexBuffer;
    VkDeviceMemory IndexBufferMemory;

    VkDeviceSize IndexBufferSize=sizeof(Indices[0])*INDEX_COUNT;
    CreateVulkanBuffer(IndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            &StagingBuffer, &StagingBufferMemory);
    void* IndexData;
    vkMapMemory(LogicalDevice,StagingBufferMemory, 0, IndexBufferSize, 0, &IndexData);
    memcpy(IndexData, Indices, (size_t) IndexBufferSize);
    vkUnmapMemory(LogicalDevice, StagingBufferMemory);

    CreateVulkanBuffer(IndexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            &IndexBuffer, &IndexBufferMemory);

    CopyBuffer(StagingBuffer,IndexBuffer, IndexBufferSize);

    vkDestroyBuffer(LogicalDevice, StagingBuffer, NULL);
    vkFreeMemory(LogicalDevice, StagingBufferMemory, NULL);

    /*Create Semaphores*/
    VkSemaphore ImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore RenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence InFlightFences[MAX_FRAMES_IN_FLIGHT];

    VkSemaphoreCreateInfo SemaphoreInfo={};
    SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo FenceInfo={};
    FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;  
    FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(u32 FrameIndex=0; FrameIndex<MAX_FRAMES_IN_FLIGHT; ++FrameIndex)
    {
        if(vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, NULL, &ImageAvailableSemaphores[FrameIndex]) != VK_SUCCESS ||
                vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, NULL, &RenderFinishedSemaphores[FrameIndex]) != VK_SUCCESS ||
                vkCreateFence(LogicalDevice, &FenceInfo, NULL, &InFlightFences[FrameIndex]) != VK_SUCCESS)
        {
            ErrorMessageBox("Failed to create Semaphore!");
        }
    }

    CreateUniformBuffers();
    CreateDescriptorSet(LogicalDevice);
    for(u32 FrameIndex=0; FrameIndex<MAX_FRAMES_IN_FLIGHT; ++FrameIndex)
    {
        GlobalVulkanData.ImageAvailableSemaphores[FrameIndex]= ImageAvailableSemaphores[FrameIndex];
        GlobalVulkanData.RenderFinishedSemaphores[FrameIndex]= RenderFinishedSemaphores[FrameIndex];
        GlobalVulkanData.InFlightFences[FrameIndex]=InFlightFences[FrameIndex];
        GlobalVulkanData.CommandBuffers[FrameIndex]= CommandBuffers[FrameIndex];
    }
    GlobalVulkanData.CurrentFrame=0;
    GlobalVulkanData.VertexBuffer=VertexBuffer;
    GlobalVulkanData.IndexBuffer=IndexBuffer;

}
void RecordCommandBuffer(VkCommandBuffer CommandBuffer, u32 ImageIndex) 
{
    VkFramebuffer *SwapchainFramebuffers = GlobalVulkanData.SwapchainFramebuffers;
    VkRenderPass RenderPass=GlobalVulkanData.RenderPass;
    VkPipeline GraphicsPipeline = GlobalVulkanData.GraphicsPipeline;
    VkExtent2D Extent = GlobalVulkanData.Extent;
    VkBuffer VertexBuffer=GlobalVulkanData.VertexBuffer;
    VkBuffer IndexBuffer=GlobalVulkanData.IndexBuffer;
    VkPipelineLayout PipelineLayout=GlobalVulkanData.PipelineLayout;

    u32 ImageCount=GlobalVulkanData.ImageCount;
    u32 CurrentFrame=GlobalVulkanData.CurrentFrame;

    VkCommandBufferBeginInfo BeginInfo={};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS) {
        ErrorMessageBox("Failed to begin to record command buffer!");
    }

    VkRenderPassBeginInfo RenderPassInfo={};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassInfo.renderPass = RenderPass;

    Assert(ImageIndex<ImageCount);
    RenderPassInfo.framebuffer = SwapchainFramebuffers[ImageIndex];
    RenderPassInfo.renderArea.offset = {0, 0};
    RenderPassInfo.renderArea.extent = Extent;

    VkClearValue ClearValues[2]={};
    ClearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    RenderPassInfo.clearValueCount = 2; 
    RenderPassInfo.pClearValues = ClearValues;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

    VkBuffer VertexBuffers[] = {VertexBuffer};
    VkDeviceSize Offsets[] = {0};
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    vkCmdBindIndexBuffer(CommandBuffer, IndexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 
                            0, 1, &GlobalVulkanData.DescriptorSets[CurrentFrame], 0, NULL);

    vkCmdDrawIndexed(CommandBuffer, (u32)(INDEX_COUNT), 1, 0, 0,0);

    vkCmdEndRenderPass(CommandBuffer);

    if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS) {
        ErrorMessageBox("Failed to record command buffer!");
    }
}   

void RecreateSwapchain(HWND Window)
{
    u32 ImageCount = GlobalVulkanData.ImageCount;
    VkPhysicalDevice PhysicalDevice =GlobalVulkanData.PhysicalDevice; 
    VkDevice LogicalDevice=GlobalVulkanData.LogicalDevice;
    VkSurfaceKHR Surface = GlobalVulkanData.Surface;
    VkSwapchainKHR Swapchain=GlobalVulkanData.Swapchain;
    VkRenderPass RenderPass=GlobalVulkanData.RenderPass;
    VkPipeline GraphicsPipeline = GlobalVulkanData.GraphicsPipeline;
    VkPipelineLayout PipelineLayout = GlobalVulkanData.PipelineLayout;
    VkFramebuffer *SwapchainFramebuffers = GlobalVulkanData.SwapchainFramebuffers;
    VkImageView *SwapchainImageViews = GlobalVulkanData.SwapchainImageViews;


    vkDeviceWaitIdle(LogicalDevice);

    /*Cleanup current swapchain*/
    for (u32 ImageIndex= 0; ImageIndex < ImageCount; ++ImageIndex)
    {
        vkDestroyFramebuffer(LogicalDevice, SwapchainFramebuffers[ImageIndex], NULL);
    }

    vkDestroyPipeline(LogicalDevice, GraphicsPipeline, NULL);
    vkDestroyPipelineLayout(LogicalDevice, PipelineLayout, NULL);
    vkDestroyRenderPass(LogicalDevice, RenderPass, NULL);

    for (u32 ImageIndex= 0; ImageIndex < ImageCount; ++ImageIndex)
    {
        vkDestroyImageView(LogicalDevice, SwapchainImageViews[ImageIndex], NULL);
    }
    vkDestroyImageView(LogicalDevice, GlobalVulkanData.DepthImageView, NULL);
    vkDestroyImage(LogicalDevice, GlobalVulkanData.DepthImage, NULL);
    vkFreeMemory(LogicalDevice, GlobalVulkanData.DepthImageMemory, NULL);
    

    vkDestroySwapchainKHR(LogicalDevice, Swapchain, NULL);

    /*Recreate the Swapchain*/
    GlobalVulkanData.Free=GlobalVulkanData.SwapchainMemory;
    win32CreateSwapchain(Window);

}

void DrawFrame(HWND Window)
{
    u32 ImageIndex;
    u32 CurrentFrame=GlobalVulkanData.CurrentFrame;
    u32 ImageCount = GlobalVulkanData.ImageCount;
    VkDevice LogicalDevice=GlobalVulkanData.LogicalDevice;
    VkSwapchainKHR Swapchain=GlobalVulkanData.Swapchain;
    VkSemaphore ImageAvailableSemaphore=GlobalVulkanData.ImageAvailableSemaphores[CurrentFrame], 
                RenderFinishedSemaphore=GlobalVulkanData.RenderFinishedSemaphores[CurrentFrame];
    VkFence InFlightFence = GlobalVulkanData.InFlightFences[CurrentFrame];
    VkCommandBuffer CommandBuffer = GlobalVulkanData.CommandBuffers[CurrentFrame];
    VkQueue PresentQueue=GlobalVulkanData.PresentQueue;
    VkQueue GraphicsQueue=GlobalVulkanData.GraphicsQueue;

    vkWaitForFences(LogicalDevice, 1, &InFlightFence, VK_TRUE, U64MAX);

    VkResult Result =vkAcquireNextImageKHR(LogicalDevice, Swapchain, U64MAX, 
            ImageAvailableSemaphore, VK_NULL_HANDLE, &ImageIndex);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain(Window);
        return;
    }
    else if(Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
    {
        ErrorMessageBox("Failed to acquire next swapchain image!");
    }

    UpdateUniformBuffer(CurrentFrame);
    vkResetFences(LogicalDevice, 1, &InFlightFence);

    vkResetCommandBuffer(CommandBuffer, 0);
    RecordCommandBuffer(CommandBuffer, ImageIndex);



    VkSubmitInfo SubmitInfo={};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore WaitSemaphores[] = {ImageAvailableSemaphore};
    VkPipelineStageFlags WaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = WaitSemaphores;
    SubmitInfo.pWaitDstStageMask = WaitStages;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CommandBuffer;

    VkSemaphore SignalSemaphores[] = {RenderFinishedSemaphore};
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = SignalSemaphores;

    if(vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, InFlightFence) != VK_SUCCESS)   
    {
        ErrorMessageBox("Failed to submit draw command buffer!");
    }


    VkPresentInfoKHR PresentInfo={};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = SignalSemaphores;

    VkSwapchainKHR Swapchains[] = {Swapchain};
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = Swapchains;
    PresentInfo.pImageIndices = &ImageIndex;
    PresentInfo.pResults = NULL; // Optional

    Result = vkQueuePresentKHR(PresentQueue, &PresentInfo);

    if(Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain(Window);
    }
    else if(Result != VK_SUCCESS )
    {
        ErrorMessageBox("Failed to present swapchain image!");
    }

    GlobalVulkanData.CurrentFrame = (CurrentFrame + 1)%MAX_FRAMES_IN_FLIGHT;

}


LRESULT CALLBACK MainWindowCallBack(HWND   Window, UINT   Msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT Result ={};
    switch(Msg)
    {
        case(WM_DESTROY):
        case(WM_CLOSE):
        case(WM_QUIT):
            GlobalRunning = false;
            break;
        case(WM_SIZE):
            if(wParam == SIZE_MINIMIZED)
                GlobalMinimized = true;
            else
                GlobalMinimized=false;
        case(WM_PAINT):
            if(GlobalRunning && !GlobalMinimized)
            {
                RecreateSwapchain(Window);
                DrawFrame(Window);
            }
            break;

    }
    Result = DefWindowProc(Window, Msg, wParam, lParam);
    return Result;
}


int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    WNDCLASSEX WindowClass ={};
    WindowClass.cbSize = sizeof(WNDCLASSEX); 
    WindowClass.lpfnWndProc = MainWindowCallBack;
    WindowClass.style =CS_VREDRAW | CS_HREDRAW;
    WindowClass.hInstance = hInst;
    WindowClass.lpszClassName= "VulkanRendererWindowClassName";

    if(RegisterClassEx(&WindowClass))
    {
        HWND Window = CreateWindowEx(WS_EX_CLIENTEDGE,
                WindowClass.lpszClassName, 
                "Software Renderer",
                WS_VISIBLE |WS_OVERLAPPEDWINDOW, 
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                hInst,
                0); 
        if(Window)
        {
            //We need access to stdout in order to use validation layers
            if(AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
            {
                freopen("CONOUT$", "w", stdout);
                freopen("CONOUT$", "w", stderr);
            }

            MSG msg = { };
            //NOTE(Alex): WM_PAINT bypasses the message queue 
            //so it needs to be handled in the callback function
            GlobalRunning = true;
            GlobalMinimized=false;
            Win32InitVulkan(Window, hInst);
            while(GlobalRunning)
            {
                while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE) )
                {

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);

                }
                if(!GlobalRunning)
                    break;
                if(GlobalMinimized)
                    continue;
                DrawFrame(Window);
            }

        }
    }
    return 0;
}
