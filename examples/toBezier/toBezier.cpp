

#include <far/topologyRefinerFactory.h>
#include <far/stencilTableFactory.h>
#include <far/patchTableFactory.h>
#include "../../regression/common/shape_utils.h"
#include "../../regression/shapes/catmark_tet.h"
#include "../../regression/shapes/catmark_car.h"
#include "../../regression/common/far_utils.h"
#include <string.h>
#include <sstream>
#include <float.h>
#include <algorithm>

using namespace OpenSubdiv;

struct Point {
    void Clear() {
        val[0] = val[1] = val[2] = 0;
    }
    void AddWithWeight(Point const &p, float w) {
        val[0] += w*p.val[0];
        val[1] += w*p.val[1];
        val[2] += w*p.val[2];
    }
    float &operator[](int i) {
        return val[i];
    }
    const float &operator[](int i) const {
        return val[i];
    }
    float val[3];
};

std::ostream &operator<<(std::ostream &os, Point const &P){
    //os << P[0] << ", " << P[1] << ", " << P[2] << " ";
    os << P[0] << ", " << P[2] << ", " << -P[1] << " ";
    return os;
}


std::string
toBezier(std::string const &obj, int level)
{
    Shape const * shape = 0;
    shape = Shape::parseObj(obj.c_str(), kCatmark, false);

    Sdc::SchemeType sdctype = GetSdcType(*shape);
    Sdc::Options sdcoptions = GetSdcOptions(*shape);

    Far::TopologyRefiner * refiner =
        Far::TopologyRefinerFactory<Shape>::Create(*shape,
            Far::TopologyRefinerFactory<Shape>::Options(sdctype, sdcoptions));

    Far::TopologyRefiner::AdaptiveOptions options(level);
    refiner->RefineAdaptive(options);

    Far::StencilTableFactory::Options soptions;
    soptions.generateOffsets = true;
    soptions.generateIntermediateLevels = true;
    Far::StencilTable const *stencilTable = Far::StencilTableFactory::Create(*refiner, soptions);

    Far::PatchTableFactory::Options poptions(level);
    poptions.SetEndCapType(Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS);
    Far::PatchTable const *patchTable = Far::PatchTableFactory::Create(*refiner, poptions);

    if (Far::StencilTable const *vertexStencilsWithLocalPoints =
        Far::StencilTableFactory::AppendLocalPointStencilTable(
            *refiner, stencilTable, patchTable->GetLocalPointStencilTable())) {
        delete stencilTable;
        stencilTable = vertexStencilsWithLocalPoints;
    }

    int numControlVertices = stencilTable->GetNumControlVertices();
    int numVertices = numControlVertices + stencilTable->GetNumStencils();
    std::vector<Point> vertexBuffer(numVertices);

    // fill coarse verts
    memcpy(&vertexBuffer[0], &shape->verts[0], numVertices);

    // centering
    float min[3] = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    float max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    float center[3];
    for (int i = 0; i < numControlVertices; ++i) {
        for(int j=0; j<3; ++j) {
            min[j] = std::min(min[j], vertexBuffer[i][j]);
            max[j] = std::max(max[j], vertexBuffer[i][j]);
        }
    }
    for(int j=0; j<3; ++j) {
        center[j] = (min[j]+max[j])*0.5;
    }
    for (int i = 0; i < numControlVertices; ++i) {
        for(int j=0; j<3; ++j) {
            vertexBuffer[i][j] -= center[j];
        }
    }

    // refine
    stencilTable->UpdateValues(&vertexBuffer[0], &vertexBuffer[numControlVertices]);

    std::stringstream ss;
    // for (int i = 0; i < numVertices; ++i) {
    //     ss << vertexBuffer[i].x;
    //     ss << vertexBuffer[i].y;
    //     ss << vertexBuffer[i].z;
    // }

    ss << "[";
    for (int array=0; array<(int)patchTable->GetNumPatchArrays(); ++array) {
        Far::PatchDescriptor desc = patchTable->GetPatchArrayDescriptor(array);
        if (desc.GetType() != Far::PatchDescriptor::REGULAR) continue;
        int numPatches = patchTable->GetNumPatches(array);
        for (int patch = 0 ; patch < numPatches; ++patch) {
            Far::ConstIndexArray indices = patchTable->GetPatchVertices(array, patch);
            // convert to bezier
            float Q[16] = {1.f/6.f, 4.f/6.f, 1.f/6.f, 0.f,
                           0.f,     4.f/6.f, 2.f/6.f, 0.f,
                           0.f,     2.f/6.f, 4.f/6.f, 0.f,
                           0.f,     1.f/6.f, 4.f/6.f, 1.f/6.f};

            // TODO: boundary, single-crease
            Point bp[16];
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    Point H[4];
                    for (int l = 0; l < 4; ++l) {
                        H[l].Clear();
                        for (int k = 0; k < 4; ++k) {
                            H[l].AddWithWeight(vertexBuffer[indices[l*4+k]], Q[i*4+k]);
                        }
                    }
                    bp[i*4+j].Clear();
                    for (int k=0; k<4; ++k) {
                        bp[i*4+j].AddWithWeight(H[k], Q[j*4+k]);
                    }
                }
            }
            for (int i = 0; i < 16; ++i) {
                if (patch != 0 || i != 0) ss << ", ";
                ss << bp[i];
            }
        }
    }
    ss << "]\n";


    delete stencilTable;
    delete patchTable;
    delete refiner;
    delete shape;

    return ss.str();
}

std::string result;
extern "C" {

const char* toBezier(int level)
{
    //    printf("%s\n", catmark_tet.c_str());
    result = toBezier(catmark_tet, level);
    //std::string str = toBezier(catmark_car, 1);
    return result.c_str();
}
}
