
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
typedef float r32;
typedef double r64;
typedef float f32;
typedef double f64;
typedef bool b32;
#define F32MAX FLT_MAX
#define F32MIN -FLT_MAX
#define U32MAX ((u32)-1)
#define U64MAX ((u64)-1)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define Assert assert 

//macros operate on 32 bits by default so we need to specify the use of long long (64 bits)
#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value) * 1024LL)
#define Gigabytes(value) (Megabytes(value) * 1024LL)
#define Terabytes(value) (Gigabytes(value) * 1024LL)

#define MAX_FRAMES_IN_FLIGHT 2

struct File
{
    void *Contents;
    u32 FileSize;
};

struct VulkanData
{
    void *Memory;
    void *SwapchainMemory;
    void *Free;
    u32  MemSize;
    VkPhysicalDevice PhysicalDevice;
    VkDevice LogicalDevice;
    VkSurfaceKHR Surface;
    VkSwapchainKHR Swapchain;
    VkSemaphore ImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore RenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence InFlightFences[MAX_FRAMES_IN_FLIGHT];
    u32 ImageCount;
    VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT];
    u32 GraphicsQueueIndex;
    u32 PresentQueueIndex;
    VkQueue GraphicsQueue;
    VkQueue PresentQueue;
    VkRenderPass RenderPass;
    VkFramebuffer *SwapchainFramebuffers;
    VkImageView *SwapchainImageViews;
    VkPipeline GraphicsPipeline;
    VkExtent2D Extent;
    VkPipelineLayout PipelineLayout;
    u32 CurrentFrame;
};

inline i32
Clamp(i32 Min, i32 Value, i32 Max)
{
    i32 Result;
    if(Value<Min)
        Result = Min;
    else if(Value > Max)
        Result = Max;
    else
        Result = Value;

    return Result;
}
