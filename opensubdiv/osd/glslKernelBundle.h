//
//     Copyright (C) Pixar. All rights reserved.
//
//     This license governs use of the accompanying software. If you
//     use the software, you accept this license. If you do not accept
//     the license, do not use the software.
//
//     1. Definitions
//     The terms "reproduce," "reproduction," "derivative works," and
//     "distribution" have the same meaning here as under U.S.
//     copyright law.  A "contribution" is the original software, or
//     any additions or changes to the software.
//     A "contributor" is any person or entity that distributes its
//     contribution under this license.
//     "Licensed patents" are a contributor's patent claims that read
//     directly on its contribution.
//
//     2. Grant of Rights
//     (A) Copyright Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free copyright license to reproduce its contribution,
//     prepare derivative works of its contribution, and distribute
//     its contribution or any derivative works that you create.
//     (B) Patent Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free license under its licensed patents to make, have
//     made, use, sell, offer for sale, import, and/or otherwise
//     dispose of its contribution in the software or derivative works
//     of the contribution in the software.
//
//     3. Conditions and Limitations
//     (A) No Trademark License- This license does not grant you
//     rights to use any contributor's name, logo, or trademarks.
//     (B) If you bring a patent claim against any contributor over
//     patents that you claim are infringed by the software, your
//     patent license from such contributor to the software ends
//     automatically.
//     (C) If you distribute any portion of the software, you must
//     retain all copyright, patent, trademark, and attribution
//     notices that are present in the software.
//     (D) If you distribute any portion of the software in source
//     code form, you may do so only under this license by including a
//     complete copy of this license with your distribution. If you
//     distribute any portion of the software in compiled or object
//     code form, you may only do so under a license that complies
//     with this license.
//     (E) The software is licensed "as-is." You bear the risk of
//     using it. The contributors give no express warranties,
//     guarantees or conditions. You may have additional consumer
//     rights under your local laws which this license cannot change.
//     To the extent permitted under your local laws, the contributors
//     exclude the implied warranties of merchantability, fitness for
//     a particular purpose and non-infringement.
//

#ifndef OSD_GLSL_COMPUTE_KERNEL_BUNDLE_H
#define OSD_GLSL_COMPUTE_KERNEL_BUNDLE_H

#if not defined(__APPLE__)
    #if defined(_WIN32)
        #include <windows.h>
    #endif
    #include <GL/gl.h>
#else
    #include <OpenGL/gl3.h>
#endif

#include "../version.h"

#include "../osd/computeContext.h"
#include "../osd/nonCopyable.h"

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

    class OsdGLSLComputeKernelBundle
        : OsdNonCopyable<OsdGLSLComputeKernelBundle> {
    public:
        OsdGLSLComputeKernelBundle();
        ~OsdGLSLComputeKernelBundle();

        bool Compile(int numVertexElements, int numVaryingElements);

        void ApplyBilinearFaceVerticesKernel(
            int F_IT_ofs, int F_ITa_ofs, int offset, int start, int end);

        void ApplyBilinearEdgeVerticesKernel(
            int E_IT_ofs, int offset, int start, int end);

        void ApplyBilinearVertexVerticesKernel(
            int V_ITa_ofs, int offset, int start, int end);

        void ApplyCatmarkFaceVerticesKernel(
            int F_IT_ofs, int F_ITa_ofs, int offset, int start, int end);

        void ApplyCatmarkEdgeVerticesKernel(
            int E_IT_ofs, int E_W_ofs, int offset, int start, int end);

        void ApplyCatmarkVertexVerticesKernelB(
            int V_IT_ofs, int V_ITa_ofs, int V_W_ofs,
            int offset, int start, int end);

        void ApplyCatmarkVertexVerticesKernelA(
            int V_ITa_ofs, int V_W_ofs,
            int offset, bool pass, int start, int end);

        void ApplyLoopEdgeVerticesKernel(
            int E_IT_ofs, int E_W_ofs, int offset, int start, int end);

        void ApplyLoopVertexVerticesKernelB(
            int V_IT_ofs, int V_ITa_ofs, int V_W_ofs,
            int offset, int start, int end);

        void ApplyLoopVertexVerticesKernelA(
            int V_ITa_ofs, int V_W_ofs,
            int offset, bool pass, int start, int end);

        void ApplyEditAdd(int numEditVertices,
                          int editIndices_ofs, int editValues_ofs,
                          int primvarOffset, int primvarWidth);

        void UseProgram() const;

        GLuint GetTableUniformLocation(int tableIndex) const {
            return _tableUniforms[tableIndex];
        }

        struct Match {
        Match(int numVertexElements, int numVaryingElements) :
            _numVertexElements(numVertexElements),
                _numVaryingElements(numVaryingElements) {}
            bool operator() (const OsdGLSLComputeKernelBundle *kernel) {
                return (kernel->_numVertexElements == _numVertexElements
                        && kernel->_numVaryingElements == _numVaryingElements);
            }
            int _numVertexElements, _numVaryingElements;
        };

        friend struct Match;

    protected:
        void dispatchCompute(GLint offset, int start, int end) const;

        GLuint _program;

        // uniform locations for compute
        GLuint _tableUniforms[Table::TABLE_MAX];
        GLuint _tableOffsetUniforms[Table::TABLE_MAX];
        GLuint _uniformVertexPass;
        GLuint _uniformIndexStart;
        GLuint _uniformIndexEnd;
        GLuint _uniformIndexOffset;

        // uniform locations for vertex edit
        GLuint _uniformEditIndicesOffset;
        GLuint _uniformEditValuesOffset;
        GLuint _uniformEditNumVertices;
        GLuint _uniformEditPrimVarOffset;
        GLuint _uniformEditPrimVarWidth;

        // general face-vertex kernel (all schemes)
        GLuint _subComputeFace;
        // edge-vertex kernel (catmark + loop schemes)
        GLuint _subComputeEdge;
        // edge-vertex kernel (bilinear scheme)
        GLuint _subComputeBilinearEdge;
        // vertex-vertex kernel (bilinear scheme)
        GLuint _subComputeVertex;
        // vertex-vertex kernel A (catmark + loop schemes)
        GLuint _subComputeVertexA;
        // vertex-vertex kernel B (catmark scheme)
        GLuint _subComputeCatmarkVertexB;
        // vertex-vertex kernel B (loop scheme)
        GLuint _subComputeLoopVertexB;
        // hedit kernel (add)
        GLuint _subEditAdd;

        int _numVertexElements,
            _numVaryingElements;

        int _workGroupSize;
    };

}  // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

}  // end namespace OpenSubdiv

#endif  // OSD_GLSL_COMPUTE_KERNEL_BUNDLE_H
