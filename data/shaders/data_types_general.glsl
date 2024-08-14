#ifndef DATA_TYPES_GENERAL_GLSL
#define DATA_TYPES_GENERAL_GLSL

layout (buffer_reference, scalar) buffer fvec3Buf { vec3 val[]; };
layout (buffer_reference, scalar) buffer ivec3Buf { ivec3 val[]; };
layout (buffer_reference, scalar) buffer uvec3Buf { uvec3 val[]; };

layout (buffer_reference, scalar) buffer i32 { int32_t val; };
layout (buffer_reference, scalar) buffer u32 { uint32_t val; };

layout (buffer_reference, scalar) buffer i32Buf { int32_t val[]; };
layout (buffer_reference, scalar) buffer u32Buf { uint32_t val[]; };
layout (buffer_reference, scalar) buffer u64Buf { uint64_t val[]; };


#endif // DATA_TYPES_GENERAL_GLSL