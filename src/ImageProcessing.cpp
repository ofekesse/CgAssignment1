#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <ImageProcessing.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace ImageProcessing
{
    bool LoadRGBA(const std::string& path, Image& out)
    {
        // Make sure the buffer is NOT flipped, so saved PNGs are not upside down
        stbi_set_flip_vertically_on_load(0);

        int w = 0, h = 0, comp = 0;
        unsigned char* buf = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!buf)
        {
            std::cerr << "Failed to load image: " << path << std::endl;
            return false;
        }
        out.width = w;
        out.height = h;
        out.channels = 4;
        out.data.assign(buf, buf + (size_t)w * h * 4);
        stbi_image_free(buf);
        return true;
    }

    Image ToGrayscale(const Image& rgba)
    {
        Image gray;
        gray.width = rgba.width;
        gray.height = rgba.height;
        gray.channels = 1;
        gray.data.resize((size_t)rgba.width * rgba.height);

        for (size_t i = 0; i < gray.data.size(); ++i)
        {
            const unsigned char* p = &rgba.data[i * 4];
            /* weighted luminance (NTSC/PAL) */
            float g = 0.2989f * p[0] + 0.5870f * p[1] + 0.1140f * p[2]; 
            gray.data[i] = (unsigned char)std::clamp(g + 0.5f, 0.0f, 255.0f);
        }
        return gray;
    }

    bool SavePNG(const std::string& path, const Image& img)
    {
        int ok = stbi_write_png(path.c_str(), img.width, img.height, img.channels,
                                img.data.data(), img.width * img.channels);
        if (!ok)
            std::cerr << "Failed to write " << path << std::endl;
        return ok != 0;
    }

    bool SaveTxt(const std::string& path, const Image& gray)
    {
        std::ofstream f(path);
        if (!f)
        {
            std::cerr << "Failed to write " << path << std::endl;
            return false;
        }
        for (size_t i = 0; i < gray.data.size(); ++i)
        {
            if (i) f << ',';
            f << (int)(gray.data[i] / 16);
        }
        return true;
    }

    bool SaveBinaryTxt(const std::string& path, const Image& bw)
    {
        std::ofstream f(path);
        if (!f)
        {
            std::cerr << "Failed to write " << path << std::endl;
            return false;
        }
        for (size_t i = 0; i < bw.data.size(); ++i)
        {
            if (i) f << ',';
            f << (bw.data[i] ? 1 : 0);
        }
        return true;
    }

    Image Canny(const Image& gray, float lowRatio, float highRatio, int blurRadius, float sigma)
    {
        const int W = gray.width, H = gray.height;
        const size_t N = (size_t)W * H;
        auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

        // ---- 1. Noise reduction: separable 5x5 OR 3x3 Gaussian (sigma = 1.4) ----
        /* Building the Gaussian kernel */
        const int R = blurRadius;
        std::vector<float> k(2 * R + 1);
        float sum = 0.0f;
        for (int i = -R; i <= R; ++i)
        {
            k[i + R] = std::exp(-(i * i) / (2.0f * sigma * sigma));
            sum += k[i + R];
        }
        for (float& v : k) v /= sum; // normalize kernel (sum of all elements = 1)
        /* Finished building the Gaussian kernel */

        // Horizontal and vertical passes of the separable Gaussian filter per pixel
        std::vector<float> tmp(N), blur(N);
        for (int y = 0; y < H; ++y)          // horizontal pass
            for (int x = 0; x < W; ++x)
            {
                float s = 0.0f;
                for (int i = -R; i <= R; ++i)
                    s += k[i + R] * gray.data[(size_t)y * W + clampi(x + i, 0, W - 1)]; // clampi pins coordinates to the nearest valid pixel, so edge pixels reuse their edge value
                tmp[(size_t)y * W + x] = s;
            }
        for (int y = 0; y < H; ++y)          // vertical pass
            for (int x = 0; x < W; ++x)
            {
                float s = 0.0f;
                for (int i = -R; i <= R; ++i)
                    s += k[i + R] * tmp[(size_t)clampi(y + i, 0, H - 1) * W + x];
                blur[(size_t)y * W + x] = s;
            }

        // ---- 2. Gradient (Sobel) along x and y: magnitude + direction ----
        std::vector<float> mag(N, 0.0f);
        std::vector<unsigned char> dir(N, 0);   // 0 = horizontal, 1 = 45deg, 2 = vertical, 3 = 135deg
        float maxMag = 0.0f;
        auto B = [&](int x, int y) { return blur[(size_t)clampi(y, 0, H - 1) * W + clampi(x, 0, W - 1)]; };
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                // Compute the Sobel gradient along x and y, then compute the magnitude and direction
                float gx = -B(x - 1, y - 1) + B(x + 1, y - 1) - 2 * B(x - 1, y) + 2 * B(x + 1, y) - B(x - 1, y + 1) + B(x + 1, y + 1);
                float gy = -B(x - 1, y - 1) - 2 * B(x, y - 1) - B(x + 1, y - 1) + B(x - 1, y + 1) + 2 * B(x, y + 1) + B(x + 1, y + 1);
                float m = std::sqrt(gx * gx + gy * gy); // magnitude = L2 norm
                mag[(size_t)y * W + x] = m;
                maxMag = std::max(maxMag, m); // The maximum magnitude is used to compute the high and low thresholds for the double thresholding step

                float a = std::atan2(gy, gx) * 180.0f / 3.14159265f;   // converted from radians to -180..180 degrees
                if (a < 0) a += 180.0f;                                // shift to 0..180 if negative because the gradient direction is symmetric (180deg = 0deg)
                unsigned char d;
                /* Assign gradient direction based on angle */
                if (a < 22.5f || a >= 157.5f) d = 0;
                else if (a < 67.5f) d = 1;
                else if (a < 112.5f) d = 2;
                else d = 3;
                dir[(size_t)y * W + x] = d;
            }

        // ---- 3. Non-maximum suppression: keep only local maxima along the gradient direction ----
        std::vector<float> nms(N, 0.0f);
        const int dx[4] = { 1, 1, 0, -1 };   // neighbour offset along the gradient, per direction
        const int dy[4] = { 0, 1, 1, 1 };
        auto M = [&](int x, int y) { return (x < 0 || y < 0 || x >= W || y >= H) ? 0.0f : mag[(size_t)y * W + x]; };
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                int d = dir[(size_t)y * W + x];
                float m = mag[(size_t)y * W + x];
                if (m >= M(x + dx[d], y + dy[d]) && m >= M(x - dx[d], y - dy[d]))
                    nms[(size_t)y * W + x] = m;
            }

        // ---- 4. Double threshold: 0 = none, 1 = weak, 2 = strong ----
        const float high = highRatio * maxMag, low = lowRatio * maxMag; // The high and low thresholds are computed as a fraction of the maximum magnitude found in the previous step
        std::vector<unsigned char> cls(N, 0);
        std::vector<size_t> stack;
        for (size_t i = 0; i < N; ++i)
        {
            if (nms[i] >= high) { cls[i] = 2; stack.push_back(i); }
            else if (nms[i] >= low) cls[i] = 1;
        }

        // ---- 5. Hysteresis: weak pixels survive only if connected (8-neighbourhood) to a strong one ----
        while (!stack.empty())
        {
            size_t i = stack.back();
            stack.pop_back();
            int x = (int)(i % W), y = (int)(i / W);
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox)
                {
                    int nx = x + ox, ny = y + oy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue; // skip out-of-bounds pixels
                    size_t j = (size_t)ny * W + nx;
                    if (cls[j] == 1) { cls[j] = 2; stack.push_back(j); }
                }
        }
        
        Image out;
        out.width = W;
        out.height = H;
        out.channels = 1;
        out.data.resize(N);
        for (size_t i = 0; i < N; ++i) out.data[i] = (cls[i] == 2) ? 255 : 0;
        return out;
    }
}
