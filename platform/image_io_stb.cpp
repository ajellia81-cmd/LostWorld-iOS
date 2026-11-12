// iOS port of image_io.cpp: PNG (and any picture stb_image reads) through
// stb_image / stb_image_write instead of Windows Imaging Component (pictures
// of other formats, which WIC also read, are PNG, JPEG or BMP in this repository)
#include "image_io.h"

#include <windows.h>   // (platform/compat: FindFirstFileA)

#include <algorithm>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"

bool png_load(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba) {
    int n = 0;
    stbi_uc* px = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!px) return false;
    rgba.assign(px, px + size_t(w) * size_t(h) * 4);
    stbi_image_free(px);
    return true;
}

bool png_save(const std::string& path, int w, int h, const uint8_t* rgba) {
    return stbi_write_png(path.c_str(), w, h, 4, rgba, w * 4) != 0;
}

// 0xAARRGGBB words: BGRA bytes in memory
static void swap_rb(uint8_t* p, size_t pixels) {
    for (size_t i = 0; i < pixels; i++) std::swap(p[i * 4], p[i * 4 + 2]);
}

bool image_load_bgra(const std::wstring& path, int& w, int& h, std::vector<uint32_t>& px) {
    std::string utf8;
    for (wchar_t c : path) {   // (the game's paths are ASCII)
        if (uint32_t(c) < 0x80) utf8 += char(c);
        else utf8 += '?';
    }
    std::vector<uint8_t> rgba;
    if (!png_load(utf8, w, h, rgba)) return false;
    swap_rb(rgba.data(), rgba.size() / 4);
    px.resize(rgba.size() / 4);
    memcpy(px.data(), rgba.data(), rgba.size());
    return true;
}

bool png_save_bgra(const std::string& path, int w, int h, const uint32_t* px) {
    std::vector<uint8_t> rgba(reinterpret_cast<const uint8_t*>(px), reinterpret_cast<const uint8_t*>(px) + size_t(w) * h * 4);
    swap_rb(rgba.data(), size_t(w) * h);
    return png_save(path, w, h, rgba.data());
}

bool bmp_save(const std::string& path, int w, int h, const void* bgra) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + DWORD(w) * DWORD(h) * 4;
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h;   // top-down
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    bool ok = fwrite(&fh, sizeof(fh), 1, f) == 1 && fwrite(&ih, sizeof(ih), 1, f) == 1 &&
              fwrite(bgra, 4, size_t(w) * h, f) == size_t(w) * h;
    return fclose(f) == 0 && ok;
}

std::vector<std::string> list_files(const std::string& dir, const char* pattern) {
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA((dir + "/" + pattern).c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return out;
    do {
        out.push_back(fd.cFileName);
    } while (FindNextFileA(find, &fd));
    FindClose(find);
    return out;
}

void list_hashes(const std::string& dir, std::unordered_set<uint64_t>& out) {
    for (const std::string& name : list_files(dir, "*.png")) {
        unsigned long long hash = 0;
        if (sscanf(name.c_str(), "%16llx", &hash) == 1) out.insert(hash);
    }
}
