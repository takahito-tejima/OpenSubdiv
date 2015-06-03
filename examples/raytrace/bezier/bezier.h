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

#ifndef BEZIER_BEZIER_H
#define BEZIER_BEZIER_H

#include <cmath>

#ifdef __GNUC__
#define NO_INLINE __attribute__((noinline))
#else
#define NO_INLINE
#endif

namespace OsdBezier {

template<int N, int k>
struct Binomial
{
    const static int value =  (Binomial<N-1,k-1>::value + Binomial<N-1,k>::value);
};

template<>
struct Binomial<0, 0>
{
    const static int value = 1;
};

template<int N>
struct Binomial<N, 0>
{
    const static int value = 1;
};

template<int N>
struct Binomial<N, N>
{
    const static int value = 1;
};

template<int N, int k>
inline int binomial()
{
    return Binomial<N, k>::value;
}

template<typename REAL, int N, int I>
inline REAL bernstein(REAL t) {
    return REAL(binomial<N, I>() * std::pow(REAL(1)-t, N-I) * std::pow(t, I));
}

template<typename T, typename REAL, int N, int I>
struct BezierCurveEvaluate
{
    static T evaluate(const REAL t, const T *const cp) {
        return cp[I] * bernstein<REAL, N, I>(t) +
            BezierCurveEvaluate<T, REAL, N, I - 1>::evaluate(t, cp);
    }
};

template<typename T, typename REAL, int N>
struct BezierCurveEvaluate<T, REAL, N, 0>
{
   static T evaluate(const REAL t, const T *const cp)
   {
       return cp[0] * bernstein<REAL, N, 0>(t);
   }
};

template <int N, int I, int STRIDE, typename VALUE_TYPE, typename REAL>
struct BezierSplit {
    typedef VALUE_TYPE ValueType;
    typedef REAL Real;
    BezierSplit(ValueType a[], ValueType b[], const ValueType p[], Real t) {
        int tn = N-1-I;//3
        a[I*STRIDE] = p[0];
        b[tn*STRIDE] = p[tn*STRIDE];
        ValueType tmp[(N-1)*STRIDE];
        Real s = 1-t;
        t = 1-s;
        for (int j = 0; j < tn; j++) {
            //p[j] + t*(p[j+1] - p[j])
            tmp[j*STRIDE] = s*p[j*STRIDE] + t*p[(j+1)*STRIDE];
        }
        BezierSplit<N, I+1, STRIDE, VALUE_TYPE, REAL>(a, b, &tmp[0], t);
    }
};

template <int N, int STRIDE, typename VALUE_TYPE, typename REAL>
struct BezierSplit<N, N, STRIDE, VALUE_TYPE, REAL> {
    typedef VALUE_TYPE ValueType;
    typedef REAL Real;
    BezierSplit(ValueType[], ValueType[], const ValueType[], Real) {
    }
};

template <int N, int STRIDE, typename VALUE_TYPE, typename REAL>
struct BezierCrop {
    typedef VALUE_TYPE ValueType;
    typedef REAL Real;
    BezierCrop(ValueType r[], const ValueType p[], Real s, Real t) {
        ValueType tmp0[N*STRIDE], tmp1[N*STRIDE];
        BezierSplit<N, 0, STRIDE, ValueType, Real> split1(&tmp0[0], &tmp1[0], p, t);
        BezierSplit<N, 0, STRIDE, ValueType, Real> split2(&tmp1[0], r, &tmp0[0], s/t);
    }
};

template <int STRIDE, typename VALUE_TYPE, typename REAL>
struct BezierCrop<4, STRIDE, VALUE_TYPE, REAL> {
    typedef VALUE_TYPE ValueType;
    typedef REAL Real;
    BezierCrop(ValueType r[], const ValueType p[], Real s, Real t) {
        const ValueType &p0 = p[0*STRIDE];
        const ValueType &p1 = p[1*STRIDE];
        const ValueType &p2 = p[2*STRIDE];
        const ValueType &p3 = p[3*STRIDE];
        // watertight cropping for cubic b-spline
        //
        //   p0    p1   p2    p3
        //   0----s--------t---1
        //   1----T--------S---0
        //
        Real T = 1-s;
        Real S = 1-t;
        s = 1 - T;
        t = 1 - S;
        r[0*STRIDE] = (p0*(T*T)*T + p3*(s*s)*s) + (p1*(s*T)*(3*T)       + p2*(s*s)*(3*T));
        r[1*STRIDE] = (p0*(T*T)*S + p3*(s*s)*t) + (p1*T*(2*(S*s) + T*t) + p2*s*(2*(t*T) + (s*S)));
        r[2*STRIDE] = (p3*(t*t)*s + p0*(S*S)*T) + (p2*t*(2*(s*S) + t*T) + p1*S*(2*(T*t) + (S*s)));
        r[3*STRIDE] = (p3*(t*t)*t + p0*(S*S)*S) + (p2*(S*t)*(3*t)       + p1*(S*S)*(3*t));
    }
};

template<class VALUE_TYPE, class REAL, int N=4>
class BezierPatch {
public:
    typedef BezierPatch<VALUE_TYPE, REAL, N> This;
    typedef VALUE_TYPE ValueType;
    typedef REAL Real;
    static const int Order = N;
    static const int Ncp = N*N;

