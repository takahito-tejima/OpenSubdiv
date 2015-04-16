//
//   Copyright 2014 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#ifndef BEZIER_BEZIER_INTERSECT_H
#define BEZIER_BEZIER_INTERSECT_H

//
//   The original bezier-clipping code in this file was contributed by
//   Toru Matsuoka (@ototoi)
//

#include "bezier.h"
#include "bilinearIntersect.h"

#include <limits>
#include <cfloat>

#define USE_COARSESORT 1
#define USE_MINMAX_FAILFLAG 1

#if ENABLE_TRACE_PRINT
#define trace(...) { printf(__VA_ARGS__); }
#else
#define trace(...)
#endif

namespace OsdBezier {

template <typename REAL>
struct Epsilon {
    static REAL GetEps() { return 1e-4; }
    static REAL GetUvEps() { return 1.0/32.0; }
};
template <>
struct Epsilon<double> {
    static double GetEps() { return 1e-4; }
    static double GetUvEps() { return 1.0/32.0; }
};

template<class VALUE_TYPE, class REAL, int N=4>
class BezierPatchIntersection {
public:
    typedef VALUE_TYPE ValueType;
    typedef REAL Real;
    typedef BezierPatch<ValueType, Real, N> PatchType;

    static const int  DEFAULT_MAX_LEVEL = 10;

    struct RangeAABB {
        Real tmin, tmax;
    };
    struct UVT {
        UVT() : u(0), v(0), t(0), level(0) { }
        Real u, v, t;
        int level;
        int failFlag;
    };

    BezierPatchIntersection(PatchType const &patch) NO_INLINE :
        _patch(patch),
        _eps(Epsilon<Real>::GetEps()),
        _maxLevel(DEFAULT_MAX_LEVEL),
        _uvMargin(true),
        _cropUV(true), _useBezierClip(true),
        _useTriangle(false),
        _directBilinear(false),
        _wcpFlag(0)
    {

        _uRange[0] = _vRange[0] = 0;
        _uRange[1] = _vRange[1] = 1;

        _count = 0;

        patch.GetMinMax(_min, _max, Real(1e-3));
    }
    ~BezierPatchIntersection() { }

    void SetEpsilon(REAL eps){
        _eps = std::max(std::numeric_limits<REAL>::epsilon()*REAL(1e+1), eps);
    }
    void SetMaxLevel(int maxLevel){
        _maxLevel = maxLevel;
    }
    void SetUVMergin(REAL uvMargin){
        _uvMargin = uvMargin;
    }
    void SetCropUV(bool cropUV){
        _cropUV = cropUV;
    }
    void SetUseBezierClip(bool useBezierClip){
        _useBezierClip = useBezierClip;
    }
    void SetUseTriangle(bool useTriangle){
        _useTriangle = useTriangle;
    }
    void SetDirectBilinear(bool directBilinear){
        _directBilinear = directBilinear;
    }
    void SetWatertightFlag(int wcpFlag) {
        _wcpFlag = wcpFlag;
    }

    template<typename RAY, typename INTERSECTION>
    NO_INLINE bool Test(INTERSECTION* info, RAY const &r, Real tmin, Real tmax) {

        RangeAABB rng;
        if (intersectAABB(&rng, _min, _max, r, tmin, tmax)) {
            tmin = std::max(tmin, rng.tmin);
            tmax = std::min(tmax, rng.tmax);

            return testInternal(info, r, tmin, tmax);
        }
        return false;
    }

