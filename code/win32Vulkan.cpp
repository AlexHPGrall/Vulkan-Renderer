#include <windows.h>
#include <stdint.h>
#include <float.h>
#include <assert.h>
#include<iostream>
#include <stdexcept>
#include<cstring>

//shameful
#include <vector>

#include "vulkan/vulkan_core.h"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_win32.h"
#include "./Renderer.h"
//Validation Layer stuff lifted almost straight from Vulkan Tutorial
VkInstance gInstance;
#include "./ValidationLayers.h"

static b32 GlobalRunning;
static b32 GlobalMinimized;
static VulkanData GlobalVulkanData;

//static win32_image_buffer GlobalBackBuffer;
#define APPLICATION_NAME "Vulkan Example"
#define ENGINE_NAME "Vulkan Engine"


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
    MessageBox(NULL, "Failed to find suitable memory type!",
            ENGINE_NAME, MB_ICONINFORMATION);
    return -1;

}

void *VulkanMemAlloc(u32 Size)
{
    void *Result = GlobalVulkanData.Free;
    GlobalVulkanData.Free=(u8 *)GlobalVulkanData.Free+Size;

    return Result;
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
             MessageBox(NULL, "Swapchain not suitable",
                     ENGINE_NAME, MB_ICONINFORMATION);
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
        MessageBox(NULL, "Couldn't create Swapchain!",
              ENGINE_NAME, MB_ICONERROR);

    
    
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
        VkImageViewCreateInfo ImageViewInfo={};
        ImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ImageViewInfo.image = SwapchainImages[ImageViewIndex];
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewInfo.format = SwapchainImageFormat;
        ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageViewInfo.subresourceRange.baseMipLevel = 0;
        ImageViewInfo.subresourceRange.levelCount = 1;
        ImageViewInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(LogicalDevice, &ImageViewInfo, NULL, &SwapchainImageViews[ImageViewIndex]) != VK_SUCCESS)
        {
            MessageBox(NULL, "Couldn't create Image Views!",
                        ENGINE_NAME, MB_ICONERROR);
        }
        
    }

    /*Graphics Pipeline*/

    /*Loading Shaders*/
    File VertShaderFile= win32ReadEntireFile("../code/shaders/vert.spv");
    File FragShaderFile= win32ReadEntireFile("../code/shaders/frag.spv");
    if(!VertShaderFile.Contents || !FragShaderFile.Contents)
    {
            MessageBox(NULL, "Couldn't open shader file!",
                        ENGINE_NAME, MB_ICONERROR);
    }
    VkShaderModuleCreateInfo VertShaderModuleInfo={};
    VertShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VertShaderModuleInfo.codeSize = VertShaderFile.FileSize;
    VertShaderModuleInfo.pCode = (u32 *) VertShaderFile.Contents;

    VkShaderModule VertShaderModule;
    if(vkCreateShaderModule(LogicalDevice, &VertShaderModuleInfo, NULL, &VertShaderModule) != VK_SUCCESS)
    {
            MessageBox(NULL, "Couldn't create vertex shader module!",
                        ENGINE_NAME, MB_ICONERROR);
    }

    VkShaderModuleCreateInfo FragShaderModuleInfo={};
    FragShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragShaderModuleInfo.codeSize = FragShaderFile.FileSize;
    FragShaderModuleInfo.pCode = (u32 *) FragShaderFile.Contents;

    VkShaderModule FragShaderModule;
    if(vkCreateShaderModule(LogicalDevice, &FragShaderModuleInfo, NULL, &FragShaderModule) != VK_SUCCESS)
    {
            MessageBox(NULL, "Couldn't create fragment shader module!",
                        ENGINE_NAME, MB_ICONERROR);
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
    VertexInputInfo.vertexAttributeDescriptionCount = 2;
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
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

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
    PipelineLayoutInfo.setLayoutCount = 0; // Optional
    PipelineLayoutInfo.pSetLayouts = NULL; // Optional
    PipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    PipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

    if(vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, NULL, &PipelineLayout) != VK_SUCCESS)
    {
            MessageBox(NULL, "Failed to create pipeline layout!",
                        ENGINE_NAME, MB_ICONERROR);
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
    
    VkSubpassDescription Subpass={};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorAttachmentRef;    
    
    VkRenderPass RenderPass;

    VkRenderPassCreateInfo RenderPassInfo={};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = 1;
    RenderPassInfo.pAttachments = &ColorAttachment;
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;

    //Subpass dependency
    VkSubpassDependency Dependency={};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    RenderPassInfo.dependencyCount = 1;
    RenderPassInfo.pDependencies = &Dependency;


    if (vkCreateRenderPass(LogicalDevice, &RenderPassInfo, NULL, &RenderPass) != VK_SUCCESS)
    {
            MessageBox(NULL, "Failed to create Render Pass!",
                        ENGINE_NAME, MB_ICONERROR);
    }

    VkGraphicsPipelineCreateInfo PipelineInfo={};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInputInfo;
    PipelineInfo.pInputAssemblyState = &InputAssemblyInfo;
    PipelineInfo.pViewportState = &ViewportStateInfo;
    PipelineInfo.pRasterizationState = &RasterizerInfo;
    PipelineInfo.pMultisampleState = &MultisamplingInfo;
    PipelineInfo.pDepthStencilState = NULL; // Optional
    PipelineInfo.pColorBlendState = &ColorBlendingInfo;
    PipelineInfo.pDynamicState = NULL; // Optional
    PipelineInfo.layout = PipelineLayout;
    PipelineInfo.renderPass = RenderPass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    //PipelineInfo.basePipelineIndex = -1; // Optional

    VkPipeline GraphicsPipeline;
    if (vkCreateGraphicsPipelines(LogicalDevice, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &GraphicsPipeline) != VK_SUCCESS)
    {
            MessageBox(NULL, "Failed to create Graphics Pipeline!",
                        ENGINE_NAME, MB_ICONERROR);
    }
    vkDestroyShaderModule(LogicalDevice, FragShaderModule, NULL);
    vkDestroyShaderModule(LogicalDevice, VertShaderModule, NULL);

    /* Create FrameBuffers*/

    TotalSize += sizeof(VkFramebuffer)*ImageCount;
    Assert(TotalSize<MemSize);
    VkFramebuffer *SwapchainFramebuffers=(VkFramebuffer *)VulkanMemAlloc(sizeof(VkFramebuffer)*ImageCount);

    for (u32 FrameBufferIndex= 0; FrameBufferIndex < ImageCount; ++FrameBufferIndex) 
    {
        VkImageView Attachments[] = {SwapchainImageViews[FrameBufferIndex]};

        VkFramebufferCreateInfo FramebufferInfo={};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = RenderPass;
        FramebufferInfo.attachmentCount = 1;
        FramebufferInfo.pAttachments = Attachments;
        FramebufferInfo.width = Extent.width;
        FramebufferInfo.height = Extent.height;
        FramebufferInfo.layers = 1;
        if(vkCreateFramebuffer(LogicalDevice, &FramebufferInfo, NULL, &SwapchainFramebuffers[FrameBufferIndex]) != VK_SUCCESS)
        {
                MessageBox(NULL, "Failed to create Framebuffer!",
                            ENGINE_NAME, MB_ICONERROR);
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
            MessageBox(NULL, "Validation Layers requested but not available!",
                        ENGINE_NAME, MB_ICONERROR);
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
            MessageBox(NULL, "Incompatible driver. Create Instance failed",
                        ENGINE_NAME, MB_ICONERROR);
        else
            MessageBox(NULL, "Create Instance failed",
                        ENGINE_NAME, MB_ICONERROR);
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
    VkDeviceCreateInfo DeviceInfo={};
    DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DeviceInfo.pNext = NULL;
    DeviceInfo.flags = 0;
    DeviceInfo.queueCreateInfoCount = QueueIndexCount;
    DeviceInfo.pQueueCreateInfos = QueueCreateInfos;
    DeviceInfo.enabledExtensionCount = 1;
    DeviceInfo.ppEnabledExtensionNames = DeviceEnabledExtensions;
    DeviceInfo.pEnabledFeatures = NULL;

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

    //Here starts the swapchain block
    GlobalVulkanData.SwapchainMemory = GlobalVulkanData.Free;

    win32CreateSwapchain(Window);    

    /*
    char DbTextBuffer[256];
    _snprintf_s(DbTextBuffer, sizeof(DbTextBuffer),
            "Current Swapchain Extent:\nWidth: %d\nHeight:%d",
            SwapchainCapabilities.currentExtent.width,
            SwapchainCapabilities.currentExtent.height);


    MessageBox(NULL, DbTextBuffer,
              ENGINE_NAME, MB_ICONINFORMATION);
    */


    /*Create Vertex Buffer*/
    VkBufferCreateInfo BufferInfo{};
    
    BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = sizeof(Vertex) *VERTEX_COUNT; 
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer VertexBuffer;
    if (vkCreateBuffer(LogicalDevice, &BufferInfo, NULL, &VertexBuffer) != VK_SUCCESS)
    {
                MessageBox(NULL, "Failed to create Vertex Buffer!",
                            ENGINE_NAME, MB_ICONERROR);
    }
    VkMemoryRequirements MemRequirements;
    vkGetBufferMemoryRequirements(LogicalDevice, VertexBuffer, &MemRequirements);

    VkMemoryAllocateInfo VertexBufferAllocInfo{};
    VertexBufferAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VertexBufferAllocInfo.allocationSize = MemRequirements.size;
    VertexBufferAllocInfo.memoryTypeIndex = 
    FindMemoryType(MemRequirements.memoryTypeBits, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory VertexBufferMemory;
    if (vkAllocateMemory(LogicalDevice, &VertexBufferAllocInfo, NULL, &VertexBufferMemory) != VK_SUCCESS) 
    {
            MessageBox(NULL, "Failed to allocate Vertex Buffer memory!",
                        ENGINE_NAME, MB_ICONERROR);
    }

    vkBindBufferMemory(LogicalDevice, VertexBuffer, VertexBufferMemory, 0);

    void* Data;
    vkMapMemory(LogicalDevice, VertexBufferMemory, 0, BufferInfo.size, 0, &Data);
    memcpy(Data, Vertices, (size_t) BufferInfo.size);
    vkUnmapMemory(LogicalDevice, VertexBufferMemory);
   /*Command Buffer*/

    /*Create Command Pool*/

    VkCommandPoolCreateInfo CommandPoolInfo={};
    CommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.queueFamilyIndex = GraphicsQueueIndex;
    CommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 0;Optional

    VkCommandPool CommandPool;
    if(vkCreateCommandPool(LogicalDevice, &CommandPoolInfo, NULL, &CommandPool) != VK_SUCCESS)
    {
                MessageBox(NULL, "Failed to create Command Pool!",
                            ENGINE_NAME, MB_ICONERROR);
    }
    
    /*Command Buffer Allocation*/
    VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT]={};

    VkCommandBufferAllocateInfo CommandBufferAllocInfo={};
    CommandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocInfo.commandPool = CommandPool;
    CommandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if(vkAllocateCommandBuffers(LogicalDevice, &CommandBufferAllocInfo, CommandBuffers) != VK_SUCCESS)
    {
                MessageBox(NULL, "Failed to Allocate Command Buffers!",
                            ENGINE_NAME, MB_ICONERROR);
    }


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
                    MessageBox(NULL, "Failed to create Semaphore!",
                                ENGINE_NAME, MB_ICONERROR);
        }
    }

    for(u32 FrameIndex=0; FrameIndex<MAX_FRAMES_IN_FLIGHT; ++FrameIndex)
    {
        GlobalVulkanData.ImageAvailableSemaphores[FrameIndex]= ImageAvailableSemaphores[FrameIndex];
        GlobalVulkanData.RenderFinishedSemaphores[FrameIndex]= RenderFinishedSemaphores[FrameIndex];
        GlobalVulkanData.InFlightFences[FrameIndex]=InFlightFences[FrameIndex];
        GlobalVulkanData.CommandBuffers[FrameIndex]= CommandBuffers[FrameIndex];
    }
    GlobalVulkanData.CurrentFrame=0;
    GlobalVulkanData.VertexBuffer=VertexBuffer;
    
}
 void RecordCommandBuffer(VkCommandBuffer CommandBuffer, u32 ImageIndex) 
{
        VkFramebuffer *SwapchainFramebuffers = GlobalVulkanData.SwapchainFramebuffers;
        VkRenderPass RenderPass=GlobalVulkanData.RenderPass;
        VkPipeline GraphicsPipeline = GlobalVulkanData.GraphicsPipeline;
        VkExtent2D Extent = GlobalVulkanData.Extent;
        VkBuffer VertexBuffer=GlobalVulkanData.VertexBuffer;

        u32 ImageCount=GlobalVulkanData.ImageCount;

        VkCommandBufferBeginInfo BeginInfo={};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS) {
                MessageBox(NULL, "Failed to begin to record command buffer!",
                            ENGINE_NAME, MB_ICONERROR);
        }

        VkRenderPassBeginInfo RenderPassInfo={};
        RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassInfo.renderPass = RenderPass;
        
        Assert(ImageIndex<ImageCount);
        RenderPassInfo.framebuffer = SwapchainFramebuffers[ImageIndex];
        RenderPassInfo.renderArea.offset = {0, 0};
        RenderPassInfo.renderArea.extent = Extent;

        VkClearValue ClearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        RenderPassInfo.clearValueCount = 1;
        RenderPassInfo.pClearValues = &ClearColor;

        vkCmdBeginRenderPass(CommandBuffer, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

        VkBuffer VertexBuffers[] = {VertexBuffer};
        VkDeviceSize Offsets[] = {0};
        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
        

        vkCmdDraw(CommandBuffer, VERTEX_COUNT, 1, 0, 0);

        vkCmdEndRenderPass(CommandBuffer);

        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS) {
                MessageBox(NULL, "Failed to record command buffer!",
                            ENGINE_NAME, MB_ICONERROR);
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
                MessageBox(NULL, "Failed to acquire next swapchain image!",
                            ENGINE_NAME, MB_ICONERROR);
    }

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
                MessageBox(NULL, "Failed to submit draw command buffer!",
                            ENGINE_NAME, MB_ICONERROR);
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
                MessageBox(NULL, "Failed to present swapchain image!",
                            ENGINE_NAME, MB_ICONERROR);
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
