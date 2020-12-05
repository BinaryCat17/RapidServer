#include "file.hpp"

namespace rapid {
    string readFile(fs::path const &f) {
        ifstream file(f);
        if (file) {
            ostringstream oss;
            oss << file.rdbuf();
            return oss.str();
        } else {
            throw runtime_error("cannot open file: " + f.string());
        }
    }
}