#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "mongo/client/dbclient.h"

#define CONNECTION_FAIL -1
#define NO_LOGO -2
#define NO_IMAGE -3

#define MATCH_CUTOFF 0.9
#define HIST_CUTOFF 0.002
#define HIST_BINS 12

using namespace std;
using namespace cv;
using mongo::BSONElement;
using mongo::BSONObj;
using mongo::BSONObjBuilder;
using mongo::BSONObjIterator;
using mongo::BSONArray;
using mongo::BSONArrayBuilder;
using mongo::DBClientCursor;

const string Logos = "token.logos";

//// Feature extraction
// void get_histogram(Mat &im, Mat &hist){
//     Mat hsv;
//     cvtColor(im, hsv, COLOR_BGR2HSV);
//     int h_bins = 8; int s_bins = 12; int v_bins = 3;
//     int histSize[] = {h_bins, s_bins, v_bins};
//     float h_ranges[] = {0, 180};
//     float s_ranges[] = {0, 256};
//     float v_ranges[] = {0, 256};

//     const float* ranges[] = {h_ranges, s_ranges, v_ranges};
//     int channels[] = {0, 1, 2};
//     calcHist(&hsv, 1, channels, Mat(), hist, 3, histSize, ranges);
//     normalize(hist, hist);
// }

int get_histogram(Mat &im, float *histogram){
    Mat hls, hist;
    cvtColor(im, hls, COLOR_BGR2HLS);
    int h_bins = HIST_BINS - 2; int l_bins = 4;
    int histSize[] = {h_bins, l_bins};
    float h_ranges[] = {0, 180};
    float l_ranges[] = {0, 256};

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
    for(int i = 0; i < HIST_BINS; i++) {
        if (histogram[i] < HIST_CUTOFF) histogram[i] = 0.0;
    }
    int count = 0;
    for(int i = 1; i < HIST_BINS; i++) {
        if (histogram[i] > 0.0) count++;
    }
    return (count > 4) ? 4 : count;
}

void get_descriptors(Mat &im, Mat &desc){
    //SURF surf(400.0, 3, 3, false);
    SIFT sift(0);
    vector<KeyPoint> kp;
    //surf(im, Mat(), kp, desc);
    sift(im, Mat(), kp, desc);
}

void mat_BSON(Mat &mat, BSONObj &bson){
    BSONObjBuilder b;
    BSONArrayBuilder a;
    for (int i = 0; i < mat.dims; ++i)
        a << mat.size[i];
    b.append("size", a.arr());
    b.append("type", mat.type());
    b.appendBinData("data", 
                    mat.total() * mat.elemSize(),
                    mongo::BinDataGeneral,
                    mat.data);
    bson = b.obj();
}

void hist_BSON(float *hist, int hist_count, BSONObj &bson){
    BSONObjBuilder b;
    BSONArrayBuilder a;
    for (int i = 0; i < HIST_BINS; ++i)
        a << hist[i];
    b.append("bins", a.arr());
    b.append("count", hist_count);
    bson = b.obj();
}

void BSON_mat(Mat &mat, BSONObj bson){
    vector<int> sizes;
    BSONObjIterator arr = bson.getObjectField("size");
    while(arr.more()) {
        sizes.push_back(arr.next().numberInt());
    }
    int type = bson.getIntField("type");
    mat = Mat(sizes.size(), &sizes[0], type);
    int len = 0;
    const char *data = bson.getField("data").binData(len);
    memcpy(mat.data, data, len);
}

int BSON_hist(float *hist, BSONObj bson){
    vector<int> sizes;
    BSONObjIterator arr = bson.getObjectField("bins");
    for (int i = 0; i < HIST_BINS; ++i)
        hist[i] = arr.next().Number();
    return bson.getIntField("count");
}

int extract_features(string logo_id, string image_store, string db_server){
    // Find logo
    mongo::DBClientConnection db;
    try {
        db.connect(db_server);
    } catch( const mongo::DBException &e ) {
        return CONNECTION_FAIL;
    }
    auto_ptr<DBClientCursor> cursor =
        db.query(Logos, MONGO_QUERY("_id" << logo_id));
    if (!cursor->more()) return NO_LOGO;
    BSONObj p = cursor->next();
    string file = image_store + p.getStringField("file");
    // Get image features
    Mat im, desc;
    im = imread(file , CV_LOAD_IMAGE_COLOR);
    if (im.empty()) return NO_IMAGE;
    float hist[HIST_BINS];
    int hist_count = get_histogram(im, &hist[0]);
    get_descriptors(im, desc);
    float aspect = 1.0 * im.size().width / im.size().height;
    // Save features
    BSONObj descriptors, histogram;
    mat_BSON(desc, descriptors);
    hist_BSON(hist, hist_count, histogram);
    BSONObjBuilder b;
    b.append("descriptors", descriptors);
    b.append("histogram", histogram);
    b.append("aspect", aspect);
    db.update(Logos,
              BSON("_id" << logo_id),
              BSON("$set" << BSON( "features" << b.obj())));
    return 0;
}

//// Feature comparison
float descriptor_distance(Mat desc1, Mat desc2){
    //FlannBasedMatcher matcher;
    BFMatcher matcher(NORM_L1);
    vector<DMatch> matches;
    matcher.match(desc1, desc2, matches);
    float r = 0.0;
    vector<DMatch>::iterator it;
    for(it = matches.begin(); it != matches.end(); ++it){
        float d = (*it).distance;
        r += (d*d);
    }
    r /= matches.size();
    return r;
}

