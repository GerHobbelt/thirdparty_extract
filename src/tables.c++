//#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

cv::Mat extract_table_find(const std::string& path_pdf)
{
    std::string path_png = path_pdf + ".png";
    std::string command = "mutool convert -o " + path_png + " " + path_pdf;
    int e = system(command.c_str());
    assert(!e);
    
    cv::Mat image = cv::imread(path_png);
    cv::Mat image_grey;
    cv::cvtColor(image, image_grey, cv::COLOR_BGR2GRAY);
    
    // Invert each pixel.
    for (int i=0; i<image_grey.cols; ++i)
    {
        for (int j=0; j<image_grey.rows; ++j)
        {
            auto& p = image_grey.at<unsigned char>(i, j);
            p = 255 - p;
        }
    }
    
    cv::Mat image_grey_threshold;
    cv::adaptiveThreshold(
            image_grey,
            image_grey_threshold,
            255,
            cv::ADAPTIVE_THRESH_GAUSSIAN_C,
            cv::THRESH_BINARY,
            15 /*blocksize*/,
            -2 /*C*/
            );
    
    return image_grey_threshold;
}