    /// Constructor.
    BezierPatch() {
        //for (int i = 0; i < ncp; ++i) _cp[i] = VALUE_TYPE();
    }

    /// Constructor.
    BezierPatch(BezierPatch const &other) {
        for (int i = 0; i < Ncp; ++i) _cp[i] = other._cp[i];
    }

    /// Constructor.
    template<typename MATRIX>
    BezierPatch(BezierPatch const &other, MATRIX const &m) {
        for (int i = 0; i < Ncp; ++i) _cp[i] = m * other._cp[i];
    }

    /// Constructor.
    BezierPatch(BezierPatch const &other, Real u0, Real u1, Real v0, Real v1) {//with crop
        for (int i = 0; i < Ncp; ++i) _cp[i] = other._cp[i];
        ValueType tmp[Ncp];
        for (int i = 0; i < N; ++i) bezierCrop (&tmp[i*N+0], &_cp[i*N+0], u0, u1);
        for (int i = 0; i < N; ++i) bezierCropV(&_cp[i], &tmp[i], v0, v1);
    }

    /// Constructor.
    template<class T>
    BezierPatch(T const *p) {
        for (int i = 0; i < Ncp; ++i) _cp[i] = p[i];
    }

    /// Get
    ValueType const &Get(int i, int j) const {
        return _cp[N*j + i];
    }

    /// Set
    void Set(int i, int j, VALUE_TYPE const &v) {
        _cp[N*j + i] = v;
    }

    /// Evaluate
    ValueType Evaluate(REAL u, REAL v) const {
        ValueType b[N];
        for (int i = 0; i < N; ++i) {
            b[i] = evaluate(u, _cp+i*N);
        }
        return evaluate(v, b);
    }

    /// Evaluate derivative
    ValueType EvaluateDu(REAL u, REAL v) const {
        ValueType b[N];
        for (int i = 0; i < N; ++i) {
            b[i] = evaluateD(u, _cp+i*N);
        }
        return evaluate(v, b);
    }

    /// Evaluate derivative
    ValueType EvaluateDv(REAL u, REAL v) const {
        ValueType b[N];
        for (int i = 0; i < N; ++i) {
            b[i] = evaluate(u, _cp+i*N);
        }
        return evaluateD(v, b);
    }

    ValueType EvaluateNormal(Real u, Real v) const {
        ValueType du = EvaluateDu(u, v);
        ValueType dv = EvaluateDv(u, v);
        ValueType normal = cross(du, dv);
        normal.normalize();
        return normal;
    }

    /// min
    ValueType GetMin() const {
        ValueType min = _cp[0];
        for (int i = 1; i < Ncp; ++i) {
            min = ValueType::min(min, _cp[i]);
        }
        return min;
    }
    /// max
    ValueType GetMax() const {
        ValueType max = _cp[0];
        for (int i = 1; i < Ncp; ++i) {
            max = ValueType::max(max, _cp[i]);
        }
        return max;
    }

    void GetMinMax(ValueType &min, ValueType &max) const NO_INLINE {
#if 0
        min = max = _cp[0];
        for (int i = 1; i < Ncp; ++i) {
            min = min.min(_cp[i]);
            max = max.max(_cp[i]);
        }
#else
        min = _cp[0].min(_cp[1]).min(_cp[2]).min(_cp[3])
            .min(_cp[4]).min(_cp[5]).min(_cp[6]).min(_cp[7])
            .min(_cp[8]).min(_cp[9]).min(_cp[10]).min(_cp[11])
            .min(_cp[12]).min(_cp[13]).min(_cp[14]).min(_cp[15]);
        max = _cp[0].max(_cp[1]).max(_cp[2]).max(_cp[3])
            .max(_cp[4]).max(_cp[5]).max(_cp[6]).max(_cp[7])
            .max(_cp[8]).max(_cp[9]).max(_cp[10]).max(_cp[11])
            .max(_cp[12]).max(_cp[13]).max(_cp[14]).max(_cp[15]);
#endif
    }
    void GetMinMax(ValueType &min, ValueType &max, Real eps) const {
        GetMinMax(min, max);
        min -= ValueType(eps);
        max += ValueType(eps);
    }

    /// transform
    template<typename MATRIX>
    This & Transform(const MATRIX &m) {
        for (int i = 0; i < Ncp; ++i) _cp[i] = m * _cp[i];
        return *this;
    }

