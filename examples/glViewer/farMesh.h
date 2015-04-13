#ifndef FAR_MESH_H
#define FAR_MESH_H

#include <osd/mesh.h>
#include <osd/cpuGLVertexBuffer.h>
#include <osd/glDrawContext.h>
#include <osd/vertexDescriptor.h>
#include <common/vtr_utils.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Osd {

typedef MeshInterface<GLDrawContext> GLMeshInterface;

template <int N>
class FarMesh : public GLMeshInterface {
public:
    typedef CpuGLVertexBuffer VertexBuffer;
    typedef GLDrawContext DrawContext;
    typedef DrawContext::VertexBufferBinding VertexBufferBinding;

    struct Vertex {
        void Clear() {
            for (int i = 0; i < N; ++i) { value[i] = 0.0f; }
        }
        void AddWithWeight(Vertex const &v, float w) {
            for (int i = 0; i < N; ++i) {
                value[i] += v.value[i] * w;
            }
        }

        void ApplyVertexEdit(float x, float y, float z) {
            value[0] += x;
            value[1] += y;
            value[2] += z;
        }

        float value[N];
    };

    FarMesh(Far::TopologyRefiner *refiner,
            int numVertexElements,
            int numVaryingElements,
            int level,
            MeshBitset bits) :

            _refiner(refiner),
            _patchTables(0),
            _vertexBuffer(0),
            _varyingBuffer(0),
            _drawContext(0)
    {

        if (bits.test(MeshAdaptive)) {
            Far::TopologyRefiner::AdaptiveOptions options(level);
            options.useSingleCreasePatch = bits.test(MeshUseSingleCreasePatch);

            _refiner->RefineAdaptive(options);
        } else {
            //  This dependency on FVar channels should not be necessary
            bool fullTopologyInLastLevel = refiner->GetNumFVarChannels()>0;

            Far::TopologyRefiner::UniformOptions options(level);
            options.fullTopologyInLastLevel = fullTopologyInLastLevel;
            _refiner->RefineUniform(options);
        }


        int numVertexElementsInterleaved = numVertexElements +
            (bits.test(MeshInterleaveVarying) ? numVaryingElements : 0);
        int numVaryingElementsNonInterleaved = 
            (bits.test(MeshInterleaveVarying) ? 0 : numVaryingElements);

        initializeContext(numVertexElements, numVaryingElements,
                          numVertexElementsInterleaved, level, bits);

        int numVertices = GetNumVertices();//GLMeshInterface::getNumVertices(*_refiner);

        // FIXME: need a better API for numTotalVertices.
        if (_patchTables->GetEndCapVertexStencilTables()) {
            numVertices += _patchTables->GetEndCapVertexStencilTables()->GetNumStencils();
        }

        initializeVertexBuffers(numVertices,
                                numVertexElementsInterleaved,
                                numVaryingElementsNonInterleaved);

        // will retire soon
        _drawContext->UpdateVertexTexture(_vertexBuffer);
    }

    virtual ~FarMesh() {
        delete _refiner;
        delete _patchTables;
        delete _vertexBuffer;
        delete _varyingBuffer;
        delete _drawContext;
        delete _vertexStencils;
        delete _varyingStencils;
    }

    virtual int GetNumVertices() const {
        assert(_refiner);

        return _refiner->IsUniform() ?
            _refiner->GetNumVertices(0) + _vertexStencils->GetNumStencils() :
                _refiner->GetNumVerticesTotal();

    }


    virtual void UpdateVertexBuffer(float const *vertexData, int startVertex, int numVerts) {
        _vertexBuffer->UpdateData(vertexData, startVertex, numVerts);
    }

    virtual void UpdateVaryingBuffer(float const *varyingData, int startVertex, int numVerts) {
        _varyingBuffer->UpdateData(varyingData, startVertex, numVerts);
    }

    virtual void Refine() {
        Vertex *verts = (Vertex *)_vertexBuffer->BindCpuBuffer();
        int ncoarseverts = _refiner->GetNumVertices(0);
        if (false) {
            _vertexStencils->UpdateValues(verts, verts + ncoarseverts);
        } else {
            int firstVertex = ncoarseverts;
            int maxlevel = _refiner->GetMaxLevel();
            int controlVertex = 0;

            HierarchicalEditsXYZ *hedits = (HierarchicalEditsXYZ*)_refiner->GetHierarchicalEdits();

            for(int i = 1; i <= maxlevel; ++i) {
                int nVertsInLevel = _refiner->GetNumVertices(i);
                //printf("Level %d cp %d, first %d, %d\n", i, controlVertex, firstVertex, nVertsInLevel);

                _vertexStencils->UpdateValues(verts + controlVertex,
                                              verts + ncoarseverts,
                                              firstVertex-ncoarseverts,
                                              firstVertex-ncoarseverts+nVertsInLevel);

                // apply hierarchical edit
                if (hedits) {
                    int level = i;
                    for (size_t i = 0; i < hedits->vertexValueEdits.size(); ++i) {
                        OpenSubdiv::Far::HierarchicalEdits::IndexPath const &edit = hedits->vertexValueEdits[i];
                        if (level+1 != (int)edit.indicesInFace.size()) continue;

                        int vid = edit.indicesInLevel[level+1] + firstVertex;

                        verts[vid].ApplyVertexEdit(hedits->editValues[i*3],
                                                   hedits->editValues[i*3+1],
                                                   hedits->editValues[i*3+2]);
                   }
                }

                controlVertex = firstVertex;
                firstVertex += nVertsInLevel;
            }
            // apply gregory
            int nGregoryBasisVerts = _vertexStencils->GetNumStencils() - (firstVertex - ncoarseverts);
            if (nGregoryBasisVerts > 0) {
                _vertexStencils->UpdateValues(verts + controlVertex,
                                              verts + ncoarseverts,
                                              firstVertex-ncoarseverts,
                                              -1);
            }
            //printf("Stencils applied. %d + %d\n", _vertexStencils->GetNumStencils(), nGregoryBasisVerts);
        }
    }

