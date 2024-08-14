#ifndef BVH_COMPRESSED_BINARY_GLSL
#define BVH_COMPRESSED_BINARY_GLSL 1

void setC0(inout NodeBvhBinaryCompressed node, in int32_t c0Id, in Aabb c0Box)
{
    node.c0 = c0Id;
    node.bv[0] = c0Box.min.x;
    node.bv[1] = c0Box.max.x;
    node.bv[2] = c0Box.min.y;
    node.bv[3] = c0Box.max.y;
    node.bv[8] = c0Box.min.z;
    node.bv[9] = c0Box.max.z;
}

void setC1(inout NodeBvhBinaryCompressed node, in int32_t c1Id, in Aabb c1Box)
{
    node.c1 = c1Id;
    node.bv[4] = c1Box.min.x;
    node.bv[5] = c1Box.max.x;
    node.bv[6] = c1Box.min.y;
    node.bv[7] = c1Box.max.y;
    node.bv[10] = c1Box.min.z;
    node.bv[11] = c1Box.max.z;
}

void setBoxC0(inout NodeBvhBinaryCompressed node, in Aabb c0Box)
{
    node.bv[0] = c0Box.min.x;
    node.bv[1] = c0Box.max.x;
    node.bv[2] = c0Box.min.y;
    node.bv[3] = c0Box.max.y;
    node.bv[8] = c0Box.min.z;
    node.bv[9] = c0Box.max.z;
}

void setBoxC1(inout NodeBvhBinaryCompressed node, in Aabb c1Box)
{
    node.bv[4] = c1Box.min.x;
    node.bv[5] = c1Box.max.x;
    node.bv[6] = c1Box.min.y;
    node.bv[7] = c1Box.max.y;
    node.bv[10] = c1Box.min.z;
    node.bv[11] = c1Box.max.z;
}

Aabb getBoxC0(in NodeBvhBinaryCompressed node)
{
    return Aabb(vec3(node.bv[0], node.bv[2], node.bv[8]),
                vec3(node.bv[1], node.bv[3], node.bv[9]));
}

Aabb getBoxC1(in NodeBvhBinaryCompressed node)
{
    return Aabb(vec3(node.bv[4], node.bv[6], node.bv[10]),
                vec3(node.bv[5], node.bv[7], node.bv[11]));
}

Aabb getBox(in NodeBvhBinaryCompressed node)
{
    return Aabb(vec3(min(node.bv[0], node.bv[4]),
                     min(node.bv[2], node.bv[6]),
                     min(node.bv[8], node.bv[10])),
                vec3(max(node.bv[1], node.bv[5]),
                     max(node.bv[3], node.bv[7]),
                     max(node.bv[9], node.bv[11])));
}

// interleaved c0|c1 layout
// bv[0] = c0[0]
// bv[1] = c0[1]
// bv[2] = c1[0]
// bv[3] = c1[1]
// bv[4] = c0[2]
// bv[5] = c0[3]
// bv[6] = c1[2]
// bv[7] = c1[3]
// ...

void setBoxC0(inout NodeBvhBinaryDOP14Compressed node, in float c0Dop[14])
{
    node.bv[0] = c0Dop[0];
    node.bv[1] = c0Dop[1];
    node.bv[4] = c0Dop[2];
    node.bv[5] = c0Dop[3];
    node.bv[8] = c0Dop[4];
    node.bv[9] = c0Dop[5];
    node.bv[12] = c0Dop[6];
    node.bv[13] = c0Dop[7];
    node.bv[16] = c0Dop[8];
    node.bv[17] = c0Dop[9];
    node.bv[20] = c0Dop[10];
    node.bv[21] = c0Dop[11];
    node.bv[24] = c0Dop[12];
    node.bv[25] = c0Dop[13];
}

void setBoxC1(inout NodeBvhBinaryDOP14Compressed node, in float c1Dop[14])
{
    node.bv[2] = c1Dop[0];
    node.bv[3] = c1Dop[1];
    node.bv[6] = c1Dop[2];
    node.bv[7] = c1Dop[3];
    node.bv[10] = c1Dop[4];
    node.bv[11] = c1Dop[5];
    node.bv[14] = c1Dop[6];
    node.bv[15] = c1Dop[7];
    node.bv[18] = c1Dop[8];
    node.bv[19] = c1Dop[9];
    node.bv[22] = c1Dop[10];
    node.bv[23] = c1Dop[11];
    node.bv[26] = c1Dop[12];
    node.bv[27] = c1Dop[13];
}

void setBoxC0(inout NodeBvhBinaryCompressed node, in float[14] c0Dop)
{
    node.bv[0] = c0Dop[0];
    node.bv[1] = c0Dop[1];
    node.bv[2] = c0Dop[2];
    node.bv[3] = c0Dop[3];
    node.bv[8] = c0Dop[4];
    node.bv[9] = c0Dop[5];
}

