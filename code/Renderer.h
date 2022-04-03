
#define PI32 3.14159265359f
#define TAU32 6.28318530717958647692f

#include <math.h>
#include <intrin.h>
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
#define VERTEX_COUNT 4
#define INDEX_COUNT 6
#define ATTRIBUTES_COUNT 3

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
    VkBuffer IndexBuffer;
    VkCommandPool CommandPool;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkBuffer UniformBuffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory UniformBuffersMemory[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet DescriptorSets[MAX_FRAMES_IN_FLIGHT];
    VkImage TextureImage;
    VkDeviceMemory TextureImageMemory;
    VkImageView TextureImageView;
    VkSampler TextureSampler;
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

union v4
{
   struct
   {
       f32 x,y,z,w;
   };
   struct
   {
       f32 r,g,b,a;
   };
   struct
   {
       v3 xyz;
       f32 _Ingnored1;
   };
   struct
   {
       v2 xy;
       v2 _Ignored2;
   };
   struct
   {
       f32 _Ingnored3;
       v2 yz;
       f32 _Ignored4;
   };
    __m128 Vec;
   
};

union mat4
{
    struct
    {
        v4 X,Y,Z,W;
    };
    struct
    {
        __m128 A;
        __m128 B;
        __m128 C;
        __m128 D;
    };
    v4 col[4];
    f32 M[4][4];
};

struct Vertex
{
    v2 Position;
    v3 Color;
    v2 TexCoord;
};

struct UniformBufferObject
{
    mat4 Model,View,Proj;
};

static Vertex Vertices[VERTEX_COUNT] =
{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

static u16 Indices[]={0,1,2,2,3,0};

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


static VkVertexInputAttributeDescription AttributeDescriptions[ATTRIBUTES_COUNT];
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
    
    AttributeDescriptions[2].binding = 0;
    AttributeDescriptions[2].location = 2;
    AttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[2].offset = OFFSETOF(Vertex, TexCoord);

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

inline v4 
operator*(v4 A, v4 B)
{
    v4 Result={A.x*B.x,A.y*B.y,A.z*B.z,A.w*B.w};
    return Result;

};

inline v4
operator*(mat4 M, v4 v)
{
    v4 Result ={};
    __m128 vx = _mm_set_ps1(v.x);
    __m128 vy = _mm_set_ps1(v.y);
    __m128 vz = _mm_set_ps1(v.z);
    __m128 vw = _mm_set_ps1(v.w);
    Result.Vec = _mm_add_ps(_mm_mul_ps(vx, M.A),
                           _mm_add_ps(_mm_mul_ps(vy, M.B),
                           _mm_add_ps(_mm_mul_ps(vz, M.C), _mm_mul_ps(vw, M.D))));

    return Result;

}
inline mat4
operator*(mat4 A, mat4 B)
{
    mat4 Result;
    Result.X = A*B.X;
    Result.Y = A*B.Y;
    Result.Z = A*B.Z;
    Result.W = A*B.W;
    return Result;
}
