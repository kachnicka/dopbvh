#include "random.glsl"

#define ENV_MAP 0
#define RR 0
#define RR_MIN_DEPTH 0

#define PT_DEPTH_TMP 8

void ClosestHit(RayKHR r)
{
    perInvocationRayCounter++;
    uint rayFlags = gl_RayFlagsCullBackFacingTrianglesEXT;
    prd.hitT = INFINITY;
    traceRayEXT(
        topLevelAS,
        rayFlags,
        0xFF,
        0,
        0,
        0,
        r.origin,
        0.f,
        r.direction,
        INFINITY,
        0
    );
}

bool AnyHit(RayKHR r, float maxDist)
{
    shadow_payload.isHit = true;
    shadow_payload.seed = prd.seed;
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsCullBackFacingTrianglesEXT;

    traceRayEXT(
        topLevelAS,
        rayFlags,
        0xFF,
        0,
        0,
        1,
        r.origin,
        EPSILON,
        r.direction,
        maxDist,
        1
    );

    return shadow_payload.isHit;
}

vec3 PathTrace(RayKHR r)
{
    vec3 radiance = vec3(0.f);
    vec3 throughput = vec3(1.f);
    vec3 absorption = vec3(0.f);

    for (int depth = 0; depth < PT_DEPTH_TMP; depth++)
    {
        ClosestHit(r);

        if(prd.hitT == INFINITY)
        {
            if (depth == 0)
                return vec3(0.15f);

            vec3 env;
            env = vec3(max(0.f, dot(r.direction.xyz, pc.dirLight.xyz)));
            env *= pc.dirLight.w;
            return radiance + (env * throughput);
        }

        NodeInstance i = sceneDescription.instances[prd.instanceIndex];
        Vertices vertices = Vertices(i.vertexAddress);
        Indices indices = Indices(i.indexAddress);
        Normals normals = Normals(i.normalAddress);

        const ivec3 ind = indices.i[prd.primitiveID];

        const vec3 v0 = vertices.v[ind.x];
        const vec3 v1 = vertices.v[ind.y];
        const vec3 v2 = vertices.v[ind.z];

        const vec3 n0 = normals.n[ind.x];
        const vec3 n1 = normals.n[ind.y];
        const vec3 n2 = normals.n[ind.z];

        const vec3 barycentrics = vec3(1.0 - prd.baryCoord.x - prd.baryCoord.y, prd.baryCoord.x, prd.baryCoord.y);

        const vec3 pos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
        const vec3 worldPos = vec3(prd.objectToWorld * vec4(pos, 1.0));

        const vec3 nrm = n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z;
        const vec3 worldNrm = normalize(vec3(nrm * prd.worldToObject));

        // Lambert sample
        const vec3 albedo = vec3(1.f);
        const vec3 newDirection = SampleHemisphereCosineWorldSpace(rnd(prd.seed), rnd(prd.seed), worldNrm);
        const float cosTheta = dot(newDirection, worldNrm);
        const vec3 f = albedo * M_PI_INV;
        const float pdf = cosTheta * M_PI_INV;

        throughput *= (f * cosTheta) / pdf;
        r.origin = OffsetRay(worldPos, worldNrm);
        r.direction = newDirection;
    }

    return radiance;
}

vec3 samplePixel(in ivec2 imageCoords, in ivec2 sizeImage, in uint sampleId)
{
    vec3 pixelColor = vec3(0.f);

    const vec2 subpixelJitter = sampleId == 0 ? vec2(.5f) : vec2(rnd(prd.seed), rnd(prd.seed));
    const vec2 pixelCenter    = vec2(imageCoords) + subpixelJitter;
    const vec2 inUV           = pixelCenter / vec2(sizeImage.xy);
    const vec2 d              = inUV * 2.f - 1.f;

    vec4 origin    = camera.viewInv * vec4(0.f, 0.f, 0.f, 1.f);
    vec4 target    = camera.projectionInv * vec4(d.x, d.y, 1.f, 1.f);
    vec4 direction = camera.viewInv * vec4(normalize(target.xyz), 0.f);
  
    RayKHR ray = RayKHR(origin.xyz, direction.xyz);
    vec3 radiance = PathTrace(ray);

    return radiance;
}
