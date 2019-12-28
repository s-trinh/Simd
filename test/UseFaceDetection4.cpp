#include <iostream>
#include <string>

#include "Simd/SimdDetection.hpp"
#include "Simd/SimdDrawing.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main(int argc, char * argv[])
{
    std::string filename = "1280px-Solvay_conference_1927.png";
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--input" && i+1 < argc) {
            filename = std::string(argv[i+1]);
        }
    }

    int width = 0, height = 0, channels = 0;
    unsigned char *image = stbi_load(filename.c_str(), &width, &height, &channels, STBI_grey);
    if (image == NULL) {
        std::cerr << "Cannot read: " << filename << std::endl;
        return EXIT_FAILURE;
    }

    //copy
    std::vector<unsigned char> bitmap(width*height);
    memcpy(bitmap.data(), image, width*height);

    stbi_image_free(image);

    std::cout << "width: " << width << " ; height: " << height << " ; channels: " << std::endl;

    SimdSetThreadNumber(1);

    typedef Simd::Detection<Simd::Allocator> Detection;
    Detection detection;
    if (detection.Load("haar_face_0.xml")) {
        Detection::View image(width, height, Detection::View::Gray8, bitmap.data());
        detection.Init(image.Size(), 1.1, Detection::Size(30,30));

        Detection::Objects objects;
        detection.Detect(image, objects);

        for (size_t i = 0; i < objects.size(); i++) {
            std::cout << i << ") " << objects[i].rect.Left() << ", " << objects[i].rect.Top() << " ; "
                      << objects[i].rect.Width() << "x" << objects[i].rect.Height() << std::endl;
        }

        unsigned char * bgr = new unsigned char[width*height*3];
        SimdGrayToBgr(bitmap.data(), width, height, width, bgr, width*3);

        Detection::View canvas(width, height, width*3, Detection::View::Bgr24, bgr);

        for (size_t i = 0; i < objects.size(); i++) {
            Simd::DrawRectangle(canvas, objects[i].rect, Simd::Pixel::Bgr24(255, 0, 0), 3);
        }

        const int stride_in_bytes = width*3;
        int res = stbi_write_png("faces_detection.png", width, height, STBI_rgb, bgr, stride_in_bytes);
        if (res == 0) {
            std::cerr << "Cannot write detected faces." << std::endl;
            return EXIT_FAILURE;
        }
        delete[] bgr;
    }

    return EXIT_SUCCESS;
}
