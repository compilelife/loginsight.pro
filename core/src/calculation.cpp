#include "calculation.h"
#include <thread>
#include <chrono>
#include <utility>
#include <algorithm>
#include "eventloop.h"
#include "stdout.h"
#include <regex>
#include <list>
#include <boost/algorithm/string.hpp>
#ifdef _WIN32
#include <shlwapi.h>
#endif

Calculation::Calculation() {
    mCoreNum = thread::hardware_concurrency();
    LOGI("core num: %d", mCoreNum);
}

Calculation& Calculation::instance() {
    static Calculation impl;
    return impl;
}

template<class Tasks, class ProcessFunc>
shared_ptr<Promise> scheduleImpl(Tasks&& tasks, ProcessFunc&& process) {
    return Promise::from([tasks, process](bool* cancel)mutable{
        vector<future<any>> futures;
        for (auto &&task : tasks) {
            auto futureRet = async(launch::async, process, cancel, move(task));
            futures.push_back(move(futureRet));
        }

        vector<any> rets;
        for (auto &&f : futures)
            rets.push_back(f.get());

        return rets;
    });
}

shared_ptr<Promise> Calculation::schedule(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process) {
    auto tasks = split(iter);
    return scheduleImpl(tasks, process);
}

shared_ptr<Promise> Calculation::schedule(vector<string_view> tasks, 
            function<any(bool*,string_view)>&& process) {
    return scheduleImpl(tasks, process);
}

shared_ptr<Promise> Calculation::peekFirst(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process) {
    return Promise::from([tasks = split(iter), process](bool* cancel){
        list<shared_ptr<Promise>> futures;
        for (auto &&task : tasks) {
            //转换为promise，以方便在获得首个结果后可以cancel之
            auto futureRet = Promise::from([task, process](bool* c){
                return process(c, task);
            });
            futures.push_back(move(futureRet));
        }

        //按入队顺序依次取线程的返回值，如果有值，则返回，否则继续wait
        while (!futures.empty()) {
            futures.front()->wait();
            auto ret = futures.front()->value();

            futures.pop_front();

            if (ret.has_value()) {
                //等待其他计算线程退出，避免这些线程仍在后台执行的情况下，与主线程的其他计算形成多线程竞争
                for (auto &&p : futures)
                    p->cancel();
                
                return ret;
            }
        }
        
        return any();
    });
}

vector<shared_ptr<LogView>> Calculation::split(shared_ptr<LogView>& iter) {
    auto size = iter->size();
    if (size <= BLOCK_LINE_NUM) {
        vector<shared_ptr<LogView>> ret;
        ret.push_back(move(iter));
        return ret;
    }

    LogLineI step = size / mCoreNum;
    LogLineI from = 0;

    vector<shared_ptr<LogView>> ret;

    while (from < size) {
        auto newIter = iter->subview(from, step);
        from += newIter->size();

        ret.push_back(move(newIter));
    }
    
    return ret;
}

//FIXME: utf16le会残留一个0到下一行导致乱码；不能单纯判断\n 0的情况，因为utf16be也是这个特征（0是下一个字符的）
pair<FindLineIter, FindLineIter> findNewLine(FindLineIter from, FindLineIter end) {
    auto newline = find(from, end, '\n');
    auto next = newline + 1;
    if (next > end) {
        next = end;
    }

    //删除\r\n或者\r\r\n中的\r，确保行末整洁
    while (newline != end && newline != from && *(newline - 1) == '\r') {
        --newline;
    }

    return {newline, next};
}

struct StringCase {
    string pattern;
    bool operator() (string_view text) {
        return text.find(pattern) != string_view::npos;
    }
};

struct StringCaseReverse {
    StringCase impl;
    bool operator() (string_view text) {
        return !impl(text);
    }
};

typedef const boost::iterator_range<std::string::const_iterator> StringRange;
struct StringIgnoreCase {
    string pattern;
    bool operator() (string_view text) {
        #ifdef _WIN32
        string s(text);
        return StrStrI(s.c_str(), pattern.c_str()) != NULL;
        #else
        //windows上这个函数的执行速度较慢
        return boost::ifind_first(text, pattern);
        #endif
    }
};

