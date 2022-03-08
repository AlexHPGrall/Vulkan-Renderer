#define NEAR_PLANE 0.5f 
#define FAR_PLANE 100

static mat4
InverseRotationAndTranslationMatrix(mat4 *Matrix)
{
    mat4 Result ={};
    Result.X= {Matrix->M[0][0],Matrix->M[1][0],Matrix->M[2][0], 0};
    Result.Y= {Matrix->M[0][1],Matrix->M[1][1],Matrix->M[2][1], 0};
    Result.Z= {Matrix->M[0][2],Matrix->M[1][2],Matrix->M[2][2], 0};
    Result.W.w = 1;
    Result.W.x = -(Matrix->W.x*Result.M[0][0] + 
            Matrix->W.y*Result.M[1][0]+Matrix->W.z*Result.M[2][0]);
    Result.W.y = -(Matrix->W.x*Result.M[0][1] +
            Matrix->W.y*Result.M[1][1]+Matrix->W.z*Result.M[2][1]);
    Result.W.z = -(Matrix->W.x*Result.M[0][2] +
            Matrix->W.y*Result.M[1][2]+Matrix->W.z*Result.M[2][2]);

    return Result;
}

//This is a Projection Matrix made for Vulkan
//it assumes that the camera is looking up the Z axis
static mat4
ProjectionMatrix(f32 fov, f32 Width, f32 Height)
{
    mat4 Result={};
    f32 ScreenHalfWidth = Width/2.0f;
    f32 ScreenHalfHeight= Height/2.0f;
    f32 FocalDist= ScreenHalfWidth/tanf(fov/2.0f);
    f32 f=FAR_PLANE;
    f32 n=NEAR_PLANE;
    Result.X={FocalDist/ScreenHalfWidth,0,0,0};
    Result.Y={0,FocalDist/ScreenHalfHeight,0,0};
    Result.Z={0,0,(n)/(n-f),1};
    Result.W={0,0,-(f*n)/(n-f),0};

    return Result;
}

static mat4
XRotationMatrix(f32 angle, v4 Translation={0})
{
    mat4 Result = {};
    Result.X.xyz = {1,0,0};
    Result.Y.yz = {cosf(angle), sinf(angle)};
    Result.Z.yz = {-sinf(angle), cosf(angle)};
    Result.W=Translation;
    Result.M[3][3] = 1;
    return Result;
}

static mat4
YRotationMatrix(f32 angle, v4 Translation={0})
{
    mat4 Result = {};
    Result.X.xyz = {cosf(angle),0,-sinf(angle)};
    Result.Y.y = 1;
    Result.Z.xyz = {sinf(angle), 0,cosf(angle)};
    Result.W=Translation;
    Result.M[3][3] = 1;
    return Result;
}

static mat4
ZRotationMatrix(f32 angle, v4 Translation={0})
{
    mat4 Result = {};
    Result.X.xy = {cosf(angle), sinf(angle)};
    Result.Y.xy = {-sinf(angle), cosf(angle)};
    Result.Z.z = 1;
    Result.W=Translation;
    Result.M[3][3] = 1;
    return Result;
}