    template<typename RAY>
    Real ComputeEpsilon(RAY const & r, Real eps)const
    {
        RangeAABB rng;
        if(r.hasDifferential &&
           intersectAABB(&rng, _min, _max, r, 0, std::numeric_limits<Real>::max()))
        {
            Real t = std::max(Real(0), rng.tmin);
            //ValueType org(r.org[0], r.org[1], r.org[2]);
            ValueType dir(r.dir[0], r.dir[1], r.dir[2]);
            ValueType dirDX = (dir + ValueType(r.dDdx[0],r.dDdx[1],r.dDdx[2]));
            ValueType dirDY = (dir + ValueType(r.dDdy[0],r.dDdy[1],r.dDdy[2]));
            dirDX.normalize();
            dirDY.normalize();
            ValueType P0 = t * dir;
            ValueType PX = t * dirDX;
            ValueType PY = t * dirDY;

            Real lx = (PX-P0).length();
            Real ly = (PY-P0).length();

            eps = std::min(lx,ly)*Real(0.25);
        }
        return eps;
    }

protected:
    template<typename RAY, typename INTERSECTION>
    NO_INLINE bool testInternal(INTERSECTION* info, const RAY& r, Real tmin, Real tmax) {
        typename ValueType::Matrix4Type mat(ValueType(r.org), ValueType(r.dir)); // getZAlign

        bool bRet = false;
        UVT uvt;
        uvt.failFlag = 0;
        {
            PatchType patch(_patch, mat);
            if (testBezierPatch(&uvt, patch, tmin, tmax, _eps)) {
                Real t = uvt.t;
                Real u = uvt.u;
                Real v = uvt.v;

                u = _uRange[0]*(1-u) + _uRange[1]*u;//global
                v = _vRange[0]*(1-v) + _vRange[1]*v;//global
                info->t = t;
                info->u = u;
                info->v = v;
                info->clipLevel = uvt.level;
                {
                    ValueType du = _patch.EvaluateDu(u,v);
                    ValueType dv = _patch.EvaluateDv(u,v);
                    ValueType normal = cross(du,dv);
                    normal.normalize();
                    info->normal = normal;
                    info->geometricNormal = normal;
                    //  info->tangent  = Conv(U);
                    //  info->binormal = Conv(V);
                }
                tmax = t;
                bRet = true;
                trace("hit t = %f, uv = (%f, %f)\n", t, u, v);
            }
        }

        if (uvt.failFlag == 0 or _wcpFlag == 0) return bRet;

        {
            // TODO: still inefficient. wcpFlag knows which edge has to be split
            // and failFlag knows which edge is actually being tested.

            // test against split faces too
            /*
              +---+---+
           ^  | 2 | 3 |
           |  +---+---+
           |  | 0 | 1 |
           u  +---+---+
                v---->
             */
            static Real offsets[8] = {0.0,0.0, 0.0,0.5, 0.5,0.0, 0.5,0.5};
            PatchType tmp[2];
            PatchType children[4];
            _patch.SplitU(tmp, 0.5);
            tmp[0].SplitV(&children[0], 0.5);
            tmp[1].SplitV(&children[2], 0.5);

            int ni = -1;
            for (int i = 0; i < 4; ++i) {
                children[i].Transform(mat);
                trace("Child pass %d\n", i);
                if (testBezierPatch(&uvt, children[i], tmin, tmax, _eps)) {
                    Real t = uvt.t;
                    tmax = t;
                    ni = i;
                }
            }

            if(0<=ni){
                int i = ni;
                Real t = uvt.t;
                Real u = uvt.u * 0.5 + offsets[2*i+0];
                Real v = uvt.v * 0.5 + offsets[2*i+1];

                u = _uRange[0]*(1-u) + _uRange[1]*u;//global
                v = _vRange[0]*(1-v) + _vRange[1]*v;//global
                info->t = t;
                info->u = u;
                info->v = v;
                info->clipLevel = uvt.level;
                {
                    ValueType du = _patch.EvaluateDu(u,v);
                    ValueType dv = _patch.EvaluateDv(u,v);
                    ValueType normal = cross(du,dv);
                    normal.normalize();
                    info->normal = normal;
                    info->geometricNormal = normal;
                    //                info->tangent  = Conv(U);
                    //                info->binormal = Conv(V);
                }
                bRet = true;
                trace("hit t = %f, uv = (%f, %f)\n", t, u, v);
            }
        }
        if (_count > 1000) return false;
        return bRet;
    }
    bool testBezierPatch(UVT* info, PatchType const & patch, Real zmin, Real zmax, Real eps) NO_INLINE {
        ValueType min, max;

        patch.GetMinMax(min, max, eps*1e-3);

        if (0 < min[0] || max[0] < 0) return false;//x
        if (0 < min[1] || max[1] < 0) return false;//y
        if (max[2] < zmin || zmax < min[2]) return false;//z

        if (_cropUV) return testBezierClipRangeU(info, patch, 0, 1, 0, 1, zmin, zmax, 0, _maxLevel, eps);
        else         return testBezierClipU     (info, patch, 0, 1, 0, 1, zmin, zmax, 0, _maxLevel, eps);
    }

