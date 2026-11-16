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

    Image Halftone(const Image& gray)
    {
        const int W = gray.width, H = gray.height;

        Image out;
        out.width = W * 2;
        out.height = H * 2;
        out.channels = 1;
        out.data.assign((size_t)out.width * out.height, 0); // initialize a 512x512 output image with all black pixels (0)

        // Dot order inside the 2x2 block: level L turns on every cell whose rank is < L
        //   rank: [0 2]   level 1 -> TL, level 2 -> TL+BR, level 3 -> TL+BR+TR, level 4 -> all four
        //         [3 1]
        const int rank[2][2] = { { 0, 2 }, { 3, 1 } };

        for (int y = 0; y < H; ++y)          // each input row fills TWO output rows (2y and 2y+1)
        {
            unsigned char* top = &out.data[(size_t)(2 * y) * out.width];
            unsigned char* bottom = top + out.width;
            for (int x = 0; x < W; ++x)
            {
                int level = gray.data[(size_t)y * W + x] * 5 / 256;   // 0..4 (five gray levels)
                top[2 * x]        = level > rank[0][0] ? 255 : 0; // top-left
                top[2 * x + 1]    = level > rank[0][1] ? 255 : 0; // top-right
                bottom[2 * x]     = level > rank[1][0] ? 255 : 0; // bottom-left
                bottom[2 * x + 1] = level > rank[1][1] ? 255 : 0; // bottom-right
            }
        }
        return out;
    }

    Image FloydSteinberg(const Image& gray)
    {
        const int W = gray.width, H = gray.height;

        // Working copy in float: diffused errors are fractional and can push a value outside 0..255
        std::vector<float> work(gray.data.begin(), gray.data.end());

        Image out;
        out.width = W;
        out.height = H;
        out.channels = 1;
        out.data.resize((size_t)W * H);

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                size_t i = (size_t)y * W + x;
                float oldVal = std::clamp(work[i], 0.0f, 255.0f);

                // Quantize to the nearest of 16 levels (0..15), displayed as level * 17 (0..255)
                int level = (int)(oldVal / 17.0f + 0.5f);
                float newVal = level * 17.0f;
                out.data[i] = (unsigned char)newVal;

                float err = oldVal - newVal;

                // Neighbours that receive the error: right (7/16), below-left (3/16), below (5/16), below-right (1/16)
                struct N { int dx, dy; float w; };
                const N all[4] = { { 1, 0, 7.0f / 16 }, { -1, 1, 3.0f / 16 }, { 0, 1, 5.0f / 16 }, { 1, 1, 1.0f / 16 } };

                bool avail[4];
                int count = 0;
                for (int n = 0; n < 4; ++n)
                {
                    int nx = x + all[n].dx, ny = y + all[n].dy;
                    avail[n] = nx >= 0 && nx < W && ny < H;
                    if (avail[n]) ++count;
                }
                if (count == 0) continue;   // bottom-right corner: nowhere to diffuse

                for (int n = 0; n < 4; ++n)
                {
                    if (!avail[n]) continue;
                    // Full neighbourhood: standard weights. Missing neighbours (image edges): split equally.
                    float share = (count == 4) ? all[n].w : 1.0f / count;
                    work[(size_t)(y + all[n].dy) * W + (x + all[n].dx)] += err * share;
                }
            }
        return out;
    }
}
