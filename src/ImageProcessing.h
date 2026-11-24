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

    /* Optional debug output of Canny: the image after each stage (all 1 channel, 0..255). */
    struct CannyStages
    {
        Image gradient;     // 1. gradient magnitude, normalized to 0..255
        Image nms;          // 2. after non-maximum suppression, same normalization
        Image threshold;    // 3. double threshold: 0 = none, 128 = weak, 255 = strong
        Image hysteresis;   // 4. after hysteresis (the final result)
    };

    /* Canny edge detection on a grayscale image. Output: 1 channel, each pixel 0 or 255.
       Stages: Gaussian filter -> Sobel gradient -> non-max suppression -> double threshold -> hysteresis. */
    Image Canny(const Image& gray, float lowRatio = 0.20f, float highRatio = 0.26f,
                 int blurRadius = 2, float sigma = 1.4f,
                 CannyStages* stages = nullptr);  // if not null, filled with the 4 stage images; radius 2 = 5x5 kernel, radius 1 = 3x3

    /* Halftone: every gray pixel becomes a 2x2 black/white block (output is 2x wider and 2x taller,
       i.e. 4x the pixels). Output: 1 channel, each pixel 0 or 255. */
    Image Halftone(const Image& gray);

    /* Floyd-Steinberg error diffusion: 256 gray levels -> 16 levels. Output: 1 channel, values
       are level * 17 (0, 17, ..., 255) so it displays correctly and SaveTxt gives 0..15. */
    Image FloydSteinberg(const Image& gray);

    /* Writes a 1- or 4-channel image as PNG. */
    bool SavePNG(const std::string& path, const Image& img);

    /* Writes a binary (0/255) image as comma-separated (no spaces) 0/1 values. */
    bool SaveBinaryTxt(const std::string& path, const Image& bw);

    /* Writes a single channel image as comma-separated (no spaces) values in 0..15 (value / 16). */
    bool SaveTxt(const std::string& path, const Image& gray);
}
