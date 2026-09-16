#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

// Using
using std::filesystem::path;
using std::filesystem::directory_entry;
using std::filesystem::directory_iterator;

using String = std::string;  

template <typename T>
using Vector = std::vector<T>;

template <typename K, typename V>
using Map = std::map<K, V>;

// Forward declarations
String getParentPid(String procStat);

String getPidFromEntry(directory_entry dirEntry);

String readProcStat(directory_entry dirEntry);

int32_t getPidWidth(); 

Vector<String> getPidList(Vector<directory_entry> procList);

Vector<directory_entry> getProcList();

Map<String, String> getProcParents(Vector<directory_entry> procList);

struct Process {
    String pid;
    String parentPid;
};

constexpr String PROC_FOLDER = "/proc";
constexpr String PROC_STAT_FOLDER = "/stat";


// Formatter for map<K,V> - assumes that the types can actually be formatted...
// gcc support for this comes native in 15.1 onwards
// https://www.cppstories.com/2022/custom-stdformat-cpp20/
template <typename K, typename V>
struct std::formatter<Map<K, V>> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const Map<K, V>& m, std::format_context& ctx) const {
        auto out = ctx.out();
        bool first = true; // separates entries by ', ' after first entry.

        for (const auto& [k, v] : m) {
            out = std::format_to(out, "{}[{}, {}]", first ? "" : ", ", k, v);
            first = false;
        }
        
        return out;
    }
};


int main()
{        
    auto procList = getProcList();
    auto pidList = getPidList(procList);
    auto procParents = getProcParents(procList);

    std::println("{}", procParents);
    
    return 0;
}


/**
 * Read each processes parent id from /proc/PID/stat
 */
std::map<String, String> getProcParents(Vector<directory_entry> procList)
{
    std::map<String, String> procParents{};

    for (auto const& dirEntry : procList) {
        String procStat = readProcStat(dirEntry);

        // TODO figure out why some processes don't yield info
        if (procStat.length() == 0) {
            continue;
        }

        String processPid = getPidFromEntry(dirEntry);
        String parentPid = getParentPid(procStat);

        // Ignore self process
        if (processPid == "self" || processPid == "thread-self") { continue; }

        procParents.insert_or_assign(processPid, parentPid);
    }

    return procParents;
}


/**
 * Read /proc/sys/kernel/pid_max
 * 7 nums ie 4194304 -> 64bit
 * 5 nums ie 32768   -> 32bit
 */
int32_t getPidWidth() {
    std::ifstream iStream("/proc/sys/kernel/pid_max");
    String line = "";

    if (iStream.is_open()) {  
        std::getline(iStream, line);
    }

    return line.length();
}


String getPidFromEntry(directory_entry dirEntry) {
    return dirEntry.path().filename().string();
}


/**
 * Extract parent PID from a stat string. It is 4th entry in the string, entries are
 * separated by a space.
 */
String getParentPid(String procStat) {
    auto it = std::views::split(procStat, ' ');
    auto fourth = *std::ranges::next(it.begin(), 3);
    return String(fourth.begin(), fourth.end());
}


/**
 * Read proc stat from a process. Returns an empty string if nothing was read.
 */
String readProcStat(directory_entry dirEntry) {
    const String procStatPath = dirEntry.path().string() + PROC_STAT_FOLDER;
    std::ifstream iStream(procStatPath);
    String line = "";

    if (iStream.is_open()) {  
        std::getline(iStream, line);
    }

    return line;
}


/**
 * Get the list of all process entries on system
 */
Vector<directory_entry> getProcList()
{
    Vector<directory_entry> procList{};   

    for (auto const& dirEntry : directory_iterator(PROC_FOLDER) ) {
        procList.push_back(dirEntry);
    }

    return procList;
}


/**
 * Extract process pids from the entry list
 */
Vector<String> getPidList(Vector<directory_entry> procList) {    
    Vector<String> pidList{};
    
    for (auto const& dirEntry : procList) {
        pidList.push_back(getPidFromEntry(dirEntry));
    }

    return pidList;
}

