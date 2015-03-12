#include "feature_extraction.cc"

int main() {
    Features f;
    Contour s;
    get_features("public/search_store/QydsZ6MK.png", &f, s);
    waitKey();
    get_features("public/img_store/7JDxfhfF/7k-Pgf2fY.png", &f, s);
    waitKey();
    get_features("public/img_store/7ksweM2Gt/Q1Twxz3zY.png", &f, s);
    waitKey();
    get_features("tests/rects.png", &f, s);
    waitKey();
    return EXIT_SUCCESS;
}
