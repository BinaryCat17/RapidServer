#pragma once
#include "utils.hpp"
#include <filesystem>
#include <fstream>
#include <map>

namespace rapid {
    namespace fs = filesystem;

    template<typename T>
    struct Explorer {
        static string_view file(T && e, fs::path const &f);
    };

    string readFile(fs::path const &f);

    class CacheExplorer {
    public:
        explicit CacheExplorer(fs::path root) : mRoot(move(root)) {}

        string_view file(fs::path const &f) const {
            return atOrInsert(mCache, f, curry(readFile, mRoot / f));
        }

    private:
        fs::path mRoot;
        mutable map<fs::path, string> mCache;
    };

    template<>
    struct Explorer<CacheExplorer> {
        static string_view file(CacheExplorer const&e, fs::path const &f) {
            return e.file(f);
        }
    };




}