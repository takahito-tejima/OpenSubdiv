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

#include "shader.h"
#include "mesh.h"
#include "ray.h"
#include "scene.h"

inline float
randomreal(void) {
    static unsigned int x = 123456789, y = 362436069, z = 521288629,
        w = 88675123;
    unsigned t = x ^ (x << 11);
    x = y;
    y = z;
    z = w;
    w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    return w * (1.0f / 4294967296.0f);
}

vec3f
ShadeLambert(const Scene *scene, const Ray &ray, const Intersection &isect)
{
    // No zero in d to distinguish crack pixel color(dark background color)
    const Material& mat = scene->GetMesh().GetMaterial(isect.matID);

    vec3f diffuse = mat.diffuse;
    float d = std::max(float(0.2), dot(ray.dir, isect.normal));
    return d * diffuse;
}

vec3f
ShadePatchCoord(const Scene *, const Ray &ray, const Intersection &isect)
{
    // No zero in d to distinguish crack pixel color(dark background color)
    float d = std::max(float(0.2), dot(ray.dir, isect.normal));
    return d * vec3f(isect.u, isect.v, 1);
}
// ---------------------------------------------------------------------------
// Simple heat map coloring

vec3f
ShadeHeatmap(const Scene *, const Ray &, const Intersection &isect)
{
    float val = isect.clipLevel;
    float maxVal = isect.maxLevel;

    float blue[3]; blue[0] = 0.0; blue[1] = 0.0; blue[2] = 1.0;
    //vector3 green(0.0, 1.0, 0.0);
    float red[3]; red[0] = 1.0; red[1] = 0.0; red[2] = 0.0;
    //vector3 red(1.0, 0.0, 0.0);

    // 0 -> blue, 50 -> (blue + red)/2, 100 -> red
    if (val < 0.0) val = 0.0;
    if (val > maxVal) val = maxVal;
    float t = val / maxVal; // => [0, 1]

    vec3f col;
    col[0] = (1.0 - t) * blue[0] + t * red[0];
    col[1] = (1.0 - t) * blue[1] + t * red[1];
    col[2] = (1.0 - t) * blue[2] + t * red[2];
    return col;
}

// ---------------------------------------------------------------------------

vec3f
ShadePatchType(const Scene *scene, const Ray &ray, const Intersection &isect)
{
    vec3f col;
    float l = isect.level * 0.05;
    float d = std::max(float(0.2), dot(ray.dir, isect.normal));

    col = d * (scene->GetMesh().GetColor(isect.patchID) - vec3f(l));
    col[0] = std::max(0.0f, col[0]);
    col[1] = std::max(0.0f, col[1]);
    col[2] = std::max(0.0f, col[2]);

    return col;
}

// ---------------------------------------------------------------------------

vec3f
ShadeAmbientOcclusion(const Scene *scene, const Ray &ray, const Intersection &isect)
{
    vec3f sample(0.5-randomreal(), 0.5-randomreal(), 0.5-randomreal());
    sample.normalize();

    Ray sray;
    sray.dir = sample * (dot(sample, isect.normal) > 0 ? -1 : 1);
    sray.invDir = sray.dir.neg();
    sray.org = ray.org + ray.dir * isect.t + sray.dir * 0.0001;

    Intersection si;
    vec3f color;
    if (scene->Traverse(sray, &si, NULL)) {
        color = vec3f(0.0f);
    } else {
        color = vec3f(1.0f);
    }
    return color;
}

// ---------------------------------------------------------------------------
// Physically-based shader

inline float vavg(vec3f x)
{
    return (x[0] + x[1] + x[2]) / 3;
}


inline vec3f vclamp01(vec3f x)
{
    vec3f ret;
    ret[0] = x[0];
    ret[1] = x[1];
    ret[2] = x[2];

    return ret;
}

inline vec3f reflect(const vec3f &in, const vec3f &n)
{
    float d = dot(in, n);
    return n * (2.0f * d) -in;
}

