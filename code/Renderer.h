
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
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

//macros operate on 32 bits by default so we need to specify the use of long long (64 bits)
#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value) * 1024LL)
#define Gigabytes(value) (Megabytes(value) * 1024LL)
#define Terabytes(value) (Gigabytes(value) * 1024LL)

#define MAX_FRAMES_IN_FLIGHT 2
#define VERTEX_COUNT 3

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
    VkBuffer VertexBuffer;
    VkCommandPool CommandPool;
};

struct v2
{
    f32 x;
    f32 y;
};

union v3
{
   struct
   {
       f32 x,y,z;
   };
   struct
   {
       f32 r,g,b;
   };
};

struct Vertex
{
    v2 Position;
    v3 Color;
};


static Vertex Vertices[VERTEX_COUNT] =
{
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};
//Depends on vertex format, so we might change that later

//Tells how to load vertex data
static VkVertexInputBindingDescription GetBindingDescription()
{
        VkVertexInputBindingDescription BindingDescription{};
        BindingDescription.binding = 0;
        BindingDescription.stride = sizeof(Vertex);
        BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return BindingDescription;
}
//Tells how to handle vertex data


static VkVertexInputAttributeDescription AttributeDescriptions[2];
static VkVertexInputAttributeDescription *GetAttributeDescription()
{
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[0].offset = OFFSETOF(Vertex, Position);
    AttributeDescriptions[1].binding = 0;
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[1].offset = OFFSETOF(Vertex, Color);
    return AttributeDescriptions;
}

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
