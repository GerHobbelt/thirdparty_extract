#include <opencv2/opencv.hpp>

void extract_table_find(const std::string& path_pdf)
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
    
    std::vector<cv::Rect>   vertical_segments;
    std::vector<cv::Rect>   horizontal_segments;
    
    auto vertical_mask = image_grey;
    auto horizontal_mask = image_grey;
    
    for (int vertical=0; vertical != 2; ++vertical)
    {
        int size = (vertical) ? image_grey_threshold.rows : image_grey_threshold.cols;
        cv::Mat element = cv::getStructuringElement(
                cv::MORPH_RECT,
                (vertical) ? cv::Point(1, size) : cv::Point(size, 1)
                );
        auto image_grey_threshold2 = image_grey_threshold;    // fixme: avoid copy.
        cv::erode(image_grey_threshold, image_grey_threshold2, element);
        auto threshold = image_grey_threshold2;
        cv::dilate(image_grey_threshold2, threshold, element);
        auto dmask = threshold;
        cv::dilate(threshold, dmask, element, cv::Point(-1, -1) /*anchor*/, 0 /*iterations*/);
        
        if (vertical)   vertical_mask = dmask;
        else            horizontal_mask = dmask;
        
        //cv::OutputArrayOfArrays contours;
        std::vector<std::vector<unsigned char>> contours;
        cv::findContours(threshold, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        for (auto& c: contours)
        {
            cv::Rect    bounding_rect = cv::boundingRect(c);
            int x1 = bounding_rect.x;
            int x2 = bounding_rect.x + bounding_rect.width;
            int y1 = bounding_rect.y;
            int y2 = bounding_rect.y + bounding_rect.height;
            if (vertical)
            {
                vertical_segments.push_back(cv::Rect((x1 + x2) / 2, y2, (x1 + x2) / 2, y1));
            }
            else
            {
                horizontal_segments.push_back(cv::Rect(x1, (y1 + y2) / 2, x2, (y1 + y2) / 2));
            }
        }
    }
    
    
}
