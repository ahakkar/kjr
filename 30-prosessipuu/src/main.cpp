#include <charconv>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

// Using
using std::filesystem::path;
using std::filesystem::directory_iterator;
using DirectoryEntry = std::filesystem::directory_entry;
using String = std::string;  

template <typename T>
using Vector = std::vector<T>;

template <typename K, typename V>
using Map = std::map<K, V>;

// Forward declarations
int32_t getPidWidth(); 

String readProcStat(const DirectoryEntry& dirEntry);

std::optional<int32_t> getParentPid(std::string_view procStat);

std::optional<int32_t> getPidFromEntry(const DirectoryEntry& dirEntry);

Vector<int32_t> getPidList(const Vector<DirectoryEntry>& procList);

Vector<DirectoryEntry> getProcList();

Map<int32_t, int32_t> getProcParents(const Vector<DirectoryEntry>& procList);

Map<int32_t, Vector<int32_t>> getProcTree(const Map<int32_t, int32_t>& procParents);

void sortChildren(Map<int32_t, Vector<int32_t>> &m);

void printTree(
    const int32_t current,
    const Map<int32_t, Vector<int32_t>>& procTree,
    const String& prefix,
    bool isLast,
    bool isRoot
);

struct Process {
    String pid;
    String parentPid;
};

const String PROC_FOLDER = "/proc";
const String PROC_STAT_FOLDER = "/stat";
const String PID_MAX_PATH = "/proc/sys/kernel/pid_max";


// Formatter for map<K,V> - assumes that the types can actually be formatted...
// gcc support for this comes native in 15.1 onwards
// https://www.cppstories.com/2022/custom-stdformat-cpp20/
template <typename K, typename V>
struct std::formatter<Map<K, V>>
{
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


// Copypaste from above but just for vectors
template <typename T>
struct std::formatter<Vector<T>> 
{
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const Vector<T>& v, std::format_context& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "(");        
        bool first = true;

        for (const auto& elem : v) {
            out = std::format_to(out, "{}{}", first ? "" : ", ", elem);
            first = false;
        }
        return std::format_to(out, ")");
    }
};


// Used to parse int32's from pid strings
template <std::integral T = int32_t>
std::optional<T> parseInt(std::string_view sv)
{
    T value{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

    if (ec == std::errc{} && ptr == sv.data() + sv.size()) {
        return value;
    }

    return std::nullopt;
}


int main()
{        
    auto procList = getProcList();    
    auto pidList = getPidList(procList);    
    auto procParents = getProcParents(procList);
    auto procTree = getProcTree(procParents);

    auto it = procTree.begin();
    if (it != procTree.end()) {
        int32_t current  = it->first;        
        printTree(current, procTree, "", false, true);
    }

    return 0;
}


/**
 * Recursively prints the provided parent-children map to console as a tree
 */
void printTree(
    const int32_t current,
    const Map<int32_t, Vector<int32_t>>& procTree,
    const String& prefix = "",
    bool isLast = true,
    bool isRoot = true
)
{
    // Makes root look a bit nicer
    if (isRoot) {
        std::print("{}\n", current);
    } else {
        std::print("{}{}{}\n", prefix, isLast ? "└─ " : "├─ ", current);
    }

    auto it = procTree.find(current);
    if (it == procTree.end()) { return; }

    // Which prefix the next printed child should have?
    const auto& children = it->second;
    String childPrefix = isRoot ? "" : prefix + (isLast ? "   " : "│  ");

    // Recursively call the function for each child
    for (size_t i = 0; i < children.size(); ++i) {
        printTree(
            children[i],
            procTree,
            childPrefix,
            i == children.size() - 1,
            false
        );
    }
}


/**
 * Iterate through the parents dict and construct a dict where each node knows its
 * children
 */
Map<int32_t, Vector<int32_t>> getProcTree(const Map<int32_t, int32_t>& procParents) {
    Map<int32_t, Vector<int32_t>> tree{};

    for (auto const& [current, parent] : procParents) {
        auto [it, inserted] = tree.try_emplace(parent);
        it->second.push_back(current);
    }
    
    sortChildren(tree);
    return tree;
}


/**
 * Sorts the provided map's vectors in ascending order
 */
void sortChildren(Map<int32_t, Vector<int32_t>> &m) {
    for (auto [k, v] : m) {
        std::sort(v.begin(), v.end());
    }
}


/**
 * Read each processes parent id from /proc/PID/stat
 */
Map<int32_t, int32_t> getProcParents(const Vector<DirectoryEntry>& procList)
{
    Map<int32_t, int32_t> procParents{};

    for (auto const& dirEntry : procList) {
        String procStat = readProcStat(dirEntry);

        // TODO figure out why some processes don't yield info
        if (procStat.length() == 0) {
            continue;
        }

        std::optional<int32_t> maybePid = getPidFromEntry(dirEntry);
        std::optional<int32_t> maybePPid = getParentPid(procStat);

        if (!maybePid) { continue; }
        if (!maybePPid) { continue; }

        int32_t processPid = *maybePid;        
        int32_t parentPid = *maybePPid;

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
    std::ifstream iStream(PID_MAX_PATH);
    String line = "";

    if (iStream.is_open()) {  
        std::getline(iStream, line);
    }

    return line.length();
}


/**
 * Extract PID from a /proc directory path
 */
std::optional<int32_t> getPidFromEntry(const DirectoryEntry& dirEntry) {
    return parseInt<int32_t>(dirEntry.path().filename().string());
}


/**
 * Extract parent PID from a stat string. It is 4th entry in the string, entries are
 * separated by a space.
 */
std::optional<int32_t> getParentPid(std::string_view procStat) {
    auto fields = std::views::split(procStat, ' ');
    auto it = std::ranges::next(fields.begin(), 3, fields.end());

    if (it == fields.end()) {
        return std::nullopt; 
    }

    auto fourth = *it;
    return parseInt<int32_t>(std::string_view(fourth.begin(), fourth.end()));
}


/**
 * Read proc stat from a process. Returns an empty string if nothing was read.
 */
String readProcStat(const DirectoryEntry& dirEntry) {
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
Vector<DirectoryEntry> getProcList()
{
    Vector<DirectoryEntry> procList{};   

    for (auto const& dirEntry : directory_iterator(PROC_FOLDER) ) {
        procList.push_back(dirEntry);
    }

    return procList;
}


/**
 * Extract process pids from the entry list
 */
Vector<int32_t> getPidList(const Vector<DirectoryEntry>& procList) {    
    Vector<int32_t> pidList{};
    
    for (auto const& dirEntry : procList) {
        std::optional<int32_t> maybePid = getPidFromEntry(dirEntry);
        if (!maybePid) {
            continue;  
        }
        pidList.push_back(*maybePid);
    }

    return pidList;
}
