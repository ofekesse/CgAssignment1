#pragma once

#include <string>
#include <vector>

/* Simple CPU image container. Row 0 is the TOP row of the image. */
struct Image
{
    int width = 0;
    int height = 0;
    int channels = 0;                 // 4 = RGBA, 1 = grayscale
    std::vector<unsigned char> data;  // width * height * channels bytes
};

namespace ImageProcessing
{
    /* Loads an image as RGBA (no vertical flip). Returns false on failure. */
    bool LoadRGBA(const std::string& path, Image& out);

    /* RGBA -> single channel 8-bit grayscale, weighted luminance, clamped to [0,255]. */
    Image ToGrayscale(const Image& rgba);

    /* Writes a 1- or 4-channel image as PNG. */
    bool SavePNG(const std::string& path, const Image& img);

    /* Writes a single channel image as comma-separated (no spaces) values in 0..15 (value / 16). */
    bool SaveTxt(const std::string& path, const Image& gray);
}