    // ----------------------------------------------------------------------
    static void rotateU(PatchType& patch) NO_INLINE
    {
        ValueType dx = patch.GetLv();
        typename ValueType::Matrix3Type rot(dx);
        patch.Transform(rot);
    }

    static void rotateV(PatchType& patch) NO_INLINE
    {
        ValueType dx = patch.GetLu();
        typename ValueType::Matrix3Type rot(dx);
        patch.Transform(rot);
    }

    static bool isEps(ValueType const & min, ValueType const & max, Real eps) {
        //return ((max[1]-min[1])<=eps);
        REAL xw = max[0]-min[0];
        REAL yw = max[1]-min[1];
        return (xw<=eps && yw<=eps);
    }

    static bool isLevel(int level, int max_level) {
        return (level>=max_level);
    }

    static bool isClip(int level) {
        static const int div_level[] =
        {
            1,
            1,1,
            2,2,//4
            3,3,3,3,//8
            4,4,4,4,4,4,4,4,//16
        };
        int nlevel = 0;
        if (N<=16) {
            nlevel = div_level[N];
        }else{
            nlevel = (int)ceil(log2(N));
        }
        return nlevel*2<=level;
    }
    static Real lerp(Real a, Real b, Real u) {
        Real t = Real(1) - (Real(1)-u);
        Real s = Real(1) - t;
        return s*a+t*b;
    }

    static void coarseSort(int order[2], PatchType tmp[2])
    {
#if USE_COARSESORT
        Real zs[2];
        for (int i = 0; i < 2; ++i) {
            int u = PatchType::Order>>1;
            int v = PatchType::Order>>1;

            zs[i] = tmp[i].Get(u,v)[2];
        }
        if (zs[0] <= zs[1]) {
            order[0] = 0;
            order[1] = 1;
        } else {
            order[0] = 1;
            order[1] = 0;
        }
#endif
    }

    // --------------------------------------------------------------------

    template<typename VECTOR>
    static bool scanMinMax(VECTOR const & p) {
        int nUpper = 0, nLower = 0;
        for (int i = 0; i < VECTOR::LENGTH; ++i) {
            if (p[i][1] > 0) nUpper++;
            else nLower++;
            if (nUpper && nLower) return true;
        }
        return false;
    }

    template<typename T>
    static T slope(T const a[], T const b[]) {
        T d0 = b[0] - a[0];
        T d1 = b[1] - a[1];
        return fabs(d0 / d1);
    }

    template<typename T>
    static T dist(T const p0[], T const p1[]) {
        T a = fabs(p0[1]);
        T b = fabs(p1[1]);
        T h = a + b;
        T t = p0[0] + a*(p1[0] - p0[0])/h;
        return t;
    }

    template<typename T>
    struct Curve {
        typedef typename T::ElementType ElementType;
        static const int LENGTH = N;
        T operator[](int i) const { return v[i]; }
        T &operator[](int i) { return v[i]; }
        T v[N];
    };
    template <typename T>
    struct vec2t {
        typedef T ElementType;
        T operator[](int i) const { return v[i]; }
        T &operator[](int i) { return v[i]; }
        T v[2];
    };
    typedef Curve<vec2t<typename ValueType::ElementType> > VECTOR;

