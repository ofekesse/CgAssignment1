#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <ImageProcessing.h>

#include <algorithm>
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
}
