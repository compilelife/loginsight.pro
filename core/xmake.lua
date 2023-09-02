add_rules("mode.debug", "mode.release")
set_policy("package.precompiled", false)

add_requires("libevent", {system=false})
add_requires("jsoncpp", {system=false})
add_requires("gtest 1.11.0")
add_requires("libcurl", {system=false})
add_requires("oatpp")
add_requires("boost")

set_languages("c++17")

--核心代码
target("corelib") 
    set_kind("static")
    set_installdir('../app')
    add_files("src/*.cpp|main.cpp")
    if is_os("linux") then 
        add_files("src/unix/*.cpp")
    elseif is_os("windows") then
        add_files("src/windows/*.cpp")
        add_cflags("/utf-8")
        add_cxxflags("/utf-8")
        add_defines("_HAS_STD_BYTE=0") --https://blog.csdn.net/qq_44894692/article/details/121129688
    elseif is_os("macosx") then
        add_files("src/unix/*.cpp")
    end
    add_packages("libevent", "jsoncpp", "libcurl", "oatpp", "boost")

--最终输出
target("core")
    set_kind("binary")
    set_installdir("../app")
    add_files("src/main.cpp")
    add_deps("corelib")
    add_ldflags("-static-libstdc++")
    set_basename("core.$(os)")
    if is_os("windows") then
        add_cflags("/utf-8")
        add_cxxflags("/utf-8")
        add_defines("_HAS_STD_BYTE=0")
    end
    add_packages("libevent", "jsoncpp", "libcurl", "oatpp", "boost")


--单元测试
target("utest")
    set_kind("binary")
    add_files("test/*.cpp")
    add_deps("corelib")
    add_includedirs("src")
    if is_os("windows") then
        add_cflags("/utf-8")
        add_cxxflags("/utf-8")
        add_defines("_HAS_STD_BYTE=0")
    end
    add_packages("gtest","libevent", "jsoncpp", "libcurl", "oatpp", "boost")
    after_build(function(target)
        os.cp("test/assets/", "$(buildir)/")
    end)
