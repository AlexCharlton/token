#include <iostream>

#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "mongo/client/dbclient.h"

#define CONNECTION_FAIL -1
#define NO_LOGO -2

using namespace std;
using namespace cv;
using mongo::BSONElement;
using mongo::BSONObj;
using mongo::BSONObjBuilder;
using mongo::DBClientCursor;

const string Logos = "token.logos";

void get_histogram(Mat &im, Mat &hist){
    Mat hsv;
    cvtColor(im, hsv, COLOR_BGR2HSV);
    int h_bins = 8; int s_bins = 12; int v_bins = 3;
    int histSize[] = {h_bins, s_bins, v_bins};
    float h_ranges[] = {0, 180};
    float s_ranges[] = {0, 256};
    float v_ranges[] = {0, 256};

    const float* ranges[] = {h_ranges, s_ranges, v_ranges};
    int channels[] = {0, 1, 2};
    calcHist(&hsv, 1, channels, Mat(), hist, 3, histSize, ranges);
    normalize(hist, hist);
}

void get_descriptors(Mat &im, Mat &desc){
    SURF surf(400.0, 4, 2, false);
    vector<KeyPoint> kp;
    surf(im, Mat(), kp, desc);
}

int extract_features(string logo_id, string image_store,string db_server){
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
    Mat im, hist, desc;
    im = cv::imread(file , CV_LOAD_IMAGE_COLOR);
    get_histogram(im, hist);
    get_descriptors(im, desc);
    Size size = im.size();
    float aspect = 1.0 * size.width / size.height;
    // Save features
    BSONObjBuilder b;
    b.appendBinData("descriptors", 
                    desc.total() * desc.elemSize(),
                    mongo::BinDataGeneral,
                    desc.data);
    b.appendBinData("histogram",
                    hist.total() * hist.elemSize(),
                    mongo::BinDataGeneral,
                    hist.data);
    b.append("aspect", aspect);
    db.update(Logos,
              BSON("_id" << logo_id),
              BSON("$set" << BSON( "features" << b.obj())));
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

void init(v8::Handle<v8::Object> exports){
    mongo::client::initialize();
    exports->Set(NanNew<v8::String>("extract"), 
                 NanNew<v8::FunctionTemplate>(Extract)->GetFunction());
}

NODE_MODULE(logo_features, init)
