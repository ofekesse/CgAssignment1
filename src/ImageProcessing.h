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

    /* Canny edge detection on a grayscale image. Output: 1 channel, each pixel 0 or 255.
       Stages: Gaussian filter -> Sobel gradient -> non-max suppression -> double threshold -> hysteresis. */
    Image Canny(const Image& gray, float lowRatio = 0.20f, float highRatio = 0.26f,
                 int blurRadius = 2, float sigma = 1.4f);  // radius 2 = 5x5 kernel, radius 1 = 3x3

    /* Halftone: every gray pixel becomes a 2x2 black/white block (output is 2x wider and 2x taller,
       i.e. 4x the pixels). Output: 1 channel, each pixel 0 or 255. */
    Image Halftone(const Image& gray);

    /* Writes a 1- or 4-channel image as PNG. */
    bool SavePNG(const std::string& path, const Image& img);

    /* Writes a binary (0/255) image as comma-separated (no spaces) 0/1 values. */
    bool SaveBinaryTxt(const std::string& path, const Image& bw);

    /* Writes a single channel image as comma-separated (no spaces) values in 0..15 (value / 16). */
    bool SaveTxt(const std::string& path, const Image& gray);
}