struct StringIgnoreCaseReverse {
    StringIgnoreCase impl;
    bool operator() (string_view text) {
        return !impl(text);
    }
};

struct RegexMatch {
    regex p;
    bool operator() (string_view text) {
        return regex_search(text.begin(), text.end(), p);
    }
};

struct RegexReverseMatch {
    RegexMatch impl;
    bool operator() (string_view text) {
        return !impl(text);
    }
};

FilterFunction createFilter(string_view pattern, bool caseSensitive, bool reverse) {
    if (caseSensitive) {
        if (reverse)
            return StringCaseReverse{pattern.data()};
        else
            return StringCase {pattern.data()};
    }

    if (reverse) 
        return StringIgnoreCaseReverse{pattern.data()};
    
    return StringIgnoreCase{pattern.data()};
}

FilterFunction createFilter(regex r, bool reverse) {
    if (reverse)
        return RegexReverseMatch{r};
    return RegexMatch{r};
}

struct StringCaseFind {
    string pattern;
    FindRet operator () (string_view text) {
        auto pos = text.find(pattern);
        return {
            (LineCharI)pos,
            (LineCharI)(pos == string::npos ? 0 : pattern.length())
        };
    }
};

struct StringCaseReverseFind {
    string pattern;
    FindRet operator () (string_view text) {
        auto pos = text.rfind(pattern);
        return {
            (LineCharI)pos,
            (LineCharI)(pos == string::npos ? 0 : pattern.length())
        };
    }
};

struct StringIgnoreCaseFind {
    string pattern;
    FindRet operator () (string_view text) {
        #ifdef _WIN32
        string s(text);
        auto ret = StrStrI(s.c_str(), pattern.c_str());
        if (!ret)
            return {0,0};
        
        return {
            (LineCharI)(ret - s.c_str()),
            (LineCharI)(pattern.length())
        };
        #else
        auto ret = boost::ifind_first(text, pattern);
        if (!ret)
            return {0,0};

        return {
            (LineCharI)(ret.begin() - text.begin()),
            (LineCharI)(ret.end() - ret.begin())
        };
        #endif
    }
};

struct StringIgnoreCaseReverseFind {
    string pattern;
    FindRet operator () (string_view text) {
        #ifdef _WIN32
        string s(text);
        auto ret = StrRStrI(s.c_str(), NULL, pattern.c_str());
        if (!ret)
            return {0,0};
        
        return {
            (LineCharI)(ret - s.c_str()),
            (LineCharI)(pattern.length())
        };
        #else
        auto ret = boost::ifind_last(text, pattern);
        if (!ret)
            return {0,0};

        return {
            (LineCharI)(ret.begin() - text.begin()),
            (LineCharI)(ret.end() - ret.begin())
        };
        #endif
    }
};

struct RegexMatchFind {
    regex p;
    FindRet operator () (string_view text) {
        smatch results;
        string s(text);
        auto ok = regex_search(s, results, p);
        FindRet ret {0,0};
        if (ok) {
            ret.offset = results.position(0),
            ret.len = results.length(0);
        }
        return ret;
    }
};

struct RegexMatchReverseFind {
    regex p;
    FindRet operator () (string_view text) {
        smatch results;
        string s(text);
        auto it = regex_iterator<string_view::iterator>(text.begin(), text.end(), p);
        FindRet ret;
        for (decltype(it) last; it != last; ++it) {
            ret.offset = it->position(0);
            ret.len = it->length(0);
        }
        return ret;
    }
};

FindFunction createFind(string_view pattern, bool caseSensitive, bool reverse) {
    if (caseSensitive) {
        if (reverse)
            return StringCaseReverseFind{pattern.data()};
        else
            return StringCaseFind{pattern.data()};
    }
    
    if (reverse)
        return StringIgnoreCaseReverseFind{pattern.data()};
    return StringIgnoreCaseFind{pattern.data()};
}

FindFunction createFind(regex pattern, bool reverse) {
    if (reverse)
        return RegexMatchReverseFind{pattern};
    else
        return RegexMatchFind{pattern};
}