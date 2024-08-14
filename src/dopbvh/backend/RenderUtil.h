#pragma once

#include <vLime/types.h>

namespace backend {

enum class Format {
    eR8G8B8A8Unorm,
    eR32G32B32A32Sfloat,
};

struct Extent {
    u32 x { 0 };
    u32 y { 0 };

    bool set(u32 xNew, u32 yNew)
    {
        if ((x == xNew) && (y == yNew))
            return false;
        x = xNew;
        y = yNew;
        return true;
    }
};

namespace gui {

struct TextureID {
    enum {
        defaultTexture = 0,
        font = 1,
        renderedScene = 2,
        dopVisualizer = 3,
    };
};

}

}
