#include <iostream>

#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/xfeatures2d/nonfree.hpp"

#include "beta_shape.cc"

#define NEXT 0
#define PREVIOUS 1
#define CHILD 2
#define PARENT 3

#define SIMILAR_CHILDREN 0.1
#define MIN_CHILD_AREA 10.0

#define MATCH_CUTOFF 0.05
#define HIST_CUTOFF 0.02
#define HIST_BINS 12

#define MATCH_C 1.0
#define COLOR_COUNT_C 0.5
#define HISTOGRAM_C 1.0
#define ASPECT_C 1.0

using namespace std;
using namespace cv;
//using namespace cv::xfeatures2d;

struct Features{
    unsigned char n_colors;
    unsigned short colors, points;
    float white, sharpness, aspect;
};

unsigned char bit_count16(unsigned short n){
    unsigned short x = n;
    x = x - ((x >> 1) & 0x5555);
    x = (x & 0x3333) + ((x >> 2) & 0x3333);
    x = (x + (x >> 4)) & 0x0F0F;
    x = x + (x >> 8);
    return x & 0x1F;
}

/// Extraction
void get_histogram(Mat &im, Features &f){
    Mat hls, hist;
    cvtColor(im, hls, COLOR_BGR2HLS);
    int h_bins = HIST_BINS - 2; int l_bins = 4;
    int histSize[] = {h_bins, l_bins};
    float h_ranges[] = {0, 180};
    float l_ranges[] = {0, 256};

    float histogram[HIST_BINS];
    const float* ranges[] = {h_ranges, l_ranges};
    int channels[] = {0, 1};
    calcHist(&hls, 1, channels, Mat(), hist, 2, histSize, ranges);
    normalize(hist, hist);
    for(int i = 0; i < HIST_BINS; i++)
        histogram[i] = 0.0;
    for(int i = 0; i < h_bins; i++){
        histogram[0] += hist.at<float>(i, 3);
        histogram[1] += hist.at<float>(i, 0);
        histogram[i+2] += hist.at<float>(i, 1) + hist.at<float>(i, 2);
    }
    f.colors = 0;
    f.white = histogram[0];
    for(int i = 1; i < HIST_BINS; i++) {
        if (histogram[i] > HIST_CUTOFF)
            f.colors |= 1 << (i-1);
    }
    f.n_colors = bit_count16(f.colors);
}

// void get_descriptors(Mat &im, Mat &desc){
//     Ptr<SURF> surf = SURF::create(400.0, 3, 3, false);
//     std::vector<KeyPoint> kp;
//     surf->detectAndCompute(im, Mat(), kp, desc);
// }

void get_dissimilar_children(size_t i, const Contour &c, const Contours &cs,
                             const Hierarchy &h, Contours &children){
    int child = h[i][CHILD];
    while (child >= 0){
        if (matchShapes(c, cs[child], CV_CONTOURS_MATCH_I1, 0.0)
            < SIMILAR_CHILDREN){
            get_dissimilar_children(child, c, cs, h, children);
        } else {
            if (contourArea(cs[child]) > MIN_CHILD_AREA)
                children.push_back(cs[child]);
        }
        child = h[child][NEXT];
    }
}

void get_shapes(const Mat &im, Features &f, Contour &shape, 
                Contour &sub_shape){
    Mat gray, edges;
    shape.clear(); sub_shape.clear();
    cvtColor(im, gray, COLOR_BGR2GRAY);
    // bilateralFilter(gray.clone(), gray, 11, 17, 17);
    Canny(gray, edges, 30, 150);
    Contours contours, outer, inner;
    Hierarchy hierarchy;
    findContours(edges, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE);

    for(size_t i = 0; i < hierarchy.size(); i++){
        if (hierarchy[i][3] == -1){
            outer.push_back(contours[i]);
            get_dissimilar_children(i, contours[i], contours, 
                                    hierarchy, inner);
        }
    }
    get_beta_shape(outer, shape);
    /// TODO Sort inner points by area, pick ones that make up majority for sub_shape?
    get_beta_shape(inner, sub_shape);

    int total_points = 0;
    double total_perimeter = 0;
    for(size_t i = 0; i < outer.size(); i++){
        Contour approx;
        approxPolyDP(outer[i], approx, 3, true);
        outer[i] = approx;
        total_points += approx.size();
        total_perimeter += arcLength(approx, true);
    }
    for(size_t i = 0; i < inner.size(); i++){
        Contour approx;
        approxPolyDP(inner[i], approx, 3, true);
        inner[i] = approx;
        total_points += approx.size();
        total_perimeter += arcLength(approx, true);
    }
    f.points = total_points;
    f.sharpness = total_points / total_perimeter;
}

int get_features(const string file, Features &f, Contour &shape,
                 Contour &sub_shape){
    Mat im;
    im = imread(file, CV_LOAD_IMAGE_COLOR);
    if (im.empty()) return 0;
    get_shapes(im, f, shape, sub_shape);
    Rect bb = boundingRect(shape);
    f.aspect = 1.0 * bb.width / bb.height;
    Mat cropped = im(bb);
    get_histogram(cropped, f);
    return 1;
}


/// Distance

// float descriptor_distance(Mat desc1, Mat desc2){
//     //FlannBasedMatcher matcher;
//     BFMatcher matcher(NORM_L1);
//     vector<DMatch> matches;
//     matcher.match(desc1, desc2, matches);
//     float r = 0.0;
//     vector<DMatch>::iterator it;
//     for(it = matches.begin(); it != matches.end(); ++it){
//         float d = (*it).distance;
//         r += (d*d);
//     }
//     r /= matches.size();
//     return r;
// }

float histogram_distance(const Features &f1, const Features &f2){
    // TODO white
    int color_match = f1.colors ^ f2.colors;
    int count_diff = abs(f1.n_colors - f2.n_colors);
    color_match = bit_count16(color_match) - count_diff;
    return color_match * MATCH_C + count_diff * COLOR_COUNT_C;
}

float feature_distance(const Features &f1, const Features &f2,
                       const Contour &shape1, const Contour &shape2,
                       const Contour &sub_shape1, const Contour &sub_shape2){
    float hist_dist = histogram_distance(f1, f2);
    //float a = aspect1-aspect2;
    //float aspect_dist = (a*a)/(aspect1 + aspect2);
    return HISTOGRAM_C * hist_dist
        //+ ASPECT_C * aspect_dist
        ;
}


/* Testing */
#include <bitset>
void draw_features(const string file, const Features &f, 
                   const Contour &shape, const Contour &sub_shape){
    Mat im;
    im = imread(file, CV_LOAD_IMAGE_COLOR);

    Contours cs, scs;
    cs.push_back(shape);
    scs.push_back(sub_shape);
    drawContours(im, scs, -1, Scalar(0,225,0),
                 2, LINE_AA);
    drawContours(im, cs, -1, Scalar(225,0,255),
                 2, LINE_AA);
    bitset<16> colors(f.colors);
    int n_colors = f.n_colors;
    cout << n_colors << " colors (" << f.white << " white): " << colors << endl;
    cout << "#points: " << f.points << " sharpness: " << f.sharpness << " aspect: " << f.aspect << endl;
    cout << sizeof(Features) + (sizeof(Point) * (shape.size() + sub_shape.size())) << " bytes   (shape points: " << shape.size() << ", subshape points:" << sub_shape.size() <<")" << endl;

    destroyAllWindows();
    namedWindow(file, 1);
    imshow(file, im);
    waitKey();
}