    static int scanConvex(Real ts[], VECTOR const & p) NO_INLINE {
        typedef typename VECTOR::ElementType Scalar;
        if (!scanMinMax(p)) {
            return 0;
        }
        int n = 0;
        {
            int k = -1;
            int l = -1;
            Scalar current = std::numeric_limits<Scalar>::max();
            for (int i = 1; i < VECTOR::LENGTH; ++i) {
                if (p[i][1] * p[0][1] < 0) {
                    Scalar s = slope(&p[i][0], &p[0][0]);
                    if (s < current) {
                        current = s;
                        k = i;
                    }
                }
            }
            if (k >= 0){
                current = 0;
                for (int i = 0; i < k; ++i) {
                    if (p[i][1] * p[k][1] < 0) {
                        Scalar s = slope(&p[i][0], &p[k][0]);
                        if (current < s) {
                            current = s;
                            l = i;
                        }
                    }
                }
                if (l >= 0){
                    ts[n++] = dist(&p[l][0], &p[k][0]);
                }
            }
        }
        {
            int k = -1;
            int l = -1;
            Scalar current = std::numeric_limits<Scalar>::max();
            for (int i = 0; i < VECTOR::LENGTH-1; ++i) {
                if (p[i][1] * p[VECTOR::LENGTH-1][1] < 0) {
                    Scalar s = slope(&p[i][0], &p[VECTOR::LENGTH-1][0]);
                    if (s < current) {
                        current = s;
                        k = i;
                    }
                }
            }
            if (k >= 0){
                current = 0;
                for (int i = k+1; i < VECTOR::LENGTH; ++i) {
                    if (p[i][1] * p[k][1] < 0) {
                        Scalar s = slope(&p[i][0], &p[k][0]);
                        if (current < s) {
                            current = s;
                            l = i;
                        }
                    }
                }
                if (l >= 0) {
                    ts[n++] = dist(&p[k][0], &p[l][0]);
                }
            }
        }
        return n;
    }

    static bool xCheck(Real rng[2], const VECTOR& curve)
    {
        Real t[4] = {0};
        int nn = scanConvex(t, curve);
        if (nn) {
            Real t0 = t[0];
            Real t1 = t[0];

            for(int i=1;i<nn;i++){
                t0 = std::min(t0, t[i]);
                t1 = std::max(t1, t[i]);
            }

            rng[0] = t0;
            rng[1] = t1;
            return true;
        }
        return false;
    }

    static bool getRangeU(Real out[2], PatchType const & patch) NO_INLINE {
        Real t0 = 1;
        Real t1 = 0;
        VECTOR curve;
        Real delta = 1.0/(N-1);
        for (int i = 0; i < N; ++i) {
            curve[i][0] = i*delta;
        }
        Real rng[2];
        for (int j = 0; j < N; ++j) {
            for (int i = 0; i < N; ++i) {
                curve[i][1] = (patch.Get(i,j))[1];
            }
            if (xCheck(rng, curve)) {
                t0 = std::min(t0, rng[0]);
                t1 = std::max(t1, rng[1]);
            } else {
                return false;
            }
        }
        out[0] = t0;
        out[1] = t1;
        return true;
    }

