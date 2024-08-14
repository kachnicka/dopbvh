#version 450 core

layout(set=0, binding=0) uniform usampler2D stencilTexture;

layout(push_constant) uniform uPushConstant {
    float alpha;
    float scale;
} pc;

layout(location = 0) in struct {
    vec2 UV;
} In;

layout(location = 0) out vec4 outColor;

float fade(float low, float high, float value)
{
  float mid   = (low + high) * 0.5;
  float range = (high - low) * 0.5;
  float x     = 1.0 - clamp(abs(mid - value) / range, 0.0, 1.0);
  return smoothstep(0.0, 1.0, x);
}

vec3 temperature(float intensity)
{
  const vec3 blue   = vec3(0.0, 0.0, 1.0);
  const vec3 cyan   = vec3(0.0, 1.0, 1.0);
  const vec3 green  = vec3(0.0, 1.0, 0.0);
  const vec3 yellow = vec3(1.0, 1.0, 0.0);
  const vec3 red    = vec3(1.0, 0.0, 0.0);

  vec3 color = (fade(-0.25, 0.25, intensity) * blue    //
                + fade(0.0, 0.5, intensity) * cyan     //
                + fade(0.25, 0.75, intensity) * green  //
                + fade(0.5, 1.0, intensity) * yellow   //
                + smoothstep(0.75, 1.0, intensity) * red);
  return color;
}

void main()
{
    uint s = texture(stencilTexture, In.UV).x;
    
    if (s == 0)
        outColor = vec4(0.f);
    else {
        float val = s / pc.scale;
        outColor = vec4(temperature(val), pc.alpha);
    }
}