void setBoxC1(inout NodeBvhBinaryCompressed node, in float[14] c1Dop)
{
    node.bv[4] = c1Dop[0];
    node.bv[5] = c1Dop[1];
    node.bv[6] = c1Dop[2];
    node.bv[7] = c1Dop[3];
    node.bv[10] = c1Dop[4];
    node.bv[11] = c1Dop[5];
}

void setBoxC0(inout NodeBvhBinaryDOP14Compressed_SPLIT node, in float[14] c0Dop)
{
    node.bv[0] = c0Dop[6];
    node.bv[1] = c0Dop[7];
    node.bv[2] = c0Dop[8];
    node.bv[3] = c0Dop[9];
    node.bv[4] = c0Dop[10];
    node.bv[5] = c0Dop[11];
    node.bv[6] = c0Dop[12];
    node.bv[7] = c0Dop[13];
}

void setBoxC1(inout NodeBvhBinaryDOP14Compressed_SPLIT node, in float[14] c1Dop)
{
    node.bv[8] = c1Dop[6];
    node.bv[9] = c1Dop[7];
    node.bv[10] = c1Dop[8];
    node.bv[11] = c1Dop[9];
    node.bv[12] = c1Dop[10];
    node.bv[13] = c1Dop[11];
    node.bv[14] = c1Dop[12];
    node.bv[15] = c1Dop[13];
}

float[14] getBoxC0(in NodeBvhBinaryCompressed node, in NodeBvhBinaryDOP14Compressed_SPLIT nodeSplit)
{
    return float[14](
        node.bv[0], node.bv[1],
        node.bv[2], node.bv[3],
        node.bv[8], node.bv[9],
        nodeSplit.bv[0], nodeSplit.bv[1],
        nodeSplit.bv[2], nodeSplit.bv[3],
        nodeSplit.bv[4], nodeSplit.bv[5],
        nodeSplit.bv[6], nodeSplit.bv[7]
    );
}

float[14] getBoxC1(in NodeBvhBinaryCompressed node, in NodeBvhBinaryDOP14Compressed_SPLIT nodeSplit)
{
    return float[14](
        node.bv[4], node.bv[5],
        node.bv[6], node.bv[7],
        node.bv[10], node.bv[11],
        nodeSplit.bv[8], nodeSplit.bv[9],
        nodeSplit.bv[10], nodeSplit.bv[11],
        nodeSplit.bv[12], nodeSplit.bv[13],
        nodeSplit.bv[14], nodeSplit.bv[15]
    );
}

float[14] getBoxC0(in NodeBvhBinaryDOP14Compressed node)
{
    return float[14](
        node.bv[0], node.bv[1],
        node.bv[4], node.bv[5],
        node.bv[8], node.bv[9],
        node.bv[12], node.bv[13],
        node.bv[16], node.bv[17],
        node.bv[20], node.bv[21],
        node.bv[24], node.bv[25]
    );
}

float[14] getBoxC1(in NodeBvhBinaryDOP14Compressed node)
{
    return float[14](
        node.bv[2], node.bv[3],
        node.bv[6], node.bv[7],
        node.bv[10], node.bv[11],
        node.bv[14], node.bv[15],
        node.bv[18], node.bv[19],
        node.bv[22], node.bv[23],
        node.bv[26], node.bv[27]
    );
}

float[14] getBox(in NodeBvhBinaryDOP14Compressed node)
{
    return float[14](
        min(node.bv[0], node.bv[2]),
        max(node.bv[1], node.bv[3]),
        min(node.bv[4], node.bv[6]),
        max(node.bv[5], node.bv[7]),
        min(node.bv[8], node.bv[10]),
        max(node.bv[9], node.bv[11]),
        min(node.bv[12], node.bv[14]),
        max(node.bv[13], node.bv[15]),
        min(node.bv[16], node.bv[18]),
        max(node.bv[17], node.bv[19]),
        min(node.bv[20], node.bv[22]),
        max(node.bv[21], node.bv[23]),
        min(node.bv[24], node.bv[26]),
        max(node.bv[25], node.bv[27])
    );
}

void setBoxC0(inout NodeBvhBinaryOBBCompressed node, in mat4x3 m0)
{
    node.bv[0] = m0;
}

void setBoxC1(inout NodeBvhBinaryOBBCompressed node, in mat4x3 m1)
{
    node.bv[1] = m1;
}

mat4x3 getBoxC0(in NodeBvhBinaryOBBCompressed node)
{
    return node.bv[0];
}

mat4x3 getBoxC1(in NodeBvhBinaryOBBCompressed node)
{
    return node.bv[1];
}

// TODO: this is not useful
mat4x3 getBox(in NodeBvhBinaryOBBCompressed node)
{
    return mat4x3(1.f);
}


#endif