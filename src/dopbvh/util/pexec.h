#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace pexec {

inline static std::filesystem::path get_path_to_executable()
{
#if defined(_WIN32)
    std::vector<char> buffer(MAX_PATH);
    DWORD length = GetModuleFileNameA(nullptr, buffer.data(), csize<DWORD>(buffer));
    std::string path(buffer.begin(), buffer.begin() + length);
    return path;
#else
    return std::filesystem::canonical("/proc/self/exe");
#endif
}

}
