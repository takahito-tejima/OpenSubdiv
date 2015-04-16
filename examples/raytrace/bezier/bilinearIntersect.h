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

#ifndef BEZIER_BILINEAR_INTERSECT_H
#define BEZIER_BILINEAR_INTERSECT_H

#include <limits>

#if ENABLE_TRACE_PRINT
#define trace(...) { printf(__VA_ARGS__); }
#else
#define trace(...)
#endif

namespace OsdBezier {

template<class T>
static bool solveBilinearPatch(T* t, T* u, T* v, T tmin, T tmax, T tt, T uu, T vv) {
    if (tmin <= tt && tt <= tmax) {
        *t = tt;
        *u = uu;
        *v = vv;
        return true;
    }
    return false;
}

template <class T>
static int solve2(T root[2], T const coeff[3]) {
    T A = coeff[0];
    T B = coeff[1];
    T C = coeff[2];
    if (fabs(A) <= std::numeric_limits<T>::min()) {
        if (fabs(B) <= std::numeric_limits<T>::min()) return 0;
        T x = -C/B;
        root[0] = x;
        return 1;
    } else {
        T D = B*B - 4*A*C;
        if (D < 0) {
            return 0;
        } else if (D == 0) {
            T x = -0.5*B/A;
            root[0] = x;
            return 1;
        } else {
            T x1 = (fabs(B) + sqrt(D)) / (2.0 * A);
            if (B >= 0) {
                x1 = -x1;
            }
            T x2 = C / (A * x1);
            if (x1 > x2) std::swap(x1, x2);
            root[0] = x1;
            root[1] = x2;
            return 2;
        }
    }
}
template<class T>
static T computeU(T A1, T A2, T B1, T B2, T C1, T C2, T D1, T D2, T v) {
    //return div((v*(C1-C2)+(D1-D2)),(v*(A2-A1)+(B2-B1)));
    T a = v*A2 + B2;
    T b = v*(A2-A1) + B2 - B1;
    if (fabs(b) >= fabs(a)) {
        return (v*(C1-C2) + D1 - D2)/b;
    } else {
        return (-v*C2 - D2)/a;
    }
}
template<class T>
static T computeT(T a, T b, T c, T d, T iq, T u, T v)
{
    return ((u*v)*a + u*b + v*c + d)*iq;
}

template<typename T, typename V>
static bool testBilinearPatch(T *t, T *u, T *v,
                              V const p[4], T tmin, T tmax, float uvMargin) {
    typedef typename V::ElementType Scalar;

    trace("Test bilinear (%f, %f, %f) (%f, %f, %f), (%f, %f, %f), (%f, %f, %f)\n",
          p[0][0], p[0][1], p[0][2],
          p[1][0], p[1][1], p[1][2],
          p[2][0], p[2][1], p[2][2],
          p[3][0], p[3][1], p[3][2]);

    V const & p00 = p[0];
    V const & p10 = p[1];
    V const & p01 = p[2];
    V const & p11 = p[3];

    static const int nPlane = 2;
    V a = p11-p10-p01+p00;
    V b = p10-p00;
    V c = p01-p00;
    V d = p00;

    //xz-zx
    Scalar A1 = a[0];
    Scalar B1 = b[0];
    Scalar C1 = c[0];
    Scalar D1 = d[0];

    //yz-zy
    Scalar A2 = a[1];
    Scalar B2 = b[1];
    Scalar C2 = c[1];
    Scalar D2 = d[1];

    Scalar F1 = A2*C1-A1*C2;
    Scalar F2 = A2*D1-A1*D2+B2*C1-B1*C2;
    Scalar F3 = B2*D1-B1*D2;

    Scalar coeff[] = {F1,F2,F3};
    Scalar root[2] = {};
    //    printf("<%f, %f, %f>\n", F1, F2, F3);
    int nRet = solve2(root,coeff);

    if (nRet) {
        bool bRet = false;
        trace("Solve bilinear:");
        for (int i = 0; i < nRet; ++i) {
            Scalar vv = root[i];
            trace(" vv = %f, ", vv);
            if (0 - uvMargin <= vv && vv <= 1 + uvMargin) {//TODO
                vv = std::max(Scalar(0), std::min(vv,Scalar(1)));
                Scalar uu = computeU(A1, A2, B1, B2, C1, C2, D1, D2, vv);
                trace(" uu = %f, ", uu);
                if (0 - uvMargin  <= uu && uu <= 1 + uvMargin) {//TODO
                    uu = std::max(Scalar(0), std::min(uu,Scalar(1)));
                    Scalar tt = computeT(a[nPlane], b[nPlane], c[nPlane], d[nPlane],
                                         Scalar(1), uu, vv);
                    trace("uv=(%f, %f)", uu, vv);
                    if (solveBilinearPatch(t, u, v, tmin, tmax, tt, uu, vv)){
                        tmax = *t;
                        bRet = true;
                    }
                }
            }
        }
        trace("\n");
        return bRet;
    }
    trace("\n");
    return false;
}

template <typename T, typename V>
inline T dot_(V const &a, V const &b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

template<typename T, typename V>
static bool testTriangle(T *tt, T *uu, T *vv, 
    V const & p0, V const & p1, V const & p2, V const & org, V const & dir, T tmin, T tmax) {
    //typedef typename V::ElementType Scalar;
    T t,u,v; 
    //-e1 = p0-p1
    V e1(p0-p1);//vA
    //-e2 = p0-p2
    V e2(p0-p2);//vB 
    //dir = GHI 
    V bDir(cross(e2,dir));
    
    T iM = dot_<T,V>(e1,bDir);

    if(0 < iM){
        //p0-org
        V vOrg(p0 - org);

        u = dot_<T,V>(vOrg,bDir);
        if(u < 0 || iM < u)return false;

        V vE(cross(e1,vOrg));
        
        v = dot_<T,V>(dir,vE);
        if(v < 0 || iM  < u+v)return false;

        t = -dot_<T,V>(e2,vE);
    }else if(iM < 0){
        //p0-org
        V vOrg(p0 - org);//JKL

        u = dot_<T,V>(vOrg,bDir);
        if(u > 0 || iM > u)return false;
        
        V vE(cross(e1,vOrg));
        
        v = dot_<T,V>(dir,vE);
        if(v > 0 || iM  > u+v)return false;

        t = -dot_<T,V>(e2,vE);
    }else{
        return false;
    }

    iM = T(1)/iM;
    t *= iM;
    if(t<tmin || tmax <t)return false;
    u *= iM;
    v *= iM;

    *tt = t;
    *uu = u;
    *vv = v;
    
    return true;
}


template<typename T, typename V>
static bool testQuadPlane(T *t, T *u, T *v, 
    V const p[4], V const & org, V const & dir, T tmin, T tmax) {
    //typedef typename V::ElementType Scalar;

    V const & p00 = p[0];
    V const & p10 = p[1];
    V const & p01 = p[2];
    V const & p11 = p[3];

    T tt, uu, vv;
    bool bRet = false;
    if(testTriangle(&tt, &uu, &vv, p00, p01, p10, org, dir, tmin, tmax))
    {
        T ww = T(1)-(uu+vv);
        *u = ww*T(0)+uu*T(0)+vv*T(1);//00 - 01 - 10
        *v = ww*T(0)+uu*T(1)+vv*T(0);//00 - 01 - 10
        *t = tt;
        tmax = tt;
        bRet = true;
    }
    if(testTriangle(&tt, &uu, &vv, p10, p01, p11, org, dir, tmin, tmax))
    {
        T ww = T(1)-(uu+vv);
        *u = ww*T(1)+uu*T(0)+vv*T(1);//10 - 01 - 11
        *v = ww*T(0)+uu*T(1)+vv*T(1);//10 - 01 - 11
        *t = tt;
        tmax = tt;
        bRet = true;
    }

    return bRet;
}

}


#endif  // BEZIER_BILINEAR_INTERSECT_H