inline vec3f refract(bool &tir, const vec3f &in, const vec3f &n, float eta)
{
    vec3f ret;
    vec3f N;
    double e = eta;
    double cos1 = dot(in, n);
    if (cos1 > 0.0) { // entering
        N = n;
    } else { // outgoing
        cos1 = -cos1;
        e = 1.0f / eta;
        N = n.neg();
    }

    double k = 1.0 - (e * e) * (1.0 - cos1 * cos1);
    if (k <= 0.0) {
        // Toral internal reflection.
        ret = reflect(in, n);
        tir = true;
        ret.normalize();
        return ret;
    }

    k = e * cos1 - sqrt(k);

    tir = false;
    ret = k * N - e * in;
    ret.normalize();

    return ret;
}

static void fresnel_factor(vec3f &refl, vec3f& refr, float &kr, float &kt,
                           const vec3f &in, const vec3f &n, float eta)
{
    refl = reflect(in, n);

    bool tir;
    refr = refract(tir, in, n, eta);

    if (tir) {
        kr = 1.0;
        kt = 0.0;
        return;
    }

    float cos_r = dot(refl, n);
    float cos_t = -dot(refr, n);

    float rp = (cos_t - eta * cos_r) / (cos_t + eta * cos_r);
    float rs = (cos_r - eta * cos_t) / (cos_r + eta * cos_t);
    kr = (rp * rp + rs * rs) * 0.5f;

    if (kr < 0.0f)
        kr = 0.0f;
    if (kr > 1.0f)
        kr = 1.0f;

    kt = 1.0f - kr;
}

static void GenerateBasis(vec3f &tangent, vec3f &binormal,
                          const vec3f &normal)
{
    // Find the minor axis of the vector
    int index = -1;
    double minval = 1.0e+6;
    double val = 0;

    for (int i = 0; i < 3; i++) {
        val = fabsf(normal[i]);
        if (val < minval) {
            minval = val;
            index = i;
        }
    }

    if (index == 0) {

        tangent[0] = 0.0;
        tangent[1] = -normal[2];
        tangent[2] = normal[1];
        tangent.normalize();

        binormal = cross(tangent, normal);
        binormal.normalize();

    } else if (index == 1) {

        tangent[0] = -normal[2];
        tangent[1] = 0.0;
        tangent[2] = normal[0];
        tangent.normalize();

        binormal = cross(tangent, normal);
        binormal.normalize();

    } else {

        tangent[0] = -normal[1];
        tangent[1] = normal[0];
        tangent[2] = 0.0;
        tangent.normalize();

        binormal = cross(tangent, normal);
        binormal.normalize();
    }
}

static void SampleDiffuseIS(vec3f &dir, const vec3f &normal)
{
    vec3f tangent, binormal;

    GenerateBasis(tangent, binormal, normal);

    float theta = acosf(sqrtf(1.0f - randomreal()));
    float phi = 2.0f * M_PI * randomreal();

  //double cosTheta = cos(theta);

  /* D = T*cos(phi)*sin(theta) + B*sin(phi)*sin(theta) + N*cos(theta) */
    float cos_theta = cosf(theta);
    vec3f T = tangent * cosf(phi) * sinf(theta);
    vec3f B = binormal * sinf(phi) * sinf(theta);
    vec3f N = normal * (cos_theta);

    dir = T + B + N;

    //return cos_theta; // PDF = weight
}

