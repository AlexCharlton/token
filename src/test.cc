#include "feature_extraction.cc"

void run_tests(){
    const char* logos[] = {"rects", "macdos", "starbucks1", "starbucks2", "face-crown", "throw-crest", "swan-crest"};
    for (int i = 0; i < 6; i++){
        string a = "tests/" + string(logos[i]) + ".png";
        string b = "tests/" + string(logos[i+1]) + ".png";
        compare_logos(a, b);
        cout << endl;
    }

}

int main(int argc, char* argv[]) {
    if (argc == 1) run_tests();
    else if (argc == 2) get_draw_features(string(argv[1]));
    else compare_logos(string(argv[1]), string(argv[2]));
    return EXIT_SUCCESS;
}
