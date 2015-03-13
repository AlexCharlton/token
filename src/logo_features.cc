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

void features_BSON(const Features &features, BSONObjBuilder &b){
    b.appendBinData("struct", 
                    sizeof(Features),
                    mongo::BinDataGeneral,
                    &features);
}

void contour_BSON(const string field, const Contour &c, BSONObjBuilder &b){
    b.appendBinData(field, 
                    c.size() * sizeof(Point),
                    mongo::BinDataGeneral,
                    c.data());
}

void BSON_features(Features &features, BSONObj &bson){
    int len = 0;
    const char *data = bson.getField("struct").binData(len);
    memcpy(&features, data, len);
}

void BSON_contour(const string field, Contour &c, BSONObj &bson){
    int len = 0;
    c.clear();
    const char *data = bson.getField(field).binData(len);
    c.resize(len/sizeof(Point));
    memcpy(c.data(), data, len);
}

int extract_features(const string logo_id, const string db_name, 
                     const string db_server){
    // Find logo
    mongo::DBClientConnection db;
    try {
        db.connect(db_server);
    } catch( const mongo::DBException &e ) {
        return CONNECTION_FAIL;
    }
    unique_ptr<DBClientCursor> cursor =
        db.query(db_name, MONGO_QUERY("_id" << logo_id));
    if (!cursor->more()) return NO_LOGO;
    BSONObj p = cursor->next();
    // Get image features
    Features features;
    Contour shape, sub_shape;
    if (!get_features(p.getStringField("file"), features, shape, sub_shape))
        return NO_IMAGE;
    // Save features
    BSONObjBuilder b;
    b.append("aspect", features.aspect);
    features_BSON(features, b);
    contour_BSON("shape", shape, b);
    contour_BSON("sub_shape", sub_shape, b);
    db.update(db_name,
              BSON("_id" << logo_id),
              BSON("$set" << BSON( "features" << b.obj())));
    return 0;
}

//// Feature comparison
typedef pair<string, float> Result;

int search_features(const string image, const string db_name, 
                    const string db_server, BSONArray &bson){
    mongo::DBClientConnection db;
    try {
        db.connect(db_server);
    } catch( const mongo::DBException &e ) {
        return CONNECTION_FAIL;
    }
    // Get image features
    Features features1;
    Contour shape1, sub_shape1;
    if (!get_features(image, features1, shape1, sub_shape1)) return NO_IMAGE;
    float aspect = features1.aspect;
    // Compare against db
    monotonic_clock::time_point t1 = monotonic_clock::now();
    BSONArrayBuilder a;
    // TODO remove "features." -------vvvvvvvvvvvvvvvvv
    unique_ptr<DBClientCursor> cursor =
        db.query(db_name, MONGO_QUERY("features.aspect"
                                      <<mongo::GT<< MIN_ASPECT(aspect)
                                      <<mongo::LT<< MAX_ASPECT(aspect)));
    vector<Result> v; v.reserve(512);
    while(cursor->more()) {
        BSONObj logo = cursor->next();
        BSONObj f = logo.getObjectField("features");
        if (f.isEmpty()) continue;
        Features features2;
        Contour shape2, sub_shape2;
        BSON_features(features2, f);
        BSON_contour("shape", shape2, f);
        BSON_contour("sub_shape", sub_shape2, f);
        float distance = feature_distance(features1, features2,
                                          shape1, shape2,
                                          sub_shape1, sub_shape2);
        if ((distance >= 0) && (distance < MATCH_CUTOFF))
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
