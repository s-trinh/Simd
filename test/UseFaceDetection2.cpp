#include <iostream>
#include <string>

#include "Simd/SimdDetection.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char * argv[])
{
    std::string filename = "";
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
    unsigned char * bitmap = new unsigned char[width*height];
    memcpy(bitmap, image, width*height);

    stbi_image_free(image);

    std::cout << "width: " << width << " ; height: " << height << " ; channels: " << std::endl;

    SimdSetThreadNumber(1);

    typedef Simd::Detection<Simd::Allocator> Detection;
    Detection detection;
    if (detection.Load("haar_face_0.xml")) {
        Detection::View image(width, height, Detection::View::Gray8, bitmap);
        detection.Init(image.Size(), 1.1, Detection::Size(30,30));

        Detection::Objects objects;
        detection.Detect(image, objects);

        for (size_t i = 0; i < objects.size(); i++) {
            std::cout << i << ") " << objects[i].rect.Left() << ", " << objects[i].rect.Top() << " ; "
                      << objects[i].rect.Width() << "x" << objects[i].rect.Height() << std::endl;
        }
    }

    delete[] bitmap;

    return EXIT_SUCCESS;
}
