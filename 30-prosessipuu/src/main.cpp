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
using directory_entry = std::filesystem::directory_entry;

// Forward declarations
std::string readProcStat(const directory_entry& dirEntry);

std::optional<int32_t> getParentPid(std::string_view procStat);

std::optional<int32_t> getPidFromEntry(const directory_entry& dirEntry);

std::vector<directory_entry> getProcList();

std::map<int32_t, int32_t> getProcParents(const std::vector<directory_entry>& procList);

std::map<int32_t, std::vector<int32_t>> getProcTree(const std::map<int32_t, int32_t>& procParents);

void sortChildren(std::map<int32_t, std::vector<int32_t>> &m);

void printTree(
    const int32_t current,
    const std::map<int32_t, std::vector<int32_t>>& procTree,
    const std::string& prefix,
    bool isLast,
    bool isRoot
);

const std::string PROC_FOLDER = "/proc";
const std::string PROC_STAT_FOLDER = "/stat";
const std::string PID_MAX_PATH = "/proc/sys/kernel/pid_max";


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


/**
 * Program flow follows steps 1-4 from README.md
 */
int main()
{        
    auto procList = getProcList();    
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
    const std::map<int32_t, std::vector<int32_t>>& procTree,
    const std::string& prefix = "",
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
    std::string childPrefix = isRoot ? "" : prefix + (isLast ? "   " : "│  ");

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
std::map<int32_t, std::vector<int32_t>> getProcTree(const std::map<int32_t, int32_t>& procParents) {
    std::map<int32_t, std::vector<int32_t>> tree{};

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
void sortChildren(std::map<int32_t, std::vector<int32_t>> &m) {
    for (auto [k, v] : m) {
        std::sort(v.begin(), v.end());
    }
}


/**
 * Read each processes parent id from /proc/PID/stat
 */
std::map<int32_t, int32_t> getProcParents(const std::vector<directory_entry>& procList)
{
    std::map<int32_t, int32_t> procParents{};

    for (auto const& dirEntry : procList) {
        std::string procStat = readProcStat(dirEntry);

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
 * Extract PID from a /proc directory path
 */
std::optional<int32_t> getPidFromEntry(const directory_entry& dirEntry) {
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
std::string readProcStat(const directory_entry& dirEntry) {
    const std::string procStatPath = dirEntry.path().string() + PROC_STAT_FOLDER;
    std::ifstream iStream(procStatPath);
    std::string line = "";

    if (iStream.is_open()) {  
        std::getline(iStream, line);
    }

    return line;
}


/**
 * Get the list of all process entries on system
 */
std::vector<directory_entry> getProcList()
{
    std::vector<directory_entry> procList{};   

    for (auto const& dirEntry : directory_iterator(PROC_FOLDER) ) {
        procList.push_back(dirEntry);
    }

    return procList;
}
