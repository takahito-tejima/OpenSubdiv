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
#ifndef BEZIER_MATH_H
#define BEZIER_MATH_H

#include <cmath>
#include <algorithm>

namespace OsdBezier {

template<class REAL> class matrix3t;
template<class REAL> class matrix4t;

template <typename REAL>
struct vec3t {
    typedef REAL Real;
    typedef REAL ElementType;
    typedef matrix3t<REAL> Matrix3Type;
    typedef matrix4t<REAL> Matrix4Type;

    vec3t(Real f) {
        v[0] = v[1] = v[2] = f;
    }
    vec3t() {
        v[0] = v[1] = v[2] = 0;
    }
    vec3t(Real x, Real y, Real z) {
        v[0] = x; v[1] = y; v[2] = z;
    }
    template <typename T>
    vec3t(T const &s) {
        v[0] = s[0]; v[1] = s[1]; v[2] = s[2];
    }

    vec3t operator*(Real f) const {
        return vec3t(v[0] * f, v[1] * f, v[2] * f);
    }
    vec3t operator*(vec3t f) const {
        return vec3t(v[0] * f[0], v[1] * f[1], v[2] * f[2]);
    }
    vec3t operator/(Real f) const {
        return vec3t(v[0] / f, v[1] / f, v[2] / f);
    }
    vec3t operator+(vec3t f) const {
        return vec3t(v[0] + f.v[0], v[1] + f.v[1], v[2] + f.v[2]);
    }
    vec3t operator-(vec3t f) const {
        return vec3t(v[0] - f.v[0], v[1] - f.v[1], v[2] - f.v[2]);
    }
    vec3t &operator+=(const vec3t &f2) {
        v[0] += f2.v[0]; v[1] += f2.v[1]; v[2] += f2.v[2];
        return (*this);
    }
    vec3t &operator-=(const vec3t &f2) {
        v[0] -= f2.v[0]; v[1] -= f2.v[1]; v[2] -= f2.v[2];
        return (*this);
    }
    Real operator[](int i) const { return v[i]; }
    Real &operator[](int i) { return v[i]; }

    Real length() const { return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); }
    void normalize() {
        Real inv_len = 1.0 / length();
        v[0] *= inv_len;
        v[1] *= inv_len;
        v[2] *= inv_len;
    }
    void normalize2() {
        Real inv_len = 1.0 / sqrt(v[0]*v[0] + v[1]*v[1]);
        v[0] *= inv_len;
        v[1] *= inv_len;
        v[2] = 0;
    }
    vec3t neg() const { return vec3t(-v[0], -v[1], -v[2]); }

    vec3t min(vec3t const &a) const {
        return vec3t(std::min(v[0], a.v[0]),
                     std::min(v[1], a.v[1]),
                     std::min(v[2], a.v[2]));
    }

    vec3t max(vec3t const &a) const {
        return vec3t(std::max(v[0], a.v[0]),
                     std::max(v[1], a.v[1]),
                     std::max(v[2], a.v[2]));
    }

    static vec3t min(vec3t const &a, vec3t const &b) {
        return vec3t(std::min(a.v[0], b.v[0]),
                     std::min(a.v[1], b.v[1]),
                     std::min(a.v[2], b.v[2]));
    }

    static vec3t max(vec3t const &a, vec3t const &b) {
        return vec3t(std::max(a.v[0], b.v[0]),
                     std::max(a.v[1], b.v[1]),
                     std::max(a.v[2], b.v[2]));
    }

    template <typename T>
    vec3t &operator=(const vec3t<T> &s) {
        v[0] = s[0];
        v[1] = s[1];
        v[2] = s[2];
        return *this;
    }

    bool operator==(const vec3t<REAL> &s) {
        return (v[0] == s.v[0] && v[1] == s.v[1] && v[2] == s.v[2]);
    }

    bool operator !=(const vec3t<REAL> &s) {
        return !(*this == s);
    }

    Real v[3];
};

template <typename REAL>
inline vec3t<REAL> operator*(REAL f, const vec3t<REAL> &v) {
    return v*f;
}
template <typename REAL>
inline vec3t<REAL> operator*(int f, const vec3t<REAL> &v) {
    return v*(REAL)f;
}

