
@echo off
glslc -fshader-stage=vertex shaders\vert.glsl -o shaders\vert.spv
glslc -fshader-stage=fragment shaders\frag.glsl -o shaders\frag.spv
pushd ..\build
set CompilerFlags= -Od -Oi -Z7 -MT -FC -GR- -EHa
set LinkerFlags= -OPT:REF -INCREMENTAL:NO user32.lib Gdi32.lib C:\VulkanSDK\1.3.204.0\Lib\vulkan-1.lib 
cl %CompilerFlags% ../code/win32Vulkan.cpp /link %LinkerFlags%

popd
