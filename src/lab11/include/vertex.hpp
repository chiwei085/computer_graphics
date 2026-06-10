#pragma once

#include <GL/gl.h>

#include <array>

namespace demo
{

struct Vtx
{
    float x, y, z;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float u = 0.0f, v = 0.0f;

    Vtx& n(float nx_, float ny_, float nz_) {
        nx = nx_;
        ny = ny_;
        nz = nz_;
        return *this;
    }
    Vtx& uv(float u_, float v_) {
        u = u_;
        v = v_;
        return *this;
    }

    void emit() const {
        glNormal3f(nx, ny, nz);
        glTexCoord2f(u, v);
        glVertex3f(x, y, z);
    }
};

inline Vtx vtx(float x, float y, float z) {
    return {x, y, z};
}

// Quad face with a shared normal — set it once, position+uv per vertex.
struct Face
{
    float nx, ny, nz;
    std::array<Vtx, 4> verts{};
    int idx = 0;

    Face& v(float x, float y, float z, float u = 0.0f, float vv = 0.0f) {
        verts[idx++] = vtx(x, y, z).n(nx, ny, nz).uv(u, vv);
        return *this;
    }
};

inline Face face(float nx, float ny, float nz) {
    return {nx, ny, nz};
}

// Axis-aligned boxes — describe bounds once, normals are derived.
struct FlatBox
{
    float x0, y0, z0, x1, y1, z1, ur, vr;
};
struct SmoothBox
{
    float x0, y0, z0, x1, y1, z1, ur, vr;
};

inline FlatBox flat_box(float x0, float y0, float z0, float x1, float y1,
                        float z1, float ur = 1.0f, float vr = 1.0f) {
    return {x0, y0, z0, x1, y1, z1, ur, vr};
}

inline SmoothBox smooth_box(float x0, float y0, float z0, float x1, float y1,
                            float z1, float ur = 1.0f, float vr = 1.0f) {
    return {x0, y0, z0, x1, y1, z1, ur, vr};
}

// Shared box geometry: the 6 faces of an axis-aligned box. Each corner selects
// a bound per axis (0 = min, 1 = max) and carries its UV (0/1, scaled by the
// box's repeat). Flat shading emits the face normal; smooth shading emits the
// per-corner sign normal (min -> -1, max -> +1). Both modes ride this one
// table.
struct BoxCorner
{
    int bx, by, bz;
    float u, v;
};
struct BoxFace
{
    float nx, ny, nz;
    BoxCorner corners[4];
};

inline constexpr BoxFace kBoxFaces[6] = {
    {0,
     0,
     1,
     {{0, 0, 1, 0, 0}, {1, 0, 1, 1, 0}, {1, 1, 1, 1, 1}, {0, 1, 1, 0, 1}}},
    {0,
     0,
     -1,
     {{1, 0, 0, 0, 0}, {0, 0, 0, 1, 0}, {0, 1, 0, 1, 1}, {1, 1, 0, 0, 1}}},
    {0,
     1,
     0,
     {{0, 1, 1, 0, 0}, {1, 1, 1, 1, 0}, {1, 1, 0, 1, 1}, {0, 1, 0, 0, 1}}},
    {0,
     -1,
     0,
     {{0, 0, 0, 0, 0}, {1, 0, 0, 1, 0}, {1, 0, 1, 1, 1}, {0, 0, 1, 0, 1}}},
    {1,
     0,
     0,
     {{1, 0, 1, 0, 0}, {1, 0, 0, 1, 0}, {1, 1, 0, 1, 1}, {1, 1, 1, 0, 1}}},
    {-1,
     0,
     0,
     {{0, 0, 0, 0, 0}, {0, 0, 1, 1, 0}, {0, 1, 1, 1, 1}, {0, 1, 0, 0, 1}}},
};

// RAII quad pipeline: glBegin on construct, glEnd on destruct.
// Pipe primitives in with |; each is emitted immediately — no storage overhead.
// C++17 guaranteed copy elision lets quads() return by value despite deleted
// copy.
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
        for (const auto& v : f.verts) v.emit();
        return *this;
    }

    Quads& operator|(const FlatBox& b) {
        for (const BoxFace& f : kBoxFaces) {
            for (const BoxCorner& c : f.corners) {
                (*this) | vtx(c.bx ? b.x1 : b.x0, c.by ? b.y1 : b.y0,
                              c.bz ? b.z1 : b.z0)
                              .n(f.nx, f.ny, f.nz)
                              .uv(c.u * b.ur, c.v * b.vr);
            }
        }
        return *this;
    }

    Quads& operator|(const SmoothBox& b) {
        // Corner normals = sign of each axis: min→-1, max→+1. GL_NORMALIZE
        // renormalizes.
        for (const BoxFace& f : kBoxFaces) {
            for (const BoxCorner& c : f.corners) {
                (*this) | vtx(c.bx ? b.x1 : b.x0, c.by ? b.y1 : b.y0,
                              c.bz ? b.z1 : b.z0)
                              .n(c.bx ? 1.0f : -1.0f, c.by ? 1.0f : -1.0f,
                                 c.bz ? 1.0f : -1.0f)
                              .uv(c.u * b.ur, c.v * b.vr);
            }
        }
        return *this;
    }
};

inline Quads quads() {
    return {};
}

}  // namespace demo
