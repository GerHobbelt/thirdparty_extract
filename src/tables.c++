#include <opencv2/opencv.hpp>


struct rect_compare
{
    bool operator()(const cv::Rect& a, const cv::Rect& b) const
    {
        if (a.x != b.x)             return a.x < b.x;
        if (a.y != b.y)             return a.y < b.y;
        if (a.width != b.width)     return a.width < b.width;
        if (a.height != b.height)   return a.height < b.height;
        return false;
    }
};

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
    
    // Find lines
    //
    std::vector<cv::Rect>   vertical_segments;
    std::vector<cv::Rect>   horizontal_segments;
    
    auto vertical_mask = image_grey;
    auto horizontal_mask = image_grey;
    
    int line_scale = 15;
    
    for (int vertical=0; vertical != 2; ++vertical)
    {
        int size = (vertical) ? image_grey_threshold.rows : image_grey_threshold.cols;
        size /= line_scale;
        
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
    
    // End of Find lines.
    
    // contours = find_contours(vertical_mask, horizontal_mask)
    auto mask = vertical_mask + horizontal_mask;
    std::vector<std::vector<unsigned char>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::sort(contours.begin(), contours.end());
    // todo: take first 10 items.
    std::vector<cv::Rect>   contours2;
    for (auto& c: contours)
    {
        std::vector<cv::Point>  c_poly;
        cv::approxPolyDP(c, c_poly, 3 /*epsilon*/, true /*closed*/);
        contours2.push_back(cv::boundingRect(c_poly));
    }
    // contours2 is as returned by find_contours().
    
    /* Lines are in:
        vertical_segments
        horizontal_segments
        vertical_mask
        horizontal_mask
    */
    
    // table_bbox = find_joints(contours, vertical_mask, horizontal_mask)
    auto joints = vertical_mask * horizontal_mask;
    std::map<cv::Rect, std::vector<cv::Point>, rect_compare>    table_bbox;
    for (auto rect: contours2)
    {
        cv::Mat roi(
                joints,
                cv::Range(rect.y, rect.y + rect.height),
                cv::Range(rect.x, rect.x + rect.width)
                );
        std::vector<std::vector<unsigned char>> jc;
        cv::findContours(roi, jc, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
        if (jc.size() < 5)  continue;
        std::vector<cv::Point>  joint_coords;
        for (auto& j: jc)
        {
            auto rectj = cv::boundingRect(j);
            joint_coords.push_back(
                    cv::Point(
                            rect.x + (2*rectj.x + rectj.width) / 2,
                            rect.y + (2*rectj.y + rectj.height) / 2
                            )
                    );
        }
        table_bbox[rect] = joint_coords;
    }
    // tables is table_bbox.
    
    auto table_bbox_unscaled = table_bbox;
}
