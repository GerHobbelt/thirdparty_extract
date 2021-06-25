#include <opencv2/opencv.hpp>


struct rect_compare
{
    bool operator()(const cv::Rect& a, const cv::Rect& b) const
    {
        if (a.y != b.y)             return a.y < b.y;
        if (a.x != b.x)             return a.x < b.x;
        if (a.width != b.width)     return a.width < b.width;
        if (a.height != b.height)   return a.height < b.height;
        return false;
    }
};

struct Cell
{
    //cv::Rect    rect;
    int x1;
    int y1;
    int x2;
    int y2;
    cv::Point   lb;
    cv::Point   lt;
    cv::Point   rb;
    cv::Point   rt;
    bool        left = false;
    bool        right = false;
    bool        top = false;
    bool        bottom = false;
    bool        hspan = false;
    bool        vspan = false;

    Cell(int x1, int y1, int x2, int y2)
    :
    x1(x1),
    y1(y1),
    x2(x2),
    y2(y2),
    lb(x1, y1),
    lt(x1, y2),
    rb(x2, y2),
    rt(x2, y2)
    {
    }

    int bound()
    {
        int ret = 0;
        if (left)   ret += 1;
        if (right)  ret += 1;
        if (top)    ret += 1;
        if (bottom) ret += 1;
        return ret;
    }
};

struct Table
{
    std::vector<std::vector<Cell>>  cells;
};

struct Tables
{
    std::vector<Table>  tables;
};


template<typename T>
double mat_stats(const cv::Mat& mat)
{
    double total = 0;
    for (size_t i=0; i<mat.total(); ++i)
    {
        total += mat.at<T>(i);
    }
    return total / mat.total();
}

static std::string str(const cv::Rect& rect)
{
    std::ostringstream s;
    s << "((" << rect.x << " " << rect.y << ") (" << rect.x + rect.width << " " << rect.y + rect.height << "))";
    return s.str();
}

