#include "feature_extraction.cc"

int main() {
    Features f;
    Contour s, ss;
    const char* logos[] = {"rects", "macdos", "starbucks1", "starbucks2", "face-crown", "throw-crest", "swan-crest"};
    for (int i = 0; i < 7; i++){
        string path = "tests/" + string(logos[i]) + ".png";
        get_features(path, f, s, ss);
        draw_features(path, f, s, ss);
    }
    return EXIT_SUCCESS;
}

