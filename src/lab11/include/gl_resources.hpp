#pragma once

#include <GL/freeglut.h>
#include <ft2build.h>

#include <array>
#include <filesystem>

#include FT_FREETYPE_H

namespace demo
{

// Printable ASCII range [32, 127) that the font atlas rasterizes.
inline constexpr unsigned char kFirstGlyph = 32;
inline constexpr unsigned char kLastGlyph = 127;
inline constexpr int kGlyphCount = kLastGlyph - kFirstGlyph;

class TextureHandle
{
public:
    TextureHandle() = default;
    explicit TextureHandle(GLuint id);
    ~TextureHandle();

    TextureHandle(const TextureHandle&) = delete;
    TextureHandle& operator=(const TextureHandle&) = delete;

    TextureHandle(TextureHandle&& other) noexcept;
    TextureHandle& operator=(TextureHandle&& other) noexcept;

    [[nodiscard]] GLuint id() const;
    [[nodiscard]] bool valid() const;
    void reset(GLuint id = 0);

private:
    GLuint id_ = 0;
};

struct TextureSet
{
    TextureHandle floor{};
    TextureHandle frame{};
    TextureHandle panel{};
    TextureHandle stand{};
};

struct Glyph
{
    TextureHandle texture{};
    int width = 0;
    int height = 0;
    int bearing_x = 0;
    int bearing_y = 0;
    unsigned int advance = 0;
};

class FontAtlas
{
public:
    FontAtlas() = default;
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;
    FontAtlas(FontAtlas&&) = delete;
    FontAtlas& operator=(FontAtlas&&) = delete;

    bool load(const std::filesystem::path& path, int size_px);
    // Returns the glyph for a printable ASCII char, or nullptr if out of range.
    [[nodiscard]] const Glyph* glyph(char ch) const;

private:
    void release();

    FT_Library library_ = nullptr;
    FT_Face face_ = nullptr;
    std::array<Glyph, kGlyphCount> glyphs_{};
};

bool load_texture_file(TextureHandle& texture,
                       const std::filesystem::path& path);

}  // namespace demo
