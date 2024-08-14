#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#include "rayCommon.glsl"

layout(location = 0) rayPayloadInEXT PathPayload prd;

hitAttributeEXT vec2 baryCoord;

void main()
{
    prd.hitT          = gl_HitTEXT;
    prd.primitiveID   = gl_PrimitiveID;
    prd.instanceID    = gl_InstanceID;
    prd.instanceIndex = gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT;
    prd.baryCoord     = baryCoord;
    prd.objectToWorld = gl_ObjectToWorldEXT;
    prd.worldToObject = gl_WorldToObjectEXT;
}