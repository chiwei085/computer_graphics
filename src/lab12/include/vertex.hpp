#pragma once

#include <GL/gl.h>

#include <array>

namespace lab12
{

struct Vtx
{
    float x, y, z;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;

    Vtx& n(float nx_, float ny_, float nz_) {
        nx = nx_;
        ny = ny_;
        nz = nz_;
        return *this;
    }

    void emit() const {
        glNormal3f(nx, ny, nz);
        glVertex3f(x, y, z);
    }
};

inline Vtx vtx(float x, float y, float z) {
    return {x, y, z};
}

struct Face
{
    float nx, ny, nz;
    std::array<Vtx, 4> verts{};
    int idx = 0;

    Face& v(float x, float y, float z) {
        verts[idx++] = vtx(x, y, z).n(nx, ny, nz);
        return *this;
    }
};

inline Face face(float nx, float ny, float nz) {
    return {nx, ny, nz};
}

struct FlatBox
{
    float x0, y0, z0, x1, y1, z1;
};

inline FlatBox flat_box(float x0, float y0, float z0, float x1, float y1,
                        float z1) {
    return {x0, y0, z0, x1, y1, z1};
}

struct BoxCorner
{
    int bx, by, bz;
};

struct BoxFace
{
    float nx, ny, nz;
    BoxCorner corners[4];
};

inline constexpr BoxFace kBoxFaces[6] = {
    {0, 0, 1, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},
    {0, 0, -1, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}},
    {0, 1, 0, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}},
    {0, -1, 0, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}},
    {1, 0, 0, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}},
    {-1, 0, 0, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}},
};

struct Quads
{
    Quads() { glBegin(GL_QUADS); }
    ~Quads() { glEnd(); }
    Quads(const Quads&) = delete;
    Quads& operator=(const Quads&) = delete;

    Quads& operator|(const Vtx& v) {
        v.emit();
        return *this;
    }

    Quads& operator|(const Face& f) {
        for (const Vtx& v : f.verts) v.emit();
        return *this;
    }

    Quads& operator|(const FlatBox& b) {
        for (const BoxFace& f : kBoxFaces) {
            for (const BoxCorner& c : f.corners) {
                (*this) | vtx(c.bx ? b.x1 : b.x0, c.by ? b.y1 : b.y0,
                              c.bz ? b.z1 : b.z0)
                              .n(f.nx, f.ny, f.nz);
            }
        }
        return *this;
    }
};

inline Quads quads() {
    return {};
}

}  // namespace lab12
