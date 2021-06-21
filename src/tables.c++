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
    cv::Mat image = cv::imread(path_png);
    cv::Mat image_grey;
    //std::cerr << __FILE__ << ":" << __LINE__ << ":" << "image: " << image << "\n";
    cv::cvtColor(image, image_grey, cv::COLOR_BGR2GRAY);
    
    int image_width = image.cols;
    int image_height = image.rows;
    int pdf_width = 612;
    int pdf_height = 792;
    double image_width_scaler = 1.0 * image_width / pdf_width;
    double image_height_scaler = 1.0 * image_height / pdf_height;
    double pdf_width_scaler = 1.0 / image_width_scaler;
    double pdf_height_scaler = 1.0 / image_height_scaler;
    
    
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
        std::cerr << __FILE__ << ":" << __LINE__ << ":"
                << " element=" << element
                << "\n";
        auto image_grey_threshold2 = image_grey_threshold;    // fixme: avoid copy.
        cv::erode(image_grey_threshold, image_grey_threshold2, element);
        auto threshold = image_grey_threshold2;
        cv::dilate(image_grey_threshold2, threshold, element);
        auto dmask = threshold;
        cv::dilate(threshold, dmask, element, cv::Point(-1, -1) /*anchor*/, 0 /*iterations*/);
        
        if (vertical)   vertical_mask = dmask;
        else            horizontal_mask = dmask;
        
        //cv::OutputArrayOfArrays contours;
        std::cerr << __FILE__ << ":" << __LINE__ << ": threshold.size()=" << threshold.size() << "\n";
        std::vector<std::vector<cv::Point>> contours;
        //auto contours = threshold;
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
                vertical_segments.push_back(cv::Rect((x1 + x2) / 2, y1, 0 /*width*/, y2 - y1 /*height*/));
            }
            else
            {
                horizontal_segments.push_back(cv::Rect(x1, (y1 + y2) / 2, x2 - x1 /*width*/, 0 /*height*/));
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
    
    // call scale_image().
    //    self.table_bbox, self.vertical_segments, self.horizontal_segments = scale_image(
    //        table_bbox, vertical_segments, horizontal_segments, pdf_scalers
    auto scaling_factor_x = pdf_width_scaler;
    auto scaling_factor_y = pdf_height_scaler;
    auto img_y = image_height;
    
    std::map<cv::Rect, std::vector<cv::Point>, rect_compare>    table_bbox2;
    for (auto it: table_bbox)
    {
        auto rect = it.first;
        auto points = it.second;
        rect.x      *= scaling_factor_x;
        rect.y      = (rect.y - img_y) * scaling_factor_y;
        rect.width  *= scaling_factor_x;
        rect.height *= scaling_factor_y;
        /*std::vector<int>    jx;
        std::vector<int>    jy;
        for (auto point: points)
        {
            jx.push_back(scaling_factor_x * point.x);
            jy.push_back(scaling_factor_y * point.y);
        }*/
        std::vector<cv::Point>  points2 = points;
        for (auto point: points2)
        {
            point.x *= scaling_factor_x;
            point.y = (point.y - img_y) * scaling_factor_y;
        }
        table_bbox2[rect] = points2;
    }
    std::swap(table_bbox, table_bbox2);
    
    for (auto rect: vertical_segments)
    {
        rect.x *= scaling_factor_x;
        rect.y = (rect.y - img_y) * scaling_factor_y;
        rect.width *= scaling_factor_x;
        rect.height *= scaling_factor_y;
    }
    
    for (auto rect: horizontal_segments)
    {
        rect.x *= scaling_factor_x;
        rect.y = (rect.y - img_y) * scaling_factor_y;
        rect.width *= scaling_factor_x;
        rect.height *= scaling_factor_y;
    }
    
    // sort tables based on y-coord
    // rect_compare() sorts by y first.
    
    // segments_in_bbox
    std::vector<cv::Rect>   vertical_segments_in_rect;
    std::vector<cv::Rect>   horizontal_segments_in_rect;
    for (auto it: table_bbox)
    {
        const cv::Rect& rect = it.first;
        std::vector<cv::Point>& points = it.second;
        
        // _generate_columns_and_rows().
        //
        
        // segments_in_bbox
        for (cv::Rect& r: vertical_segments)
        {
            if (rect.contains(cv::Point(r.x, r.y))
                    && rect.contains(cv::Point(r.x + r.width, r.y + r.height))
                    )
            {
                vertical_segments_in_rect.push_back(r);
            }
        }
        for (cv::Rect& r: horizontal_segments)
        {
            if (rect.contains(cv::Point(r.x, r.y))
                    && rect.contains(cv::Point(r.x + r.width, r.y + r.height))
                    )
            {
                horizontal_segments_in_rect.push_back(r);
            }
        }
        
        // Find text inside <rect>.
        
        // 
        // cols, rows = zip(*self.table_bbox[tk])
        // cols, rows = list(cols), list(rows)
        // cols.extend([tk[0], tk[2]])
        // rows.extend([tk[1], tk[3]])
        points.push_back(cv::Point(rect.x, rect.y));
        points.push_back(cv::Point(rect.x + rect.width, rect.y + rect.height));
        
        // merge_close_lines
        std::vector<int>    rows0;
        std::vector<int>    cols0;
        for (cv::Point& point: points)
        {
            if (cols0.empty() || abs(point.x - cols0.back()) <= 2)    cols0.push_back(point.x);
            if (rows0.empty() || abs(point.y - rows0.back()) <= 2)    rows0.push_back(point.y);
        }
        
        std::vector<std::pair<int, int>> rows;
        std::vector<std::pair<int, int>> cols;
        for (size_t i=0; i+1 < rows0.size(); ++i)
        {
            rows.push_back(std::pair<int, int>(rows0[i], rows0[i+1]));
        }
        for (size_t i=0; i+1 < cols0.size(); ++i)
        {
            cols.push_back(std::pair<int, int>(cols0[i], rows0[i+1]));
        }
        
        
        // _generate_table
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
        std::vector<std::vector<Cell>>  cells;
        for (auto& row: rows)
        {
            std::vector<Cell>   row_cells;
            for (auto& col: cols)
            {
                row_cells.push_back(Cell(col.first, row.first, col.second, row.second));
            }
            cells.push_back( row_cells);
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
                for ( ; jj < kk; ++jj)  cells[jj][ll].left = true;
            }
            else if (i.empty()) // # only right edge
            {
                int ll = cols.size() - 1;
                int kk = k.empty() ? rows.size() : k[0];
                for ( ; jj < kk; ++jj)   cells[jj][ll].right = true;
            }
            else    // both left and right edges
            {
                int ll = i[0];
                int kk = k.empty() ? rows.size() : k[0];
                for ( ; jj < kk; ++jj)
                {
                    cells[jj][ll].left = true;
                    cells[jj][ll - 1].right = true;
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
                for ( ; jj < kk; ++jj)  cells[ll][jj].top = true;
            }
            else if (i.empty()) // only bottom edge.
            {
                int ll = rows.size() - 1;
                int kk = k.empty() ? cols.size() : k[0];
                for ( ; jj < kk; ++jj)  cells[ll][jj].bottom = true;
            }
            else    // both top and bottom edges
            {
                int ll = i[0];
                int kk = k.empty() ? cols.size() : k[0];
                for ( ; jj < kk; ++jj)
                {
                    cells[ll][jj].top = true;
                    cells[ll - 1][jj].bottom = true;
                }
            }
        }
        
        // set_border()
        for (size_t r=0; r<rows.size(); ++r)
        {
            cells[r][0].left = true;
            cells[r][cols.size() - 1].right = true;
        }
        for (size_t c=0; c<cols.size(); ++c)
        {
            cells[0][c].top = true;
            cells[rows.size() - 1][c].bottom = true;
        }
        
        // set_span()
        for (auto& row: cells)
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
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    extract_table_find(argv[1]);
    return 0;
}
