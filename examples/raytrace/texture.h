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
#ifndef OSD_RAYTRACE_TEXTURE_H
#define OSD_RAYTRACE_TEXTURE_H

class Texture
{
public:
    typedef enum {
        FORMAT_BYTE,
        FORMAT_FLOAT,
    } Format;

    typedef enum {
        COORDINATE_LONGLAT,
        COORDINATE_ANGULAR,
    } Coordinate;

    Texture() {
        // Make invalid texture
        m_width = -1;
        m_height = -1;
        m_image = NULL;
        m_components = -1;
        m_coordinate = COORDINATE_LONGLAT;
    }

    Texture(const unsigned char *image, int width, int height,
            int components, Format format, float gamma = 1.0f,
            Coordinate coord = COORDINATE_LONGLAT) {
        Set(image, width, height, components, format, gamma, coord);
    }

    ~Texture() { }

    void Set(const unsigned char *image, int width, int height,
             int components, Format format, float gamma = 1.0f,
             Coordinate coord = COORDINATE_LONGLAT) {
        m_width         = width;
        m_height        = height;
        m_image         = image;
        m_invWidth      = 1.0f / width;
        m_invHeight     = 1.0f / height;
        m_components    = components;
        m_format        = format;
        m_invGamma      = 1.0f / gamma;
        m_coordinate    = coord;
    }

    int width() const {
        return m_width;
    }

    int height() const {
        return m_height;
    }

    int components() const {
        return m_components;
    }

    Coordinate coordinate() const {
        return m_coordinate;
    }

    const unsigned char* image() const {
        return m_image;
    }

    // Bilinear textel fetch.
    void fetch(float *rgba, float u, float v) const;

    // Fetch filtered (i, j), (i+1, j), (i, j+1) texel.
    void fetchD(float *rgba0, float *rgba1, float *rgba2, float u, float v) const;

    bool IsValid() const {
        return (m_image != NULL) && (m_width > 0) && (m_height > 0);
    }

private:

    int             m_width;
    int             m_height;
    float           m_invWidth;
    float           m_invHeight;
    int             m_components;
    const unsigned char*  m_image;
    float           m_invGamma;
    Format          m_format;
    Coordinate      m_coordinate;
};

//
// A sampler class for longlat map texture.
//
class LongLatMapSampler
{
public:
    LongLatMapSampler() {};
    ~LongLatMapSampler() {};

    static void Sample(float* rgba, float dir[3], const Texture* texture);
};

#endif  // OSD_RAYTRACE_TEXTURE_H