    /// split
    void Split(This patches[4], Real u, Real v) const {
        ValueType tmp0[Ncp], tmp1[Ncp];

        // split U
        for (int i = 0; i < N; ++i) {
            bezierSplit(&tmp0[i*N+0], &tmp1[i*N+0], &_cp[i*N+0], u);
        }
        // uv -> vu
        transpose(&tmp0[0]);// 00 01
        transpose(&tmp1[0]);// 10 11

        // split V
        for (int i = 0; i < N; ++i) {
            bezierSplit(&patches[0]._cp[i*N+0],
                        &patches[2]._cp[i*N+0],
                        &tmp0[i*N+0], v);
            bezierSplit(&patches[1]._cp[i*N+0],
                        &patches[3]._cp[i*N+0],
                        &tmp1[i*N+0], v);
        }
        // vu -> uv
        transpose(patches[0]._cp);//00
        transpose(patches[1]._cp);//10
        transpose(patches[2]._cp);//01
        transpose(patches[3]._cp);//11
    }

    /// split
    void SplitU(This patches[2], Real u) const {
        for (int i = 0; i < N; ++i) {
            bezierSplit(&patches[0]._cp[i*N+0],
                        &patches[1]._cp[i*N+0],
                        &_cp[i*N+0], u);
        }
    }

    /// split
    void SplitV(This patches[2], Real v) const {
        for (int i = 0; i < N; ++i) {
            bezierSplitV(&patches[0]._cp[i],
                         &patches[1]._cp[i],
                         &_cp[i], v);
        }
    }

    /// crop
    void CropU(This &patch, Real u0, Real u1) const {
        for (int i = 0; i < N; ++i) {
#if 0
            ValueType tmp0[N], tmp1[N];
            bezierSplit(&tmp0[0], &tmp1[0], &_cp[i*N+0], u1);
            bezierSplit(&tmp1[0], &patch._cp[i*N+0], &tmp0[0],  u0/u1);
#else
            bezierCrop(&patch._cp[i*N+0], &_cp[i*N+0], u0, u1);
#endif
        }
    }

    /// crop
    void CropV(This &patch, Real v0, Real v1) const {
        for (int i = 0; i < N; ++i) {
#if 0
            ValueType tmp0[N*N], tmp1[N*N];
            bezierSplitV(&tmp0[i], &tmp1[i], &_cp[i], v1);
            bezierSplitV(&tmp1[i], &patch._cp[i], &tmp0[i],  v0/v1);
#else
            bezierCropV(&patch._cp[i], &_cp[i], v0, v1);
#endif
        }
    }

    bool Crop(This &patch, Real u0, Real u1, Real v0, Real v1) const {
        ValueType tmp[Ncp];
        for (int i = 0; i < N; ++i) bezierCrop (&tmp[i*N+0], &_cp[i*N+0], u0, u1);
        for (int i = 0; i < N; ++i) bezierCropV(&patch._cp[i], &tmp[i], v0, v1);
        return true;
    }

    ValueType GetLv() const {
        return Get(0, N-1) - Get(0, 0) + Get(N-1, N-1) - Get(N-1, 0);
    }

    ValueType GetLu() const {
        return Get(N-1, 0) - Get(0, 0) + Get(N-1, N-1) - Get(0, N-1);
    }

    void Test() {
        This patches[4];
        Split(patches, 0.5, 0.5);
        SplitU(patches, 0.5);
        SplitV(patches, 0.5);
        CropU(patches[0], 0.25, 0.75);
        CropV(patches[0], 0.25, 0.75);
    }

    void Rotate() { // right 90
        transpose(_cp);
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N/2; ++j) {
                std::swap(_cp[i*N+j], _cp[i*N+N-1-j]);
            }
        }
    }

private:
    void bezierSplit(ValueType a[], ValueType b[], const ValueType p[], Real t) const {
        BezierSplit<N, 0, /*stride*/1, ValueType, Real> split(a, b, p, t);
    }
    void bezierSplitV(ValueType a[], ValueType b[], const ValueType p[], Real t) const {
        BezierSplit<N, 0, /*stride*/N, ValueType, Real> split(a, b, p, t);
    }
    void bezierCrop(ValueType r[], const ValueType p[], Real s, Real t) const {
        BezierCrop<N, 1, ValueType, Real> crop(r, p, s, t);
    }
    void bezierCropV(ValueType r[], const ValueType p[], Real s, Real t) const {
        BezierCrop<N, N, ValueType, Real> crop(r, p, s, t);
    }

    static void transpose(ValueType m[]) {
        for (int y = 0; y < N; ++y) {
            for (int x = y+1; x < N; ++x) {
                std::swap(m[x*N+y], m[y*N+x]);
            }
        }
    }

    static ValueType evaluate(Real t, const ValueType * const cp) {
        return BezierCurveEvaluate<ValueType, Real, N-1, N-1>::evaluate(t, cp);
    }
    static ValueType evaluateD(Real t, const ValueType * const cp) {
        if (N == 4) {
            REAL t2 = t*t;
            return cp[0] * (3*t2*-1 + 2*t* 3 + -3)
                + cp[1] * (3*t2* 3 + 2*t*-6 +  3)
                + cp[2] * (3*t2*-3 + 2*t* 3)
                + cp[3] * (3*t2* 1);
        } else {
            return ValueType();
            // not implemented !
        }
    }

    ValueType _cp[Ncp];
};

}   // end namespace OsdBezier

#endif  // BEZIER_BEZIER_H
