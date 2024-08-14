#ifndef SCENE_DATA_H
#define SCENE_DATA_H

#ifndef INCLUDE_FROM_SHADER
#include <cstdint>
namespace data_scene {
#else
#endif

struct Geometry {
    uint64_t vtxAddress;
    uint64_t idxAddress;
    uint64_t normalAddress;
    uint64_t uvAddress;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 32 };
#endif
};

#ifndef INCLUDE_FROM_SHADER
}
#else
layout (buffer_reference, scalar) buffer GeometryDescriptor { Geometry g[]; };
#endif


#endif // SCENE_DATA_H