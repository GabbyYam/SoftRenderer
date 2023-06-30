#pragma once
#include <string>

namespace soft {
    class FileDialogs {
    public:
        static std::string OpenFile(const char* filter);

        static std::string SaveFile(const char* filter);
    };
}  // namespace soft