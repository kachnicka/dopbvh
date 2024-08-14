#pragma once

namespace image {

 void write(char const *filename, int x, int y, int comp, const void *data, int stride_bytes);

}