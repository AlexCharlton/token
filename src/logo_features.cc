#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <chrono>

#include "mongo/client/dbclient.h"

#include "feature_extraction.cc"

#define CONNECTION_FAIL -1
#define NO_LOGO -2
#define NO_IMAGE -3

using namespace std;
using namespace std::chrono;
using mongo::BSONElement;
using mongo::BSONObj;
using mongo::BSONObjBuilder;
using mongo::BSONObjIterator;
using mongo::BSONArray;
using mongo::BSONArrayBuilder;
using mongo::DBClientCursor;

// void mat_BSON(Mat &mat, BSONObj &bson){
//     BSONObjBuilder b;
//     BSONArrayBuilder a;
//     for (int i = 0; i < mat.dims; ++i)
//         a << mat.size[i];
//     b.append("size", a.arr());
//     b.append("type", mat.type());
//     b.appendBinData("data", 
//                     mat.total() * mat.elemSize(),
//                     mongo::BinDataGeneral,
//                     mat.data);
//     bson = b.obj();
// }

void features_BSON(Features *features, BSONObjBuilder &b){
    b.appendBinData("struct", 
                    sizeof(Features),
                    mongo::BinDataGeneral,
                    features);
}

// void BSON_mat(Mat &mat, BSONObj bson){
//     std::vector<int> sizes;
//     BSONObjIterator arr = bson.getObjectField("size");
//     while(arr.more()) {
//         sizes.push_back(arr.next().numberInt());
//     }
//     int type = bson.getIntField("type");
//     mat = Mat(sizes.size(), &sizes[0], type);
//     int len = 0;
//     const char *data = bson.getField("data").binData(len);
//     memcpy(mat.data, data, len);
// }

void BSON_features(Features *features, BSONObj bson){
    int len = 0;
    const char *data = bson.getField("struct").binData(len);
    memcpy(features, data, len);
}

int extract_features(string logo_id, string db_name, string db_server){
    // Find logo
    mongo::DBClientConnection db;
    try {
        db.connect(db_server);
    } catch( const mongo::DBException &e ) {
        return CONNECTION_FAIL;
    }
    auto_ptr<DBClientCursor> cursor =
        db.query(db_name, MONGO_QUERY("_id" << logo_id));
    if (!cursor->more()) return NO_LOGO;
    BSONObj p = cursor->next();
    // Get image features
    Features features;
    if (!get_features(p.getStringField("file"), &features)) return NO_IMAGE;
    // Save features
    //mat_BSON(desc, descriptors);
    BSONObjBuilder b;
    features_BSON(&features, b);
    //b.append("descriptors", descriptors);
    db.update(db_name,
              BSON("_id" << logo_id),
              BSON("$set" << BSON( "features" << b.obj())));
    return 0;
}

//// Feature comparison

int search_features(string image, string db_name, string db_server, BSONArray &bson){
    mongo::DBClientConnection db;
    try {
        db.connect(db_server);
    } catch( const mongo::DBException &e ) {
        return CONNECTION_FAIL;
    }
    // Get image features
    Features features1;
    if (!get_features(image, &features1)) return NO_IMAGE;
    // Compare against db
    monotonic_clock::time_point t1 = monotonic_clock::now();
    BSONArrayBuilder a;
    auto_ptr<DBClientCursor> cursor = db.query(db_name, BSONObj());
    while(cursor->more()) {
        BSONObj logo = cursor->next();
        BSONObj f = logo.getObjectField("features");
        if (f.isEmpty()) continue;
        Features features2;
        BSON_features(&features2, f);
        float distance = feature_distance(&features1, &features2);
        if (distance >= 0)
            a << BSON("logo" << logo.removeField("features") << "distance" << distance);
    }
    monotonic_clock::time_point t2 = monotonic_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    cout << "Searched through logos in " << time_span.count() << " seconds." << endl;
    bson = a.arr();
    return 0;
}


//// Node module
#include <nan.h>

NAN_METHOD(Extract){
    NanScope();
    string server = "localhost";
    if (args.Length() < 2) {
        return NanThrowError("Expected logo ID, db name");
    }
    if (args.Length() >= 3) {
        server = *NanAsciiString(args[2]);
    }
    string logo_id = *NanAsciiString(args[0]);
    string db_name = *NanAsciiString(args[1]);
    int result = extract_features(logo_id, db_name, server);
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
        return NanThrowError("Expected logo ID, db_name");
    }
    if (args.Length() >= 3) {
        server = *NanAsciiString(args[2]);
    }
    string image = *NanAsciiString(args[0]);
    string db_name = *NanAsciiString(args[1]);
    BSONArray bson;
    int result = search_features(image, db_name, server, bson);
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
