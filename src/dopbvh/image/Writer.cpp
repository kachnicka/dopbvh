#include "Writer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_MSC_VER)
#    pragma warning(push, 0)
#else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "../util/stb_image_write.h"
#if defined(_MSC_VER)
#    pragma warning(pop)
#else
#    pragma GCC diagnostic pop
#endif

void image::write(const char * filename, int x, int y, int comp, const void * data, int stride_bytes)
{
    stbi_write_png(filename, x, y, comp, data, stride_bytes);
}
