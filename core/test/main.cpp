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

using namespace std;
using namespace std::filesystem;

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc,argv);

    string_view assetPath = "../../../assets";
    if (!exists(assetPath))
        create_directory_symlink("../test/assets", assetPath);
    setWorkingDir(assetPath.data());
    
    return RUN_ALL_TESTS();
}