float match_c = 1.0;
float color_count_c = 0.5;
float histogram_distance(float *hist1, float *hist2, 
                         int h_count1, int h_count2){
    int max1 = 0, max2 = 0, submax1 = -1, submax2 = -1;
    float m1 = -1, m2 = -1, sm1 = -1, sm2 = -1;
    for(int i = 1; i < HIST_BINS; i++){
        float v1 = hist1[i];
        if (v1 >= HIST_CUTOFF){
            if (v1 > m1) {
                sm1 = m1; submax1 = max1;
                m1 = v1; max1 = i;
            } else if (v1 > sm1){
                sm1 = v1; submax1 = i;
            }
        }
        float v2 = hist2[i];
        if (v2 >= HIST_CUTOFF){
            if (v2 > m2) {
                sm2 = m2; submax2 = max2;
                m2 = v2; max2 = i;
            } else if (v2 > sm2){
                sm2 = v2; submax2 = i;
            }
        }
    }
    float color_match = 1.5;
    if (max1 == max2) color_match -= 1;
    if (max1 == submax2) color_match -= 0.5;
    if (max2 == submax1) color_match -= 0.5;
    if (submax2 == submax1) color_match -= 0.5;
    int count_diff = abs(h_count1 - h_count2);
    return color_match * match_c + count_diff * color_count_c;
}

const float descriptor_c = 1;
const float histogram_c = 1;
const float aspect_c = 1;

float feature_distance(float *hist1, float *hist2, 
                       int h_count1, int h_count2,
                       Mat desc1, Mat desc2, 
                       float aspect1, float aspect2){
    float desc_dist = descriptor_distance(desc1, desc2);
    //float hist_dist = histogram_distance(hist1, hist2, h_count1, h_count2);
    //float a = aspect1-aspect2;
    //float aspect_dist = (a*a)/(aspect1 + aspect2);
    return //descriptor_c * desc_dist
        histogram_c * hist_dist
        //+ aspect_c * aspect_dist
        ;
}

#include <chrono>
using namespace std::chrono;

int search_features(string image, string image_store, string db_server, BSONArray &bson){
    mongo::DBClientConnection db;
    try {
        db.connect(db_server);
    } catch( const mongo::DBException &e ) {
        return CONNECTION_FAIL;
    }
       
    Mat im, desc;
    im = imread(image , CV_LOAD_IMAGE_COLOR);
    if (im.empty()) return NO_IMAGE;
    float hist[HIST_BINS];
    int hist_count = get_histogram(im, &hist[0]);
    get_descriptors(im, desc);
    float aspect = 1.0 * im.size().width / im.size().height;

    monotonic_clock::time_point t1 = monotonic_clock::now();
    int i = 0;
    BSONArrayBuilder a;
    auto_ptr<DBClientCursor> cursor = db.query(Logos, BSONObj());
    while(cursor->more()) {
        i++;
        BSONObj logo = cursor->next();
        Mat desc2;
        float hist2[HIST_BINS];
        BSONObj features = logo.getObjectField("features");
        if (features.isEmpty()) continue;
        int hist_count2 = BSON_hist(hist2, 
                                    features.getObjectField("histogram"));
        BSON_mat(desc2, features.getObjectField("descriptors"));
        float aspect2 = features.getField("aspect").Number();
        float distance = feature_distance(hist, hist2,
                                          hist_count, hist_count2,
                                          desc, desc2, 
                                          aspect, aspect2);
        //if (distance <= MATCH_CUTOFF)
            a << BSON("logo" << logo.removeField("features") << "distance" << distance);
    }
    monotonic_clock::time_point t2 = monotonic_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    std::cout << "Searched through " << i << " logos in " << time_span.count() << " seconds." << endl;
    bson = a.arr();
    return 0;
}


//// Node module
#include <nan.h>

NAN_METHOD(Extract){
    NanScope();
    string server = "localhost";
    if (args.Length() < 2) {
        return NanThrowError("Expected logo ID, image store location");
    }
    if (args.Length() >= 3) {
        server = *NanAsciiString(args[2]);
    }
    string logo_id = *NanAsciiString(args[0]);
    string image_store = *NanAsciiString(args[1]);
    int result = extract_features(logo_id, image_store, server);
    if (result < 0){
        switch (result){
        case NO_IMAGE: 
            NanThrowError("Feature extraction failed: No such image");
            break;
        case NO_LOGO: 
            NanThrowError("Feature extraction failed: No such logo");
            break;
        case CONNECTION_FAIL: 
            NanThrowError("Feature extraction failed: Connection to DB failed");
            break;
        }
    }
    NanReturnUndefined();
}

NAN_METHOD(Search){
    NanScope();
    string server = "localhost";
    if (args.Length() < 2) {
        return NanThrowError("Expected logo ID, image store location");
    }
    if (args.Length() >= 3) {
        server = *NanAsciiString(args[2]);
    }
    string image = *NanAsciiString(args[0]);
    string image_store = *NanAsciiString(args[1]);
    BSONArray bson;
    int result = search_features(image, image_store, server, bson);
    if (result < 0){
        switch (result){
        case NO_IMAGE: 
            NanThrowError("Logo search failed: No such image");
            break;
        case CONNECTION_FAIL: 
            NanThrowError("Logo extraction failed: Connection to DB failed");
            break;
        }
    }
    NanReturnValue(NanNew<v8::String>(bson.jsonString()));
}

void init(v8::Handle<v8::Object> exports){
    mongo::client::initialize();
    exports->Set(NanNew<v8::String>("extract"), 
                 NanNew<v8::FunctionTemplate>(Extract)->GetFunction());
    exports->Set(NanNew<v8::String>("search"), 
                 NanNew<v8::FunctionTemplate>(Search)->GetFunction());
}

NODE_MODULE(logo_features, init)