// (Modified) Ward glossy BRDF
// http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
// Some from OpenShadingLanguage
static void WardBRDF(vec3f& out, // output
                     float& weight,  // output
                     float ax,
                     float ay,
                     const vec3f&  in,
                     const vec3f&  normal,               // shading normal
                     const vec3f&  geometric_normal)     // geometric normal
{
    float cosNO = dot(normal, in);

    weight = 0.0f;
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;

    if (cosNO > 0.0f) {
        // @todo { Supply tangent vector for true aniso-brdf. }
        vec3f tangent, binormal;

        GenerateBasis(tangent, binormal, normal);

        float randu = randomreal();
        float randv = randomreal();

        float alphaRatio = ay / ax;
        float cosPhi, sinPhi;

        if (randu < 0.25f) {
            float val = 4 * randu;
            float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
            cosPhi = 1 / sqrtf(1 + tanPhi * tanPhi);
            sinPhi = tanPhi * cosPhi;
        } else if (randu < 0.5) {
            float val = 1 - 4 * (0.5f - randu);
            float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
            // phi = (float) M_PI - phi;
            cosPhi = -1 / sqrtf(1 + tanPhi * tanPhi);
            sinPhi = -tanPhi * cosPhi;
        } else if (randu < 0.75f) {
            float val = 4 * (randu - 0.5f);
            float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
            //phi = (float) M_PI + phi;
            cosPhi = -1 / sqrtf(1 + tanPhi * tanPhi);
            sinPhi = tanPhi * cosPhi;
        } else {
            float val = 1 - 4 * (1 - randu);
            float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
            // phi = 2 * (float) M_PI - phi;
            cosPhi = 1 / sqrtf(1 + tanPhi * tanPhi);
            sinPhi = -tanPhi * cosPhi;
        }

        // eq. 6
        // we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
        //                  and sin(atan(x)) == x/sqrt(1+x^2)
        float thetaDenom = (cosPhi * cosPhi) / (ax * ax) + (sinPhi * sinPhi) / (ay * ay);
        float tanTheta2 = -logf(1 - randv) / thetaDenom;
        float cosTheta  = 1 / sqrtf(1 + tanTheta2);
        float sinTheta  = cosTheta * sqrtf(tanTheta2);

        vec3f h; // already normalized becaused expressed from spherical coordinates
        h[0] = sinTheta * cosPhi;
        h[1] = sinTheta * sinPhi;
        h[1] = cosTheta;
        // compute terms that are easier in local space
        float dotx = h[0] / ax;
        float doty = h[1] / ay;
        float dotn = h[2];
        // transform to world space
        h = h[0] * tangent + h[1] * binormal + h[2] * normal;
        // generate the final sample
        float oh = dot(h, in);
        //omega_in->x = 2 * oh * h.x - in.x;
        //omega_in->y = 2 * oh * h.y - in.y;
        //omega_in->z = 2 * oh * h.z - in.z;
        out = 2.f * oh * h - in;

        float ng_dot_wi = dot(geometric_normal, out);
        if (ng_dot_wi > 0.0f) {
            float cosNI = dot(normal, out);
            if (cosNI > 0.0f) {
                // eq. 9
                float exp_arg = (dotx * dotx + doty * doty) / (dotn * dotn);
                float denom = 4 * (float) M_PI * ax * ay * oh * dotn * dotn * dotn;
                //float pdf = expf(-exp_arg) / denom;
                denom = (4 * (float) M_PI * ax * ay * sqrtf(cosNO * cosNI));
                float power = cosNI * expf(-exp_arg) / denom;
                weight = power;
            }
        }
    }
}

