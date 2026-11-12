#include "laws.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace luca {

const std::vector<LawInfo>& law_table() {
    static const std::vector<LawInfo> table = {
#define LUCA_LAW_INFO(name, value, where) {#name, where, &Laws::name},
        LUCA_LAWS(LUCA_LAW_INFO)
#undef LUCA_LAW_INFO
    };
    return table;
}

const LawInfo* find_law(const std::string& name) {
    for (const LawInfo& info : law_table()) {
        const std::string n = info.name;
        if (n.size() != name.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < n.size() && same; ++i)
            same = std::toupper(static_cast<unsigned char>(name[i])) == n[i];
        if (same) return &info;
    }
    return nullptr;
}

LawChanges laws_diff(const Laws& from, const Laws& to) {
    LawChanges out;
    for (const LawInfo& info : law_table())
        if (to.*info.field != from.*info.field) out.emplace_back(info.name, to.*info.field);
    return out;
}

LawChanges changed_laws(const Laws& l) { return laws_diff(Laws{}, l); }

void apply_laws(Laws& l, const LawChanges& changes) {
    for (const auto& [name, value] : changes)
        if (const LawInfo* info = find_law(name)) l.*info->field = value;
}

namespace {

bool parse_one(const std::string& name, const std::string& value, LawChanges& out, std::string& error) {
    const LawInfo* info = find_law(name);
    if (!info) {
        error = "no law named " + name;
        return false;
    }
    char* end = nullptr;
    const float v = std::strtof(value.c_str(), &end);
    if (value.empty() || *end != '\0' || !std::isfinite(v)) {
        error = "bad value for " + name + ": " + value;
        return false;
    }
    out.emplace_back(info->name, v);
    return true;
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

}  // namespace

bool parse_law(const std::string& assignment, LawChanges& out, std::string& error) {
    const std::size_t eq = assignment.find('=');
    if (eq == std::string::npos) {
        error = "expected NAME=VALUE, got " + assignment;
        return false;
    }
    return parse_one(trim(assignment.substr(0, eq)), trim(assignment.substr(eq + 1)), out, error);
}

bool read_laws_file(const std::string& path, LawChanges& out, std::string& error) {
    std::ifstream f(path);
    if (!f) {
        error = "cannot read " + path;
        return false;
    }
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        ++n;
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        line = trim(line);
        if (line.empty()) continue;
        std::size_t split = line.find('=');
        if (split == std::string::npos) split = line.find_first_of(" \t");
        if (split == std::string::npos) {
            error = path + ":" + std::to_string(n) + ": expected NAME VALUE";
            return false;
        }
        if (!parse_one(trim(line.substr(0, split)), trim(line.substr(split + 1)), out, error)) {
            error = path + ":" + std::to_string(n) + ": " + error;
            return false;
        }
    }
    return true;
}

}  // namespace luca