    static bool getRangeV(Real out[2], PatchType const &patch) NO_INLINE {
        Real t0 = 1;
        Real t1 = 0;
        VECTOR curve;
        Real delta = 1.0/(N-1);
        for (int i = 0; i < N; ++i) {
            curve[i][0] = i*delta;
        }
        Real rng[2];
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                curve[j][1] = (patch.Get(i,j))[1];
            }
            if (xCheck(rng, curve)) {
                t0 = std::min(t0,rng[0]);
                t1 = std::max(t1,rng[1]);
            }else{
                return false;
            }
        }
        out[0] = t0;
        out[1] = t1;
        return true;
    }

    bool testBezierClipU(UVT* info, PatchType const & patch,
                         Real u0, Real u1, Real v0, Real v1,
                         Real zmin, Real zmax,
                         int level, int max_level, Real eps) NO_INLINE {
        PatchType tpatch(patch);
        rotateU(tpatch);

        if (++_count > 1000 || u0 > u1 || v0 > v1) {
            return false;
        }

        trace("testBezierClipU (%f, %f) - (%f, %f) z:%f, %f  level=%d\n",
              u0, u1, v0, v1, zmin, zmax, level);

        ValueType min, max;
        tpatch.GetMinMax(min, max, eps*1e-3);
        if (0 < min[0] || max[0] < 0 || // x
            0 < min[1] || max[1] < 0 || // y
            max[2] < zmin || zmax < min[2]) { // z
#if USE_MINMAX_FAILFLAG
            // set failFlag only if it's very close
            if (fabs(min[0]) < eps ||
                fabs(max[0]) < eps ||
                fabs(min[1]) < eps ||
                fabs(max[1]) < eps) {
                int failFlag = ((u0 == Real(0)) << 0)
                    | ((u1 == Real(1)) << 1)
                    | ((v0 == Real(0)) << 2)
                    | ((v1 == Real(1)) << 3);
                info->failFlag |= failFlag;
            }
#endif
            return false;
        }
        
        bool bClip = isClip(level);
        if (bClip && (isEps(min,max,eps) || isLevel(level,max_level))){
            return testBezierClipL(info, patch, u0, u1, v0, v1, zmin, zmax, level);
        } else {
            Real tw = 1;
            Real tt0 = 0;
            Real tt1 = 1;

            if (_useBezierClip & bClip) {
                Real rng[2];
                if (getRangeU(rng, tpatch)) {
                    tt0 = rng[0];
                    tt1 = rng[1];
                    tw = tt1-tt0;
                }
            }

            if (tw>=0.4) {
                PatchType tmp[2];
                patch.SplitU(tmp, 0.5);
                Real um = (u0+u1)*0.5;
                Real ut[] = {u0,um,um,u1};

                int order[2]={0,1};
                coarseSort(order, tmp);
                bool bRet = false;
                if (ut[0] > ut[1]) std::swap(ut[0], ut[1]);
                if (ut[2] > ut[3]) std::swap(ut[2], ut[3]);
                for (int i = 0; i < 2; ++i) {
                    if (testBezierClipV(info, tmp[order[i]], ut[2*order[i]], ut[2*order[i]+1],
                                        v0, v1, zmin, zmax, level+1, max_level, eps)){
                        zmax = info->t;
                        bRet = true;
                    }
                }
                return bRet;
            } else {
                tt0 = std::max<Real>(0.0, tt0-Epsilon<Real>::GetUvEps());
                tt1 = std::min<Real>(tt1+Epsilon<Real>::GetUvEps(), 1.0);
                Real ut[] = {lerp(u0,u1,tt0),lerp(u0,u1,tt1)};
                PatchType tmp;
                patch.CropU(tmp, tt0, tt1);
                return testBezierClipV(info, tmp, ut[0], ut[1],
                                       v0, v1, zmin, zmax, level+1, max_level, eps);
            }
        }
        return false;
    }

    bool testBezierClipV(UVT *info, PatchType const &patch,
                         Real u0, Real u1, Real v0, Real v1, Real zmin, Real zmax,
                         int level, int max_level, Real eps) NO_INLINE {
        PatchType tpatch(patch);
        rotateV(tpatch);

        if (++_count > 1000 || u0 > u1 || v0 > v1) {
            return false;
        }

        trace("testBezierClipV (%f, %f) - (%f, %f) z:%f, %f  level=%d\n",
              u0, u1, v0, v1, zmin, zmax, level);

        ValueType min, max;
        tpatch.GetMinMax(min, max, eps*1e-3);
        if (0 < min[0] || max[0] < 0 || // x
            0 < min[1] || max[1] < 0 || // y
            max[2] < zmin || zmax < min[2]) { // z
#if USE_MINMAX_FAILFLAG
            // set failFlag only if it's very close
            if (fabs(min[0]) < eps ||
                fabs(max[0]) < eps ||
                fabs(min[1]) < eps ||
                fabs(max[1]) < eps) {
                int failFlag = ((u0 == Real(0)) << 0)
                    | ((u1 == Real(1)) << 1)
                    | ((v0 == Real(0)) << 2)
                      | ((v1 == Real(1)) << 3);
                info->failFlag |= failFlag;
            }
#endif      
            return false;
        }

        bool bClip = isClip(level);
        if (bClip && (isEps(min,max,eps) || isLevel(level,max_level))) {
            return testBezierClipL(info, patch, u0, u1, v0, v1, zmin, zmax, level);
        } else {
            Real tw = 1;
            Real tt0 = 0;
            Real tt1 = 1;

            if (_useBezierClip & bClip) {
                Real rng[2];
                if (getRangeV(rng, tpatch)) {
                    tt0 = rng[0];
                    tt1 = rng[1];
                    tw = tt1-tt0;
                }
            }

            if (tw >= 0.4) {
                PatchType tmp[2];
                patch.SplitV(tmp, 0.5);
                Real vm = (v0+v1)*0.5;
                Real vt[] = {v0,vm,vm,v1};

                int order[2]={0,1};
                coarseSort(order, tmp);
                bool bRet = false;
                if (vt[0] > vt[1]) std::swap(vt[0], vt[1]);
                if (vt[2] > vt[3]) std::swap(vt[2], vt[3]);
                for (int i = 0; i < 2; ++i) {
                    if (testBezierClipU(info, tmp[order[i]], u0, u1,
                                        vt[2*order[i]], vt[2*order[i]+1],
                                        zmin, zmax, level+1, max_level, eps)){
                        zmax = info->t;
                        bRet = true;
                    }
                }
                return bRet;
            } else {
                tt0 = std::max<Real>(0.0,tt0-Epsilon<Real>::GetUvEps());
                tt1 = std::min<Real>(tt1+Epsilon<Real>::GetUvEps(),1.0);
                Real vt[] = {lerp(v0,v1,tt0),lerp(v0,v1,tt1)};
                PatchType tmp;
                patch.CropV(tmp, tt0, tt1);
                return testBezierClipU(info, tmp, u0, u1,
                                       vt[0], vt[1], zmin, zmax, level+1, max_level, eps);
            }
        }
        return false;
    }

    bool testBezierClipL(UVT* info, PatchType const &patch,
                         Real u0, Real u1, Real v0, Real v1,
                         Real zmin, Real zmax, int level) NO_INLINE {
        Real t = Real(0), u = Real(0), v = Real(0);

        trace("testBezierClipL (%f, %f) - (%f, %f) z:%f, %f  level=%d\n",
              u0, u1, v0, v1, zmin, zmax, level);

        if (_directBilinear) {
            ValueType P[4];
            P[0] = patch.Get(0, 0);
            P[1] = patch.Get(N-1, 0);
            P[2] = patch.Get(0, N-1);
            P[3] = patch.Get(N-1, N-1);
            if (!_useTriangle) {
                if (testBilinearPatch(&t, &u, &v, P, zmin, zmax, _uvMargin)) {
                    u = u0*(1-u)+u1*u;
                    v = v0*(1-v)+v1*v;
                    info->u = u;
                    info->v = v;
                    info->t = t;
                    info->level = level;
                    return true;
                }
            } else {
                if (testQuadPlane(&t, &u, &v, P, ValueType(0,0,0), ValueType(0,0,1), zmin, zmax)){
                    u = u0*(1-u)+u1*u;
                    v = v0*(1-v)+v1*v;
                    info->u = u;
                    info->v = v;
                    info->t = t;
                    info->level = level;
                    return true;
                }
            }
            int failFlag = ((u0 == Real(0)) << 0)
                      | ((u1 == Real(1)) << 1)
                      | ((v0 == Real(0)) << 2)
                      | ((v1 == Real(1)) << 3);

            info->failFlag |= failFlag;
            return false;
        }

        bool bRet = false;
        int nPu = N - 1;
        int nPv = N - 1;

        REAL du = REAL(1)/nPu;
        REAL dv = REAL(1)/nPv;

        REAL uu = 0;
        REAL vv = 0;

        ValueType P[4];
        for (int j = 0; j < nPv; ++j) {
            for (int i = 0; i < nPu; ++i) {
                P[0] = patch.Get(i  ,j  );
                P[1] = patch.Get(i+1,j  );
                P[2] = patch.Get(i  ,j+1);
                P[3] = patch.Get(i+1,j+1);
                if(!_useTriangle){
                    if (testBilinearPatch(&t, &u, &v, P, zmin, zmax, _uvMargin)){

                        u = lerp(i*du,(i+1)*du,u);
                        v = lerp(j*dv,(j+1)*dv,v);

                        uu = u;
                        vv = v;

                        zmax = t;

                        u = lerp(u0,u1,u);
                        v = lerp(v0,v1,v);

                        info->u = u;
                        info->v = v;
                        info->t = t;
                        info->level = level;
                        bRet = true;
                    }
                }else{
                    if (testQuadPlane(&t, &u, &v, P, ValueType(0,0,0), ValueType(0,0,1), zmin, zmax)){
                        u = lerp(i*du,(i+1)*du,u);
                        v = lerp(j*dv,(j+1)*dv,v);

                        uu = u;
                        vv = v;

                        zmax = t;

                        u = lerp(u0,u1,u);
                        v = lerp(v0,v1,v);

                        info->u = u;
                        info->v = v;
                        info->t = t;
                        info->level = level;
                        bRet = true;
                    }
                }
            }
        }

        if (bRet) {
            ValueType p = patch.Evaluate(uu,vv);
            info->t = p[2];
        } else {
            int failFlag = ((u0 == Real(0)) << 0)
                      | ((u1 == Real(1)) << 1)
                      | ((v0 == Real(0)) << 2)
                      | ((v1 == Real(1)) << 3);

            info->failFlag |= failFlag;
        }
        return bRet;
    }

    bool testBezierClipRangeU(UVT* info, PatchType const & patch,
                              Real u0, Real u1,
                              Real v0, Real v1, Real zmin, Real zmax,
                              int level, int max_level, Real eps) NO_INLINE {
        PatchType mpatch(patch, u0, u1, v0, v1);
        PatchType tpatch(mpatch);
        rotateU(tpatch);

        trace("testBezierClipRangeU (%f, %f) - (%f, %f) z:%f, %f  level=%d\n",
              u0, u1, v0, v1, zmin, zmax, level);

        ValueType min, max;
        tpatch.GetMinMax(min, max, eps*1e-3);
        if (0 < min[0] || max[0] < 0 || // x
            0 < min[1] || max[1] < 0 || // y
            max[2] < zmin || zmax < min[2]) { // z
#if USE_MINMAX_FAILFLAG
            int failFlag = ((u0 == Real(0)) << 0)
                      | ((u1 == Real(1)) << 1)
                      | ((v0 == Real(0)) << 2)
                      | ((v1 == Real(1)) << 3);
            info->failFlag |= failFlag;
#endif
            return false;
        }
        
        bool bClip = isClip(level);
        if (bClip && (isEps(min,max,eps) || isLevel(level,max_level))){
            return testBezierClipL(info, mpatch, u0, u1, v0, v1, zmin, zmax, level);
        } else {
            Real tw = 1;
            Real tt0 = 0;
            Real tt1 = 1;

            if (_useBezierClip & bClip) {
                Real rng[2];
                if (getRangeU(rng, tpatch)) {
                    tt0 = rng[0];
                    tt1 = rng[1];
                    tw = tt1-tt0;
                }
            }

            if (tw>=0.4) {
                Real um = (u0+u1)*0.5;
                um = Real(1.0) - (Real(1.0) - um);
                Real ut[] = {u0,um,um,u1};
                int order[2]={0,1};
                bool bRet = false;
                for (int i = 0; i < 2; ++i) {
                    if (testBezierClipRangeV(info, patch, ut[2*order[i]], ut[2*order[i]+1],
                                             v0, v1, zmin, zmax, level+1, max_level, eps)) {
                        zmax = info->t;
                        bRet = true;
                    }
                }
                return bRet;
            } else {
                tt0 = std::max<Real>(0.0, tt0-Epsilon<Real>::GetUvEps());
                tt1 = std::min<Real>(tt1+Epsilon<Real>::GetUvEps(), 1.0);
                Real ut[] = {lerp(u0,u1,tt0),lerp(u0,u1,tt1)};
                return testBezierClipRangeV(info, patch, ut[0], ut[1],
                                            v0, v1, zmin, zmax, level+1, max_level, eps);
            }
        }
        return false;
    }

    bool testBezierClipRangeV(UVT *info, PatchType const &patch,
                              Real u0, Real u1, Real v0, Real v1, Real zmin, Real zmax,
                              int level, int max_level, Real eps) NO_INLINE {
        PatchType mpatch(patch, u0, u1, v0, v1);
        PatchType tpatch(mpatch);
        rotateV(tpatch);

        trace("testBezierClipRangeV (%f, %f) - (%f, %f) z:%f, %f  level=%d\n",
              u0, u1, v0, v1, zmin, zmax, level);

        ValueType min, max;
        tpatch.GetMinMax(min, max, eps*1e-3);
        if (0 < min[0] || max[0] < 0 || // x
            0 < min[1] || max[1] < 0 || // y
            max[2] < zmin || zmax < min[2]) { // z
#if USE_MINMAX_FAILFLAG
            int failFlag = ((u0 == Real(0)) << 0)
                      | ((u1 == Real(1)) << 1)
                      | ((v0 == Real(0)) << 2)
                      | ((v1 == Real(1)) << 3);
            info->failFlag |= failFlag;
#endif
            return false;
        }

        bool bClip = isClip(level);
        if (bClip && (isEps(min,max,eps) || isLevel(level,max_level))) {
            return testBezierClipL(info, mpatch, u0, u1, v0, v1, zmin, zmax, level);
        } else {
            Real tw = 1;
            Real tt0 = 0;
            Real tt1 = 1;

            if (_useBezierClip & bClip) {
                Real rng[2];
                if (getRangeV(rng, tpatch)) {
                    tt0 = rng[0];
                    tt1 = rng[1];
                    tw = tt1-tt0;
                }
            }

            if (tw >= 0.4) {
                Real vm = (v0+v1)*0.5;
                vm = Real(1.0) - (Real(1.0) - vm);
                Real vt[] = {v0,vm,vm,v1};
                int order[2]={0,1};
                bool bRet = false;
                for (int i = 0; i < 2; ++i) {
                    if (testBezierClipRangeU(info, patch, u0, u1,
                                             vt[2*order[i]], vt[2*order[i]+1],
                                             zmin, zmax, level+1, max_level, eps)){
                        zmax = info->t;
                        bRet = true;
                    }
                }
                return bRet;
            } else {
                tt0 = std::max<Real>(0.0,tt0-Epsilon<Real>::GetUvEps());
                tt1 = std::min<Real>(tt1+Epsilon<Real>::GetUvEps(),1.0);
                Real vt[] = {lerp(v0,v1,tt0),lerp(v0,v1,tt1)};
                return testBezierClipRangeU(info, patch, u0, u1,
                                            vt[0], vt[1], zmin, zmax, level+1, max_level, eps);
            }
        }
        return false;
    }

    template<typename RAY>
    NO_INLINE bool intersectAABB(RangeAABB *rng, ValueType const & min, ValueType const & max,
                       RAY const & r, Real tmin, Real tmax) const {

        //int phase = r.phase();
        int sign[3] = {r.dirSign[0],r.dirSign[1],r.dirSign[2]};
        ValueType box[2] = { min, max };

        ValueType org(r.org[0], r.org[1], r.org[2]);
        ValueType idir(r.invDir[0], r.invDir[1], r.invDir[2]);

        for (int i = 0; i < 3; ++i) {
            tmin = std::max(tmin, (box[  sign[i]][i]-org[i])*idir[i]);
            tmax = std::min(tmax, (box[1-sign[i]][i]-org[i])*idir[i]);
        }
        rng->tmin = tmin;
        rng->tmax = tmax;

        return rng->tmin <= rng->tmax;
    }

    PatchType _patch;
    Real _uRange[2];
    Real _vRange[2];
    ValueType _min;
    ValueType _max;
    Real _eps;
    int _maxLevel;
    float _uvMargin;
    bool _cropUV;
    bool _useBezierClip;
    bool _useTriangle;
    bool _directBilinear;
    int _wcpFlag;

    int _count;
};

}   // end OsdBezier

#endif  // BEZIER_BEZIER_INTERSECT_H