void extract_table_find(const std::string& path_pdf)
{
    // _generate_image().
    //
    std::string path_png = path_pdf + ".1.png";
    std::string path_png_spec = path_pdf + "..png";
    std::string command = "../../build/debug-extract/mutool convert -O resolution=300 -o " + path_png_spec + " " + path_pdf;
    std::cerr << __FILE__ << ":" << __LINE__ << ":"
            << "running: " << command << "\n";
    int e = system(command.c_str());
    assert(!e);
    
    
    // _generate_table_bbox().
    //
            // adaptive_threshold
            cv::Mat image = cv::imread(path_png);
            cv::Mat image_grey;
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << "image.elemSize()=" << image.elemSize() << "\n";
            cv::cvtColor(image, image_grey, cv::COLOR_BGR2GRAY);
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << "image_grey.elemSize()=" << image_grey.elemSize() << "\n";

            int image_width = image.cols;
            int image_height = image.rows;
            int pdf_width = 612;
            int pdf_height = 792;
            double image_width_scaler = 1.0 * image_width / pdf_width;
            double image_height_scaler = 1.0 * image_height / pdf_height;
            double pdf_width_scaler = 1.0 / image_width_scaler;
            double pdf_height_scaler = 1.0 / image_height_scaler;
            std::cerr << __FILE__ << ":" << __LINE__ << ":"
                    << " image_width_scaler=" << image_width_scaler
                    << " image_height_scaler=" << image_height_scaler
                    << " pdf_width_scaler=" << pdf_width_scaler
                    << " pdf_height_scaler=" << pdf_height_scaler
                    << "\n";

            std::cerr << __FILE__ << ":" << __LINE__ << ":" << " mat_stats<uint8_t>(image_grey)=" << mat_stats<uint8_t>(image_grey) << "\n";

            // Invert each pixel.
            for (int i=0; i<image_grey.rows; ++i)
            {
                for (int j=0; j<image_grey.cols; ++j)
                {
                    auto& p = image_grey.at<unsigned char>(i, j);
                    p = 255 - p;
                }
            }
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << " mat_stats<uint8_t>(image_grey)=" << mat_stats<uint8_t>(image_grey) << "\n";
            cv::imwrite("et-gry-invert.png", image_grey);

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
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << " mat_stats<uint8_t>(image_grey_threshold)=" << mat_stats<uint8_t>(image_grey_threshold) << "\n";
            cv::imwrite("et-threshold.png", image_grey_threshold);
            
            // Find lines
            //
            std::vector<cv::Rect>   vertical_segments;
            std::vector<cv::Rect>   horizontal_segments;

            auto vertical_mask = image_grey.clone();
            auto horizontal_mask = image_grey.clone();

            int line_scale = 15;

            //const cv::Mat image_grey_threshold_backup = image_grey_threshold;

            //for (int vertical=1; vertical >= 0; --vertical)
            for (int vertical=0; vertical < 2; ++vertical)
            {
                //image_grey_threshold = image_grey_threshold_backup;
                std::cerr << __FILE__ << ":" << __LINE__ << ": ------------------------------\n";
                std::cerr << __FILE__ << ":" << __LINE__ << ":" << " vertical=" << vertical << "\n";
                std::cerr << __FILE__ << ":" << __LINE__ << ":" << " line_scale=" << line_scale << "\n";
                std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " mat_stats<uint8_t>(image_grey_threshold)=" << mat_stats<uint8_t>(image_grey_threshold)
                        << " image_grey_threshold.size()=" << image_grey_threshold.size()
                        << "\n";
                int size = (vertical) ? image_grey_threshold.rows : image_grey_threshold.cols;
                size /= line_scale;
                std::cerr << __FILE__ << ":" << __LINE__ << ":" << " size=" << size << "\n";

                cv::Mat element = cv::getStructuringElement(
                        cv::MORPH_RECT,
                        (vertical) ? cv::Size(1, size) : cv::Size(size, 1)
                        );
                if (0) std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " element=" << element
                        << "\n";
                auto threshold = image_grey_threshold.clone();
                cv::erode(image_grey_threshold, threshold, element);
                {
                    std::ostringstream  path;
                    path << "et-erode-" << vertical << ".png";
                    imwrite(path.str(), threshold);
                }
                std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " after erode():"
                        << " &threshold=" << &threshold
                        << " mat_stats<uint8_t>(threshold)=" << mat_stats<uint8_t>(threshold)
                        << "\n";

                std::cerr << __FILE__ << ":" << __LINE__ << ":" << " mat_stats<uint8_t>(threshold)=" << mat_stats<uint8_t>(threshold) << "\n";
                int num_zero = 0;
                int num_one = 0;
                for (size_t i=0; i<threshold.total(); ++i)
                {
                    if (threshold.at<uint8_t>(i) == 0)   num_zero += 1;
                    if (threshold.at<uint8_t>(i) == 1)   num_one += 1;
                }

                std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " threshold.total()=" << threshold.total()
                        << " threshold.size()=" << threshold.size()
                        << " threshold.size=" << threshold.size
                        << " threshold num_zero=" << num_zero
                        << " threshold num_one=" << num_one
                        << "\n";
                auto dilate = threshold.clone();
                std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " dilate.size=" << dilate.size
                        << " dilate.elemSize()=" << dilate.elemSize()
                        << "\n";
                cv::dilate(threshold, dilate, element);
                std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " after dilate():"
                        << " threshold.size=" << threshold.size
                        << "\n";
                {
                    std::ostringstream  path;
                    path << "et-dilate-" << vertical << ".png";
                    imwrite(path.str(), dilate);
                }
                //cv::dilate(threshold, dmask, element, cv::Point(-1, -1) /*anchor*/, 0 /*iterations*/);

                if (vertical)   vertical_mask = dilate.clone();
                else            horizontal_mask = dilate.clone();

                //cv::OutputArrayOfArrays contours;
                std::cerr << __FILE__ << ":" << __LINE__ << ": threshold.size()=" << threshold.size() << "\n";
                std::vector<std::vector<cv::Point>> contours;
                //auto contours = threshold;
                std::cerr << __FILE__ << ":" << __LINE__ << ":"
                        << " before calling cv::findContours:"
                        << " dilate.size=" << dilate.size
                        << " dilate.elemSize()=" << dilate.elemSize()
                        << "\n";
                
                cv::findContours(dilate, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
                std::cerr << __FILE__ << ":" << __LINE__ << ": contours.size()=" << contours.size() << "\n";

                if (1)
                {
                    cv::Mat contours_image = cv::Mat::zeros(threshold.size(), CV_8UC3);
                    cv::Scalar colour(0, 0, 255);
                    cv::drawContours( contours_image, contours, -1, colour);
                    std::ostringstream path;
                    path << "et-contours-" << vertical << ".png";
                    cv::imwrite(path.str(), contours_image);
                }
                for (auto& c: contours)
                {
                    cv::Rect    bounding_rect = cv::boundingRect(c);
                    int x1 = bounding_rect.x;
                    int x2 = bounding_rect.x + bounding_rect.width;
                    int y1 = bounding_rect.y;
                    int y2 = bounding_rect.y + bounding_rect.height;
                    std::cerr << __FILE__ << ":" << __LINE__ << ":" << "x1 y1 x2 y2: (" << x1 << ' ' << y1 << ' ' << x2 << ' ' << y2 << ")\n";
                    if (vertical)
                    {
                        cv::Rect rect((x1 + x2) / 2, y2, 0 /*width*/, y1 - y2 /*height*/);
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << "    " << str(rect) << "\n";
                        vertical_segments.push_back(rect);
                    }
                    else
                    {
                        cv::Rect rect(x1, (y1 + y2) / 2, x2 - x1 /*width*/, 0 /*height*/);
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << "    " << str(rect) << "\n";
                        horizontal_segments.push_back(rect);
                    }
                }
            }
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## -A ## vertical_segments.size()=" << vertical_segments.size() << "\n";
            for (auto& i: vertical_segments)
            {
                std::cerr << "    " << str(i) << "\n";
            }
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## -A ## horizontal_segments.size()=" << horizontal_segments.size() << "\n";
            for (auto& i: horizontal_segments)
            {
                std::cerr << "    " << str(i) << "\n";
            }
        // End of Find lines.
    // End of _generate_table_bbox.
    
    // contours = find_contours(vertical_mask, horizontal_mask)
    //auto mask = vertical_mask + horizontal_mask;
    auto mask = image_grey.clone();
    //cv::Mat mat = cv::Mat::zeros(threshold.size(), CV_8UC3);
    for (size_t i = 0; i != vertical_mask.total(); ++i)
    {
        int v = vertical_mask.at<uint8_t>(i);
        int h = horizontal_mask.at<uint8_t>(i);
        int t = v + h;
        if (t > 255) t = 255;
        mask.at<uint8_t>(i) = t;
    }
    std::cerr << __FILE__ << ":" << __LINE__ << ":"
            << " vertical_mask.size()=" << vertical_mask.size()
            << " vertical_mask.elemSize()=" << vertical_mask.elemSize()
            << " horizontal_mask.size()=" << horizontal_mask.size()
            << " horizontal_mask.elemSize()=" << horizontal_mask.elemSize()
            << " mask.size()=" << mask.size()
            //<< " mask.elemSize()=" << mask.elemSize()
            << "\n";
    //std::vector<std::vector<unsigned char>> contours;
    std::vector<std::vector<cv::Point>> contours0;
    std::cerr << __FILE__ << ":" << __LINE__ << ":"
            << " before calling cv::findContours:"
            << " mask.size=" << mask.size
            << " mask.elemSize()=" << mask.elemSize()
            << "\n";
    cv::findContours(mask, contours0, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## B ## contours0.size()=" << contours0.size() << "\n";
    std::sort(contours0.begin(), contours0.end(),
            [] (const std::vector<cv::Point>& a, const std::vector<cv::Point>& b)
            {
                double a_area = cv::contourArea(a);
                double b_area = cv::contourArea(b);
                return a_area > b_area;
                /*if (a.empty())  return false;
                if (b.empty())  return true;
                return a[0].y > b[0].y;*/
            }
            );
    // todo: take first 10 items.
    for (size_t i = 0; i < 10 && i < contours0.size(); ++i)
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << "    i=" << i << ": " << cv::contourArea(contours0[i]) << "\n";
    }
    
    // *** ok ***
    
    std::vector<cv::Rect>   contours;
    for (auto& c: contours0)
    {
        std::vector<cv::Point>  c_poly;
        cv::approxPolyDP(c, c_poly, 3 /*epsilon*/, true /*closed*/);
        contours.push_back(cv::boundingRect(c_poly));
    }
    // contours is as returned by find_contours().
    std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## C ## contours.size()=" << contours.size() << "\n";
    for (auto& r: contours)
    {
        std::cerr << "    " << r << "\n";
    }
    // *** contours is ok ***
    
    /* Lines are in:
        vertical_segments
        horizontal_segments
        vertical_mask
        horizontal_mask
    */
    
    // table_bbox = find_joints(contours, vertical_mask, horizontal_mask)
    //auto joints = vertical_mask * horizontal_mask;
    auto joints = image_grey.clone();
    for (size_t i = 0; i != vertical_mask.total(); ++i)
    {
        int v = vertical_mask.at<uint8_t>(i);
        int h = horizontal_mask.at<uint8_t>(i);
        int t = v * h / 255;
        if (t > 255) t = 255;
        joints.at<uint8_t>(i) = t;
    }
    
    /*std::cerr << __FILE__ << ":" << __LINE__ << ":" << " joints.size()=" << joints.size() << "\n";
    for (auto it: joints)
    {
        std::cerr << "    " << it.first << ": " << it.second << "\n";
    }*/
    
    std::map<cv::Rect, std::vector<cv::Point>, rect_compare>    table_bbox;
    for (auto rect: contours)
    {
        cv::Mat roi(
                joints,
                cv::Range(rect.y, rect.y + rect.height),
                cv::Range(rect.x, rect.x + rect.width)
                );
        std::vector<std::vector<cv::Point>> jc;
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
        //rect.y *= -1;
        //rect.height *= -1;
        cv::Rect r;
        r.x = rect.x;
        r.y = rect.y + rect.height;
        r.width = rect.width;
        r.height = -rect.height;
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " rect=" << rect << "\n";
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << "    r=" << r << "\n";
        table_bbox[r] = joint_coords;
    }
    // tables is table_bbox.
    std::cerr << __FILE__ << ":" << __LINE__ << ":" << "## D ##  table_bbox.size()=" << table_bbox.size() << "\n";
    for (auto it: table_bbox)
    {
        std::cerr << " " << it.first << "\n";
        for (auto it2: it.second)
        {
            std::cerr << " " << it2;
        }
    }
    std::cerr << "\n";
    
    /* *** ok *** */
    
    auto table_bbox_unscaled = table_bbox;
    assert(&table_bbox_unscaled != &table_bbox);
    
    // call scale_image().
    //    self.table_bbox, self.vertical_segments, self.horizontal_segments = scale_image(
    //        table_bbox, vertical_segments, horizontal_segments, pdf_scalers
    auto scaling_factor_x = pdf_width_scaler;
    auto scaling_factor_y = pdf_height_scaler;
    auto img_y = image_height;
    std::cerr << __FILE__ << ":" << __LINE__ << ":"
            << " scaling_factor_x=" << scaling_factor_x
            << " scaling_factor_y=" << scaling_factor_y
            << " img_y=" << img_y
            << "\n";
    
    std::map<cv::Rect, std::vector<cv::Point>, rect_compare>    table_bbox2;
    for (auto it: table_bbox)
    {
        auto rect = it.first;
        auto points = it.second;
        
        int x1 = rect.x;
        int y1 = rect.y;
        int x2 = rect.x + rect.width;
        int y2 = rect.y + rect.height;
        x1 *= scaling_factor_x;
        y1 = scaling_factor_y * abs(y1 - img_y);
        x2 *= scaling_factor_x;
        y2 = scaling_factor_y * abs(y2 - img_y);
        rect.x = x1;
        rect.y = y1;
        rect.width = x2 - x1;
        rect.height = y2 - y1;
        
        std::vector<cv::Point>  points2 = points;
        for (auto& point: points2)
        {
            point.x *= scaling_factor_x;
            point.y = abs(point.y - img_y) * scaling_factor_y;
        }
        table_bbox2[rect] = points2;
    }
    std::swap(table_bbox, table_bbox2);
    
    for (auto& rect: vertical_segments)
    {
        int x1 = rect.x;
        int y1 = rect.y;
        int x2 = rect.x + rect.width;
        int y2 = rect.y + rect.height;
        x1 = x1 * scaling_factor_x;
        y1 = abs(y1 - img_y) * scaling_factor_y;
        x2 = x2 * scaling_factor_x;
        y2 = abs(y2 - img_y) * scaling_factor_y;
        rect.x = x1;
        rect.y = y1;
        rect.width = x2 - x1;
        rect.height = y2 - y1;
    }
    
    for (auto& rect: horizontal_segments)
    {
        int x1 = rect.x;
        int y1 = rect.y;
        int x2 = rect.x + rect.width;
        int y2 = rect.y + rect.height;
        x1 = x1 * scaling_factor_x;
        y1 = abs(y1 - img_y) * scaling_factor_y;
        x2 = x2 * scaling_factor_x;
        y2 = abs(y2 - img_y) * scaling_factor_y;
        rect.x = x1;
        rect.y = y1;
        rect.width = x2 - x1;
        rect.height = y2 - y1;
    }
    
    std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## E ## table_bbox.size()=" << table_bbox.size() << "\n";
    for (auto& it: table_bbox)
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## E ## table_bbox it.first=" << it.first << "\n";
        for (auto it2: it.second)
        {
            std::cerr << "    " << it2 << "\n";
        }
        std::cerr << "\n";
    }
    
    // sort tables based on y-coord
    // rect_compare() sorts by y first.
    
    Tables  tables;
    
    // segments_in_bbox
    std::vector<cv::Rect>   vertical_segments_in_rect;
    std::vector<cv::Rect>   horizontal_segments_in_rect;
    std::cerr << __FILE__ << ":" << __LINE__ << ":"
            << " ## F ## table_bbox.size()=" << table_bbox.size()
            << "\n";
    for (auto it: table_bbox)
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << "    ## F ## it.first=" << it.first
                << "\n";
        if (0) for (auto point: it.second)
        {
            std::cerr << __FILE__ << ":" << __LINE__ << ":"
                    << " point=" << point << "\n";
        }
        
        const cv::Rect& rect = it.first;
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " rect=" << str(rect) << "\n";
        std::vector<cv::Point>& points = it.second;
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " points.size()=" << points.size() << "\n";
        for (auto i: points)
        {
            std::cerr << " (" << i.x << ' ' << i.y << ")";
        }
        std::cerr << "\n";
        // _generate_columns_and_rows().
        //
        
        // segments_in_bbox
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << " ## G ## vertical_segments.size()=" << vertical_segments.size() << "\n";
        for (cv::Rect& r: vertical_segments)
        {
            std::cerr << "    " << str(r) << "\n";
            int v0 = r.x;
            int v1 = r.y;
            //int v2 = r.x + r.width;
            int v3 = r.y + r.height;
            if (v1 > rect.y - 2
                    and v3 < rect.y + rect.height + 2
                    and rect.x - 2 <= v0
                    and v0 <= rect.x + rect.width + 2
                    )
            {
                std::cerr << "    appending " << str(r) << "\n"; 
                vertical_segments_in_rect.push_back(r);
            }
        }
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << " ## G ## horizontal_segments.size()=" << horizontal_segments.size() << "\n";
        for (cv::Rect& r: horizontal_segments)
        {
            std::cerr << "    " << str(r) << "\n";
            int h0 = r.x;
            int h1 = r.y;
            int h2 = r.x + r.width;
            //int h3 = r.y + r.height;
            if (h0 > rect.x - 2
                    and h2 < rect.x + rect.width + 2
                    and rect.y - 2 <= h1
                    and h1 <= rect.y + rect.height + 2
                    )
            /*if (rect.contains(cv::Point(r.x, r.y))
                    && rect.contains(cv::Point(r.x + r.width, r.y + r.height))
                    )*/
            {
                std::cerr << "    appending " << str(r) << "\n"; 
                horizontal_segments_in_rect.push_back(r);
            }
        }
        
        // Find text inside <rect>.
        
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << " vertical_segments_in_rect.size()=" << vertical_segments_in_rect.size() << "\n";
        for (auto& rect: vertical_segments_in_rect)
        {
            std::cerr << "    " << str(rect) << "\n";
        }
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << " horizontal_segments_in_rect.size()=" << horizontal_segments_in_rect.size() << "\n";
        for (auto& rect: horizontal_segments_in_rect)
        {
            std::cerr << "    " << str(rect) << "\n";
        }
        
        // 
        // cols, rows = zip(*self.table_bbox[tk])
        // cols, rows = list(cols), list(rows)
        // cols.extend([tk[0], tk[2]])
        // rows.extend([tk[1], tk[3]])
        points.push_back(cv::Point(rect.x, rect.y));
        points.push_back(cv::Point(rect.x + rect.width, rect.y + rect.height));
        
        // merge_close_lines
        
        std::vector<int>    rows00;
        std::vector<int>    cols00;
        for (cv::Point& point: points)
        {
            cols00.push_back(point.x);
            rows00.push_back(point.y);
        }
        std::sort(rows00.rbegin(), rows00.rend());
        std::sort(cols00.begin(), cols00.end());
        
        std::vector<int>    rows0;
        std::vector<int>    cols0;
        for (int y: rows00)
        {
            if (rows0.empty() || abs(y - rows0.back()) > 2)    rows0.push_back(y);
            else rows0.back() = (rows0.back() + y) / 2;
        }
        for (int x: cols00)
        {
            if (cols0.empty() || abs(x - cols0.back()) > 2)    cols0.push_back(x);
            else cols0.back() = (cols0.back() + x) / 2;
        }

        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " rows0.size()=" << rows0.size() << ":";
        for (int i: rows0) std::cerr << " " << i;
        std::cerr << "\n";
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " cols0.size()=" << cols0.size() << ":";
        for (int i: cols0) std::cerr << " " << i;
        std::cerr << "\n";
        
        
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " ## H ##\n";
        std::vector<std::pair<int, int>> rows;
        std::vector<std::pair<int, int>> cols;
        for (size_t i=0; i+1 < rows0.size(); ++i)
        {
            rows.push_back(std::pair<int, int>(rows0[i], rows0[i+1]));
        }
        for (size_t i=0; i+1 < cols0.size(); ++i)
        {
            cols.push_back(std::pair<int, int>(cols0[i], cols0[i+1]));
        }
        
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " rows.size()=" << rows.size() << "\n";
        for (auto it: rows)
        {
            std::cerr << " (" << it.first << ' ' << it.second << ")";
        }
        std::cerr << "\n";
        std::cerr << __FILE__ << ":" << __LINE__ << ":" << " cols.size()=" << cols.size() << "\n";
        for (auto it: cols)
        {
            std::cerr << " (" << it.first << ' ' << it.second << ")";
        }
        std::cerr << "\n";
        
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << " cols.size()=" << cols.size()
                << " rows.size()=" << rows.size()
                << "\n";
        
        // _generate_table
        Table table;
        
        for (auto& row: rows)
        {
            std::vector<Cell>   row_cells;
            for (auto& col: cols)
            {
                // Note we swap row.first/second.
                row_cells.push_back(Cell(col.first, row.second, col.second, row.first));
            }
            table.cells.push_back( row_cells);
        }
        
        // Table::set_edges().
        for (cv::Rect& v: vertical_segments_in_rect)
        {
            std::vector<int>    i;
            std::vector<int>    j;
            std::vector<int>    k;
            for (size_t ii=0; ii!=cols.size(); ++ii)
            {
                if (abs(v.x - cols[ii].first) <= 2) i.push_back(ii);
            }
            for (size_t jj=0; jj!=rows.size(); ++jj)
            {
                if (abs(v.y + v.height - rows[jj].first) <= 2)  j.push_back(jj);
                if (abs(v.y - rows[jj].first) <= 2)  k.push_back(jj);
            }
            if (j.empty())  continue;
            
            int jj = j[0];
            if (i.size() == 1 && i[0] == 0) // only left edge
            {
                int ll = 0;
                int kk = k.empty() ? rows.size() : k[0];
                for ( ; jj < kk; ++jj)  table.cells[jj][ll].left = true;
            }
            else if (i.empty()) // # only right edge
            {
                int ll = cols.size() - 1;
                int kk = k.empty() ? rows.size() : k[0];
                for ( ; jj < kk; ++jj)   table.cells[jj][ll].right = true;
            }
            else    // both left and right edges
            {
                int ll = i[0];
                int kk = k.empty() ? rows.size() : k[0];
                for ( ; jj < kk; ++jj)
                {
                    table.cells[jj][ll].left = true;
                    table.cells[jj][ll - 1].right = true;
                }
            }
        }
        
        for (cv::Rect& h: horizontal_segments_in_rect)
        {
            std::vector<int>    i;
            std::vector<int>    j;
            std::vector<int>    k;
            for (size_t ii=0; ii<rows.size(); ++ii)
            {
                if (abs(h.y - rows[ii].first) <= 2) i.push_back(ii);
            }
            for (size_t jj=0; jj<cols.size(); ++jj)
            {
                if (abs(h.x - cols[jj].first) <= 2) j.push_back(jj);
                if (abs(h.x + h.width - cols[jj].first) <= 2) k.push_back(jj);
            }
            if (j.empty()) continue;
            
            int jj = j[0];
            if (i.size() == 1 && i[0] == 0) // only top edge.
            {
                int ll = i[0];
                int kk = k.empty() ? cols.size() : k[0];
                for ( ; jj < kk; ++jj)  table.cells[ll][jj].top = true;
            }
            else if (i.empty()) // only bottom edge.
            {
                int ll = rows.size() - 1;
                int kk = k.empty() ? cols.size() : k[0];
                for ( ; jj < kk; ++jj)  table.cells[ll][jj].bottom = true;
            }
            else    // both top and bottom edges
            {
                int ll = i[0];
                int kk = k.empty() ? cols.size() : k[0];
                for ( ; jj < kk; ++jj)
                {
                    table.cells[ll][jj].top = true;
                    table.cells[ll - 1][jj].bottom = true;
                }
            }
        }
        
        // set_border()
        for (size_t r=0; r<rows.size(); ++r)
        {
            table.cells[r][0].left = true;
            table.cells[r][cols.size() - 1].right = true;
        }
        for (size_t c=0; c<cols.size(); ++c)
        {
            table.cells[0][c].top = true;
            table.cells[rows.size() - 1][c].bottom = true;
        }
        
        // set_span()
        for (auto& row: table.cells)
        {
            for (auto& cell: row)
            {
                bool left = cell.left;
                bool right = cell.right;
                bool top = cell.top;
                bool bottom = cell.bottom;
                if (left and right and top and bottom)  continue;
                else if (!left and right and top and bottom)    cell.hspan = true;
                else if (left and !right and top and bottom)    cell.hspan = true;
                else if (left and right and !top and bottom)    cell.vspan = true;
                else if (left and right and top and !bottom)    cell.vspan = true;
                else if (left and right and !top and !bottom)   cell.vspan = true;
                else if (!left and !right and top and bottom)   cell.hspan = true;
                else if (cell.bound() <= 1)
                {
                    cell.hspan = true;
                    cell.vspan = true;
                }
            }
        }
        
        tables.tables.push_back(table);
    }
    
    std::cout << __FILE__ << ":" << __LINE__ << ":" << " tables.tables.size()=" << tables.tables.size() << "\n";
    for (auto& table: tables.tables)
    {
        for (auto& row: table.cells)
        {
            std::cerr << __FILE__ << ":" << __LINE__ << ":";
            for (auto& cell: row)
            {
                std::cerr << " cell(" << cell.lb << ' ' << cell.rt << ')';
            }
            std::cerr << '\n';
        }
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    extract_table_find(argv[1]);
    return 0;
}
