#include "gtest/gtest.h"
#include <filesystem>
#include <string_view>

#ifdef _WIN32
#include <direct.h>
// MSDN recommends against using getcwd & chdir names
#define getWorkingDir _getcwd
#define setWorkingDir _chdir
#else
#include "unistd.h"
#define getWorkingDir getcwd
#define setWorkingDir chdir
#endif

#include "stdout.h"

using namespace std;
using namespace std::filesystem;

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc,argv);

#ifdef _WIN32
    string_view assetPath = "..\\..\\..\\assets";
    string_view srcAssetPath = "..\\test\\assets";
#else
    string_view assetPath = "../../../assets";
    string_view srcAssetPath = "../test/assets";
#endif
    if (!exists(assetPath))
        create_directory_symlink(srcAssetPath, assetPath);

    char buf[256];
    getWorkingDir(buf, 256);
    LOGI("working dir: %s", buf);
    setWorkingDir(assetPath.data());
    getWorkingDir(buf, 256);
    LOGI("change working dir to : %s", buf);
    
    return RUN_ALL_TESTS();
}