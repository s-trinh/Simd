#include <iostream>
#include <string>

#include "Simd/SimdDetection.hpp"

int main(int argc, char * argv[])
{
    int width = 640, height = 480;
    unsigned char * bitmap = new unsigned char[width*height];
    memset(bitmap, 0, width*height);

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

    return 0;
}
