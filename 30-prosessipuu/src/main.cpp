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

using std::string;

using std::vector;

// Forward declarations
string getParentPid(string procStat);
string getPidFromEntry(directory_entry dirEntry);
int32_t getPidWidth(); 

vector<string> getPidList(vector<directory_entry> procList);
vector<directory_entry> getProcList();

std::map<string, string> getProcParents(vector<directory_entry> procList);

string readProcStat(directory_entry dirEntry);

struct Process {
    string pid;
    string parentPid;
};

constexpr string PROC_FOLDER = "/proc";
constexpr string PROC_STAT_FOLDER = "/stat";


// Formatter for map<K,V> - assumes that the types can actually be formatted...
// https://www.cppstories.com/2022/custom-stdformat-cpp20/
template <typename K, typename V>
struct std::formatter<std::map<K, V>> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const std::map<K, V>& m, std::format_context& ctx) const {
        auto out = ctx.out();
        bool first = true; // separates entries by ', ' after first entry.

        for (const auto& [k, v] : m) {
            out = std::format_to(out, "{}[{}, {}]", first ? "" : ", ", k, v);
            first = false;
        }
        
        return out;
    }
};

// g++ main.cpp -std=c++23 -o main && ./main

/*
Tehtävänanto: Kerää kaikista /proc-tiedostojärjestelmässä näkyvistä prosesseista 
PID ja ParentPID tiedot. Tulosta puurakenne näistä suhteista.

Ohjelman perusperiaate lienee toimia vastaavasti kuin pstree, joka on kehitetty
alun perin shell-skriptinä 1990-luvulla:
    https://github.com/FredHucht/pstree/tree/main
*/

/*
Linkkejä:
https://gitlab.com/procps-ng/procps/
https://www.kernel.org/doc/html/v4.12/core-api/kernel-api.html
*/

/*
https://www.kernel.org/doc/html/latest/filesystems/proc.html

/proc on virtuaalinen muistissa sijaitseva tiedostojärjestelmä, josta voi 
tavanomaisin menetelmin lukea tietoa. Käyttöjärjestelmä taustalla luo lennosta
tarvittavan tiedon, joka vastaa esim käyttäjän tiedostonlukuoperaatioon.

Käyttöjärjestelmä ei välttämättä osaa etukäteen vastata esim kyselyyn, miten
suuri virtuaalinen tiedosto /proc:ssa on, joten sitä on parasta lukea virtana
esim. stringstreamin avulla niin kauan, kuin käyttöjärjestelmä tarjoaa uutta tietoa.

Prosessilistauksen lukemisen voi tehdä esim. C opendir() readdir() closedir()
funktiokutsuilla tai std::filesystem::directory_iterator avulla.
1. Luetaan /proc :sta lista kaikista prosesseista.
2. Luetaan luupissa /proc/PID/{stat tai status} jokaisen prosessin parent id
3. Rakenetaan soveltuva tietorakenne josta voi tulostaa puun alkaen sen juuresta
4. Tulostetaan tietorakenne sopivassa muodossa formatoituna komentoriville/
   tiedostoon tms.
*/


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
std::map<string, string> getProcParents(vector<directory_entry> procList)
{
    std::map<string, string> procParents{};

    for (auto const& dirEntry : procList) {
        string procStat = readProcStat(dirEntry);

        // TODO figure out why some processes don't yield info
        if (procStat.length() == 0) {
            continue;
        }

        string processPid = getPidFromEntry(dirEntry);
        string parentPid = getParentPid(procStat);

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
    std::string line = "";

    if (iStream.is_open()) {  
        std::getline(iStream, line);
    }

    return line.length();
}


string getPidFromEntry(directory_entry dirEntry) {
    return dirEntry.path().filename().string();
}


/**
 * Extract parent PID from a stat string. It is 4th entry in the string, entries are
 * separated by a space.
 */
string getParentPid(string procStat) {
    auto it = std::views::split(procStat, ' ');
    auto fourth = *std::ranges::next(it.begin(), 3);
    return string(fourth.begin(), fourth.end());
}


/**
 * Read proc stat from a process. Returns an empty string if nothing was read.
 */
string readProcStat(directory_entry dirEntry) {
    const string procStatPath = dirEntry.path().string() + PROC_STAT_FOLDER;
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
vector<directory_entry> getProcList()
{
    vector<directory_entry> procList{};   

    for (auto const& dirEntry : directory_iterator(PROC_FOLDER) ) {
        procList.push_back(dirEntry);
    }

    return procList;
}


/**
 * Extract process pids from the entry list
 */
vector<string> getPidList(vector<directory_entry> procList) {    
    vector<string> pidList{};
    
    for (auto const& dirEntry : procList) {
        pidList.push_back(getPidFromEntry(dirEntry));
    }

    return pidList;
}