    virtual void Refine(VertexBufferDescriptor const * vertexDesc,
                        VertexBufferDescriptor const * varyingDesc,
                        bool interleaved) {
        // not implemented.
    }

    virtual void Synchronize() {
        // not implemented.
    }

    virtual VertexBufferBinding BindVertexBuffer() {
        return _vertexBuffer->BindVBO();
    }

    virtual VertexBufferBinding BindVaryingBuffer() {
        return _varyingBuffer->BindVBO();
    }

    virtual DrawContext * GetDrawContext() {
        return _drawContext;
    }

    virtual VertexBuffer * GetVertexBuffer() {
        return _vertexBuffer;
    }

    virtual VertexBuffer * GetVaryingBuffer() {
        return _varyingBuffer;
    }

    virtual Far::TopologyRefiner const * GetTopologyRefiner() const {
        return _refiner;
    }

    virtual void SetFVarDataChannel(int fvarWidth, std::vector<float> const & fvarData) {
        if (_patchTables and _drawContext and fvarWidth and (not fvarData.empty())) {
            _drawContext->SetFVarDataTexture(*_patchTables, fvarWidth, fvarData);
        }
    }

private:

    void initializeContext(int numVertexElements,
                           int numVaryingElements,
                           int numElements, int level, MeshBitset bits) {

        assert(_refiner);

        Far::StencilTablesFactory::Options options;
        options.generateOffsets=true;
        //options.generateIntermediateLevels=_refiner->IsUniform() ? false : true;
        //options.generateIntermediateLevels = true;
        options.generateIntermediateLevels = true;
        options.factorizeIntermediateLevels = false;

        Far::StencilTables const * vertexStencils=0, * varyingStencils=0;

        if (numVertexElements>0) {

            vertexStencils = Far::StencilTablesFactory::Create(*_refiner, options);

        }

        if (numVaryingElements>0) {

            options.interpolationMode = Far::StencilTablesFactory::INTERPOLATE_VARYING;

            varyingStencils = Far::StencilTablesFactory::Create(*_refiner, options);
        }

        assert(_refiner);
        Far::PatchTablesFactory::Options poptions(level);
        poptions.generateFVarTables = bits.test(MeshFVarData);
        poptions.generateAllLevels = false;
        poptions.skipIntermediateLevels = false;
        poptions.useSingleCreasePatch = bits.test(MeshUseSingleCreasePatch);

        if (bits.test(MeshUseGregoryBasis)) {
            poptions.adaptiveStencilTables = vertexStencils;
            poptions.adaptiveVaryingStencilTables = varyingStencils;
        }

        _patchTables = Far::PatchTablesFactory::Create(*_refiner, poptions);

        _drawContext = DrawContext::Create(_patchTables, numElements);

        // XXX: factory API fix needed
        // merge greogry basis stencils
        Far::StencilTables const * endCapVertexStencils =
            _patchTables->GetEndCapVertexStencilTables();

        if (endCapVertexStencils) {
            Far::StencilTables const * endCapVaryingStencils =
                _patchTables->GetEndCapVaryingStencilTables();

            // concatinate vertexStencils and endCapStencils.
            // note that endCapStensils is owned by patchTable.
            Far::StencilTables const *inStencils[] = {
                vertexStencils, endCapVertexStencils
            };
            Far::StencilTables const *concatStencils =
                Far::StencilTablesFactory::Create(2, inStencils);

            Far::StencilTables const *inVaryingStencils[] = {
                varyingStencils, endCapVaryingStencils
            };
            Far::StencilTables const *concatVaryingStencils =
                Far::StencilTablesFactory::Create(2, inVaryingStencils);

            delete vertexStencils;
            vertexStencils = concatStencils;
            delete varyingStencils;
            varyingStencils = concatVaryingStencils;
        }
        _vertexStencils = vertexStencils;
        _varyingStencils = varyingStencils;
    }

    void initializeVertexBuffers(int numVertices,
                                 int numVertexElements,
                                 int numVaryingElements) {

        if (numVertexElements) {
            _vertexBuffer = VertexBuffer::Create(numVertexElements, numVertices);
        }

        if (numVaryingElements) {
            _varyingBuffer = VertexBuffer::Create(numVaryingElements, numVertices);
        }
   }

    Far::TopologyRefiner * _refiner;
    Far::PatchTables * _patchTables;
    Far::StencilTables const * _vertexStencils;
    Far::StencilTables const * _varyingStencils;

    VertexBuffer *_vertexBuffer;
    VertexBuffer *_varyingBuffer;


    DrawContext *_drawContext;
};

}
}
}

#endif  // FAR_MESH_H