template <typename REAL>
inline REAL dot(vec3t<REAL> const &a, vec3t<REAL> const &b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

template <typename REAL>
inline vec3t<REAL> cross(vec3t<REAL> const &a, vec3t<REAL> const &b) {
    vec3t<REAL> c;
    c[0] = a[1] * b[2] - a[2] * b[1];
    c[1] = a[2] * b[0] - a[0] * b[2];
    c[2] = a[0] * b[1] - a[1] * b[0];
    return c;
}

// ----------------------------------------------------------------------------

template<class REAL>
class matrix4t {
public:
    matrix4t() { }
    matrix4t(REAL m00, REAL m01, REAL m02, REAL m03,
             REAL m10, REAL m11, REAL m12, REAL m13,
             REAL m20, REAL m21, REAL m22, REAL m23,
             REAL m30, REAL m31, REAL m32, REAL m33) {
        m[0] = m00;
        m[1] = m01;
        m[2] = m02;
        m[3] = m03;

        m[4] = m10;
        m[5] = m11;
        m[6] = m12;
        m[7] = m13;

        m[8] = m20;
        m[9] = m21;
        m[10] = m22;
        m[11] = m23;

        m[12] = m30;
        m[13] = m31;
        m[14] = m32;
        m[15] = m33;
    }

    template <typename V>
    matrix4t(V const &rayOrg, V const &rayDir) {

        V z = rayDir;
        int plane = 0;
        if (fabs(z[1]) < fabs(z[plane])) plane = 1;
        if (fabs(z[2]) < fabs(z[plane])) plane = 2;

        V x = (plane == 0) ? V(1, 0, 0) :
            ((plane == 1) ? V(0, 1, 0) : V(0, 0, 1));
        V y = cross(z, x);
        y.normalize();
        x = cross(y, z);
        matrix4t rot = matrix4t(x[0],x[1],x[2],0,
                                y[0],y[1],y[2],0,
                                z[0],z[1],z[2],0,
                                0,0,0,1);
        matrix4t trs = matrix4t(1,0,0,-rayOrg[0],
                                0,1,0,-rayOrg[1],
                                0,0,1,-rayOrg[2],
                                0,0,0,1);
        *this = rot*trs;
    }


    REAL * operator[] (int i){            //pointer what self is const.
        return element[i];
    }
    const REAL * operator[] (int i) const {//pointer what self is const.
        return element[i];
    }

    union {
        REAL m[16];
        REAL element[4][4];
    };
};

template <typename REAL>
static inline matrix4t<REAL> operator*(matrix4t<REAL> const& lhs, matrix4t<REAL> const& rhs)
{
    return matrix4t<REAL>(
        lhs[0][0] * rhs[0][0] + lhs[0][1] * rhs[1][0] + lhs[0][2] * rhs[2][0] + lhs[0][3] * rhs[3][0],
        lhs[0][0] * rhs[0][1] + lhs[0][1] * rhs[1][1] + lhs[0][2] * rhs[2][1] + lhs[0][3] * rhs[3][1],
        lhs[0][0] * rhs[0][2] + lhs[0][1] * rhs[1][2] + lhs[0][2] * rhs[2][2] + lhs[0][3] * rhs[3][2],
        lhs[0][0] * rhs[0][3] + lhs[0][1] * rhs[1][3] + lhs[0][2] * rhs[2][3] + lhs[0][3] * rhs[3][3],

        lhs[1][0] * rhs[0][0] + lhs[1][1] * rhs[1][0] + lhs[1][2] * rhs[2][0] + lhs[1][3] * rhs[3][0],
        lhs[1][0] * rhs[0][1] + lhs[1][1] * rhs[1][1] + lhs[1][2] * rhs[2][1] + lhs[1][3] * rhs[3][1],
        lhs[1][0] * rhs[0][2] + lhs[1][1] * rhs[1][2] + lhs[1][2] * rhs[2][2] + lhs[1][3] * rhs[3][2],
        lhs[1][0] * rhs[0][3] + lhs[1][1] * rhs[1][3] + lhs[1][2] * rhs[2][3] + lhs[1][3] * rhs[3][3],

        lhs[2][0] * rhs[0][0] + lhs[2][1] * rhs[1][0] + lhs[2][2] * rhs[2][0] + lhs[2][3] * rhs[3][0],
        lhs[2][0] * rhs[0][1] + lhs[2][1] * rhs[1][1] + lhs[2][2] * rhs[2][1] + lhs[2][3] * rhs[3][1],
        lhs[2][0] * rhs[0][2] + lhs[2][1] * rhs[1][2] + lhs[2][2] * rhs[2][2] + lhs[2][3] * rhs[3][2],
        lhs[2][0] * rhs[0][3] + lhs[2][1] * rhs[1][3] + lhs[2][2] * rhs[2][3] + lhs[2][3] * rhs[3][3],

        lhs[3][0] * rhs[0][0] + lhs[3][1] * rhs[1][0] + lhs[3][2] * rhs[2][0] + lhs[3][3] * rhs[3][0],
        lhs[3][0] * rhs[0][1] + lhs[3][1] * rhs[1][1] + lhs[3][2] * rhs[2][1] + lhs[3][3] * rhs[3][1],
        lhs[3][0] * rhs[0][2] + lhs[3][1] * rhs[1][2] + lhs[3][2] * rhs[2][2] + lhs[3][3] * rhs[3][2],
        lhs[3][0] * rhs[0][3] + lhs[3][1] * rhs[1][3] + lhs[3][2] * rhs[2][3] + lhs[3][3] * rhs[3][3]
                   );
    }

template <typename REAL>
static inline vec3t<REAL> operator*(const matrix4t<REAL>& m, vec3t<REAL> const & v)
{
    REAL r[4];
    r[0] = m.m[0] * v[0] + m.m[1] * v[1] + m.m[2] * v[2] + m.m[3];
    r[1] = m.m[4] * v[0] + m.m[5] * v[1] + m.m[6] * v[2] + m.m[7];
    r[2] = m.m[8] * v[0] + m.m[9] * v[1] + m.m[10] * v[2] + m.m[11];
    r[3] = m.m[12] * v[0] + m.m[13] * v[1] + m.m[14] * v[2] + m.m[15];
    REAL ir = REAL(1.0)/r[3];
    return vec3t<REAL>(r[0]*ir, r[1]*ir, r[2]*ir);
}

// ----------------------------------------------------------------------------

template<class T>
class matrix3t {
public:
    matrix3t() { }
    template <typename V>
    matrix3t(const V &m0, const V &m1, const V &m2) {
        m[0] = m0[0];
        m[1] = m0[1];
        m[2] = m0[2];
        m[3] = m1[0];
        m[4] = m1[1];
        m[5] = m1[2];
        m[6] = m2[0];
        m[7] = m2[1];
        m[8] = m2[2];
    }
    matrix3t(T m00, T m01, T m02, T m10, T m11, T m12, T m20, T m21, T m22) {
        m[0] = m00;
        m[1] = m01;
        m[2] = m02;
        m[3] = m10;
        m[4] = m11;
        m[5] = m12;
        m[6] = m20;
        m[7] = m21;
        m[8] = m22;
    }
    matrix3t(const matrix3t& rhs){
        memcpy(m,rhs.m,sizeof(T)*9);
    }

    // getRotate constructor.
    template <typename V>
    matrix3t(V const &dx) {
        V x = dx;
        x.normalize2();
        V y(-x[1], x[0], 0);
        // V z(0, 0, 1);

        m[0] = x[0];
        m[1] = x[1];
        m[2] = x[2];
        m[3] = y[0];
        m[4] = y[1];
        m[5] = y[2];
        m[6] = 0;
        m[7] = 0;
        m[8] = 1;
    }

    T* operator[](size_t i)     {return m+3*i;}
    const T* operator[](size_t i)const{return m+3*i;}

    //0,1,2
    //3,4,5
    //6.7.8
    void transpose(){
        std::swap(m[1],m[3]);
        std::swap(m[2],m[6]);
        std::swap(m[5],m[7]);
    }
    T m[3*3];
};

template<class T, class V>
inline V operator*(const matrix3t<T>& m, const V& v)
{
    V r;
    r[0] = m.m[0] * v[0] + m.m[1] * v[1] + m.m[2] * v[2];
    r[1] = m.m[3] * v[0] + m.m[4] * v[1] + m.m[5] * v[2];
    r[2] = m.m[6] * v[0] + m.m[7] * v[1] + m.m[8] * v[2];
    return r;
}

//----------------------------------------------------------

typedef matrix3t<float> matrix3f;
typedef matrix4t<float> matrix4f;
typedef vec3t<float> vec3f;
typedef matrix3t<double> matrix3d;
typedef matrix4t<double> matrix4d;
typedef vec3t<double> vec3d;

} // namespace OsdBezier

#endif  // BEZIER_MATH_H