bool
ShadePBSbounce(const Scene *scene, const Ray &ray, const Intersection &isect, vec3f& weight, vec3f& newDir)
{
    // Currently available: diffuse + reflection(+glossy reflection)
    const Material& mat = scene->GetMesh().GetMaterial(isect.matID);

    // Preserve Energy conservation for each channel.
    vec3f diffuse = mat.diffuse;
    vec3f reflection = mat.reflection;
    vec3f refraction = mat.refraction;
    float reflectionGlossiness = mat.reflectionGlossiness;
    float refractionGlossiness = 1.0f;//mat.refractionGlossiness;
    bool  fresnel = (refraction[0]>0.f || refraction[1]>0.f || refraction[2]>0.f);//qmat.fresnel; //????
    float ior = mat.ior;

    vec3f in = ray.dir.neg();
    in.normalize();
    
// normal inversed on the car model?
vec3f ns = isect.normal.neg();
    float IdotN = dot(in, ns);
    float eta = (IdotN >= 0.f) ? 1.0 / ior : ior;
    vec3f n = (IdotN >= 0.f) ? ns : ns.neg();
    n.normalize();

    // ks wins, kt next, kd weaks.
    vec3f one(1.0, 1.0, 1.0);
    vec3f ksRGB0 = reflection;
    vec3f ktRGB0 = refraction;
    vec3f ksRGB = ksRGB0;
    vec3f ktRGB = vclamp01((one - ksRGB0) * ktRGB0);
    vec3f kdRGB = vclamp01((one - ksRGB - ktRGB) * diffuse);

    if (fresnel) { // adjust ks and kt energy with fresnel factor. kd is 0 if we have kt
        vec3f sDir, tDir;
        float fresnelKr, fresnelKt;
        fresnel_factor(sDir, tDir, fresnelKr, fresnelKt, in, ns, eta);
    // sDir and tDir not used.

        ksRGB = fresnelKr * ksRGB0;
        ktRGB = fresnelKt * ktRGB0;
        kdRGB = vec3f(0.f);
    }

    float ks = vavg(ksRGB); ks = std::min(1.0f, std::max(0.0f, ks));
    float kt = vavg(ktRGB); kt = std::min(1.0f, std::max(0.0f, kt));
    float kd = vavg(kdRGB); kd = std::min(1.0f, std::max(0.0f, kd));
    
    float kTotal = ks+kt+kd;
    
    if (kTotal == 0.f) return false;
    
    ks /= kTotal;
    kt /= kTotal;
    kd /= kTotal;
    
    float xi = randomreal();

    if (xi < kd) {
        SampleDiffuseIS(newDir, n);
        weight = kdRGB / kd;
    }
    else if (xi < ks+kd) { // reflection
        if (reflectionGlossiness < 1.0) {
            // glossy reflection. WardBRDF

            // larget = sharper.
            float ax = sqrtf(1.0f - reflectionGlossiness); // isotropic
            float ay = ax;

            float wr;

            WardBRDF(newDir, wr, ax, ay, in, n, n);
            
            weight = ksRGB * wr  / ks;

        } else {
            // perfect specular.
            newDir = reflect(in, n);
            weight = ksRGB/ ks;
        }
    }
    else { // refraction
        // Simple Ward refraction.
        // @todo { GGX Glossy transmission. }

        if (refractionGlossiness < 1.0f) {
            // glossy reflection. WardBRDF
      
            // larget = sharper.
            float ax = sqrtf(1.0f - refractionGlossiness); // isotropic
            float ay = ax;

            float wr;

            WardBRDF(newDir, wr, ax, ay, in, n, n);

            weight = ktRGB * wr  / kt;
            newDir = newDir - 2.f * dot(newDir, n) * n;

        } else {
            // perfect transmission.
            bool tir = false;
            newDir = refract(tir, in, n, eta);
            weight = ktRGB / kt;
        }
    }

    return true;
}


vec3f
ShadePBS(const Scene *scene, const Ray &ray, const Intersection &isect)
{
    const int maxDepth = 6;
   
    Ray indirectRay = ray;
    Intersection indirectIsect = isect;
    
    vec3f result(1.f);

// check model orientation, GREEN -> good, RED -> bad
//float vdotn = -dot(ray.dir, isect.normal);
//if (vdotn>0.f) return vec3f(0.f, vdotn, 0.f);
//else return vec3f(-vdotn, 0.f, 0.f);
    
    for (int depth=0; depth<=maxDepth; ++depth)
    {
        if (depth == maxDepth) // max depth we just return an ambient
        {
            //result = result * vec3f(0.01f);
            result = vec3f(0.0f);
        }
        else // shading
        {
            vec3f weight;
            vec3f newDir;

            if (ShadePBSbounce(scene, indirectRay, indirectIsect, weight, newDir))
            {
                result = result * weight;

                // trace indirect ray
                indirectRay.dir = newDir;
                indirectRay.invDir = newDir.neg();
                indirectRay.org = indirectIsect.position + 0.001f * newDir;
                //indirectRay.org = ray.org + ray.dir * isect.t + sray.dir * 0.0001;
                indirectRay.depth = depth + 1;
                indirectIsect.t = 1.0e+30f;

                if (!scene->Traverse(indirectRay, &indirectIsect)) // if no hit, return the environment
                {
                    // env light
                    result = result * scene->GetEnvColor(newDir);
                    break;
                }
            }
            else
            {
                result = vec3f(0.f);
                break;
            }
        }
    }
    
    return result;
}
