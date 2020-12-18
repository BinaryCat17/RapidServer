#include "utils.hpp"
#include <fstream>

namespace rapid {
    std::string readFile(std::filesystem::path const &f) {
        std::ifstream file(f);
        if (file) {
            std::ostringstream oss;
            oss << file.rdbuf();
            return oss.str();
        } else {
            throw std::runtime_error("cannot open file: " + f.string());
        }
    }
}
