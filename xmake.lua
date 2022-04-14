add_rules("mode.debug", "mode.release")

--这样引入的libevent编译出来是非多线程的，也就是所有的调用libevent调用最好都在同一个线程里
add_requires("libevent", {system=false})
add_requires("jsoncpp", {system=false})
add_requires("gtest 1.11.0")
set_languages("c++17")

--核心代码
target("corelib") 
    set_kind("static")
    add_files("src/*.cpp|main.cpp")
    if is_os("linux") then 
        add_files("src/unix/*.cpp")
    elseif is_os("windows") then
        add_files("src/windows/*.cpp")
    elseif is_os("macosx") then
        add_files("src/unix/*.cpp")
    end
    add_packages("libevent", "jsoncpp")

--最终输出
target("core")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("corelib")

--单元测试
target("utest")
    set_kind("binary")
    add_files("test/*.cpp")
    add_deps("corelib")
    add_includedirs("src")
    add_packages("gtest")
    after_build(function(target)
        os.cp("test/assets/", "$(buildir)/")
    end)