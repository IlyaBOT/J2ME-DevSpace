#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

#ifndef J2ME_CLI_VERSION
#define J2ME_CLI_VERSION "0.3"
#endif

#ifndef J2ME_INSTALL_DATADIR
#define J2ME_INSTALL_DATADIR ""
#endif

struct AppMeta {
    std::string name = "J2ME DevSpace";
    std::string id = "j2me-devspace";
    std::string cli = "j2me";
    std::string version = J2ME_CLI_VERSION;
    std::string channel = "beta";
    std::string codename = "Vector";
    std::string defaultProfile = "lg-gs290";
    std::string githubRepository = "IlyaBOT/J2ME-DevSpace";
};

struct PackageOption {
    std::string id;
    std::string type;
    std::string version;
    std::string label;
    std::string source;
    std::string url;
    std::string urlTemplate;
    std::string mirrorUrl;
    std::string mirrorUrlTemplate;
    std::string archive;
    std::string manualUrl;
    std::string notes;
    std::vector<std::string> supportedOs;
    std::vector<std::string> supportedArch;
    bool selectedByDefault = false;
    bool manual = false;
};

static std::string readAll(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeAll(const fs::path &p, const std::string &s) {
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f << s;
}

static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static bool hasArg(const std::vector<std::string> &args, const std::string &a) {
    return std::find(args.begin(), args.end(), a) != args.end();
}

static bool endsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string argAfter(const std::vector<std::string> &args, const std::string &a, const std::string &fallback = "") {
    for (size_t i = 0; i + 1 < args.size(); ++i) if (args[i] == a) return args[i + 1];
    return fallback;
}

static json parseJsonText(const std::string &text, json fallback = json::object()) {
    if (trim(text).empty()) return fallback;
    try {
        return json::parse(text);
    } catch (const std::exception &) {
        return fallback;
    }
}

static json readJsonFile(const fs::path &p, json fallback = json::object()) {
    return parseJsonText(readAll(p), fallback);
}

static std::optional<json> readJsonFileStrict(const fs::path &p, const std::string &schema = "") {
    try {
        json j = json::parse(readAll(p));
        if (!schema.empty()) {
            if (!j.is_object() || !j.contains("schema") || !j.at("schema").is_string()) return std::nullopt;
            if (j.at("schema").get<std::string>() != schema) return std::nullopt;
        }
        return j;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

static void writeJsonFile(const fs::path &p, const json &j) {
    writeAll(p, j.dump(2) + "\n");
}

static std::string jstr(const json &j, const std::string &key, const std::string &fallback = "") {
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_string()) return fallback;
    return j.at(key).get<std::string>();
}

static int jint(const json &j, const std::string &key, int fallback = 0) {
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_number_integer()) return fallback;
    return j.at(key).get<int>();
}

static std::string jsonString(const std::string &jsonText, const std::string &key, const std::string &fallback = "") {
    return jstr(parseJsonText(jsonText), key, fallback);
}

static int jsonInt(const std::string &jsonText, const std::string &key, int fallback = 0) {
    return jint(parseJsonText(jsonText), key, fallback);
}

static std::vector<std::string> jsonArrayStrings(const std::string &jsonText, const std::string &key) {
    std::vector<std::string> out;
    json j = parseJsonText(jsonText);
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_array()) return out;
    for (const auto &item : j.at(key)) {
        if (item.is_string()) out.push_back(item.get<std::string>());
    }
    return out;
}

static std::vector<std::string> jsonArrayStrings(const json &j, const std::string &key) {
    std::vector<std::string> out;
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_array()) return out;
    for (const auto &item : j.at(key)) {
        if (item.is_string()) out.push_back(item.get<std::string>());
    }
    return out;
}

static std::string javaStringEscape(const std::string &s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default: o << c; break;
        }
    }
    return o.str();
}

static std::string nowIsoLocal() {
    std::time_t t = std::time(nullptr);
    std::tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return std::string(buf);
}

static bool startsWithPath(fs::path child, fs::path parent) {
    child = fs::weakly_canonical(child);
    parent = fs::weakly_canonical(parent);
    auto c = child.begin();
    auto p = parent.begin();
    for (; p != parent.end(); ++p, ++c) {
        if (c == child.end() || *c != *p) return false;
    }
    return true;
}

static fs::path absPath(const fs::path &p) {
    std::error_code ec;
    fs::path a = fs::absolute(p, ec);
    if (ec) return p;
    return a.lexically_normal();
}

static bool isSafeName(const std::string &s) {
    if (s.empty() || s == "." || s == "..") return false;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.') continue;
        return false;
    }
    return true;
}

static bool isSafeFileName(const std::string &s, const std::string &requiredExt = "") {
    if (!isSafeName(s)) return false;
    fs::path p(s);
    if (p.is_absolute() || p.has_parent_path() || p.filename().string() != s) return false;
    if (!requiredExt.empty() && p.extension() != requiredExt) return false;
    return true;
}

static bool isSafeJavaQualifiedName(const std::string &s) {
    if (s.empty() || s.front() == '.' || s.back() == '.') return false;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, '.')) {
        if (part.empty()) return false;
        if (!std::isalpha(static_cast<unsigned char>(part[0])) && part[0] != '_') return false;
        for (unsigned char c : part) {
            if (!std::isalnum(c) && c != '_') return false;
        }
    }
    return true;
}

static bool rejectUnsafeName(const std::string &kind, const std::string &value) {
    if (isSafeName(value)) return false;
    std::cerr << "Invalid " << kind << " name: " << value << "\n"
              << "Use only letters, digits, '.', '_' and '-'. Path separators are not allowed.\n";
    return true;
}

static std::string descriptorValue(const std::string &s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '\r' || c == '\n') continue;
        if (c < 32 && c != '\t') continue;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

static std::string xmlEscape(const std::string &s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

static fs::path findToolHome(const char *argv0) {
    if (const char *env = std::getenv("J2ME_DEVSPACE_HOME")) {
        fs::path p = absPath(env);
        if (fs::exists(p / "j2me.devspace.yml")) return p;
    }

    fs::path exe = argv0 ? fs::path(argv0) : fs::path();
    if (!exe.empty()) {
        if (exe.is_relative()) exe = fs::current_path() / exe;
        exe = absPath(exe);
        fs::path dir = exe.parent_path();
        std::vector<fs::path> candidates = {
            dir,
            dir.parent_path(),
            dir.parent_path() / "share" / "j2me-devspace",
            dir.parent_path().parent_path(),
            dir.parent_path().parent_path() / "share" / "j2me-devspace",
            fs::current_path(),
            fs::current_path().parent_path()
        };
        for (const auto &c : candidates) {
            if (!c.empty() && fs::exists(c / "j2me.devspace.yml")) return absPath(c);
        }
    }
    if (std::string(J2ME_INSTALL_DATADIR).size() > 0) {
        fs::path installed = absPath(J2ME_INSTALL_DATADIR);
        if (fs::exists(installed / "j2me.devspace.yml")) return installed;
    }
    return absPath(fs::current_path());
}

static std::string yamlScalar(const YAML::Node &n, const std::string &key, const std::string &fallback = "") {
    try {
        if (n && n[key] && n[key].IsScalar()) return n[key].as<std::string>();
    } catch (const std::exception &) {}
    return fallback;
}

static AppMeta loadAppMeta(const fs::path &toolHome) {
    AppMeta m;
    fs::path metaPath = toolHome / "j2me.devspace.yml";
    if (!fs::exists(metaPath)) return m;

    try {
        YAML::Node y = YAML::LoadFile(metaPath.string());
        m.name = yamlScalar(y, "name", m.name);
        m.id = yamlScalar(y, "id", m.id);
        m.cli = yamlScalar(y, "cli", m.cli);
        m.version = yamlScalar(y, "version", m.version);
        m.channel = yamlScalar(y, "channel", m.channel);
        m.codename = yamlScalar(y, "codename", m.codename);
        m.defaultProfile = yamlScalar(y, "default_profile", m.defaultProfile);
        m.githubRepository = yamlScalar(y, "github_repository", m.githubRepository);
    } catch (const std::exception &e) {
        std::cerr << "Warning: failed to read " << metaPath << ": " << e.what() << "\n";
    }
    return m;
}

static std::string formatAppVersion(const AppMeta &m) {
    std::ostringstream out;
    out << m.name << " " << m.version;
    if (!m.channel.empty()) out << "-" << m.channel;
    if (!m.codename.empty()) out << " (" << m.codename << ")";
    return out.str();
}

static fs::path findWorkspaceRoot() {
    if (const char *env = std::getenv("J2ME_WORKSPACE")) {
        fs::path p = absPath(env);
        json marker = readJsonFile(p / "j2me.workspace.json");
        if (jstr(marker, "schema") == "j2me.workspace/1") return p;
    }
    fs::path p = absPath(fs::current_path());
    for (;;) {
        fs::path markerPath = p / "j2me.workspace.json";
        if (fs::exists(markerPath)) {
            json marker = readJsonFile(markerPath);
            if (jstr(marker, "schema") == "j2me.workspace/1") return p;
        }
        if (!p.has_parent_path() || p == p.parent_path()) break;
        p = p.parent_path();
    }
    return {};
}

static int requireWorkspace(const fs::path &ws) {
    if (!ws.empty()) return 0;
    std::cerr << "J2ME workspace not found in current directory tree.\n"
              << "Create one first, then cd into it:\n"
              << "  j2me --workspace setup <directory>\n"
              << "  cd <directory>\n";
    return 2;
}

static std::string shellQuote(const std::string &s) {
#ifdef _WIN32
    std::string out = "\"";
    for (char c : s) out += (c == '"' ? "\\\"" : std::string(1, c));
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char c : s) out += (c == '\'' ? "'\\''" : std::string(1, c));
    out += "'";
    return out;
#endif
}

static std::string shellQuote(const fs::path &p) {
    return shellQuote(p.string());
}

static std::string powershellQuote(const std::string &s) {
    std::string out = "'";
    for (char c : s) out += (c == '\'' ? "''" : std::string(1, c));
    out += "'";
    return out;
}

static int runCommand(const std::string &cmd) {
    std::cout << "> " << cmd << "\n";
    int status = std::system(cmd.c_str());
    if (status == -1) return 127;
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;
#endif
}

static bool copyDirIfExists(const fs::path &from, const fs::path &to);

static std::string hostOsToken() {
#ifdef _WIN32
    return "windows";
#elif __APPLE__
    return "mac";
#else
    return "linux";
#endif
}

static std::string hostArchToken() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "x64";
#endif
}

static std::string replaceAll(std::string s, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

static std::string expandPackageUrl(std::string url) {
    url = replaceAll(url, "{os}", hostOsToken());
    url = replaceAll(url, "{arch}", hostArchToken());
    return url;
}

static std::string defaultArchiveNameForPackage(const PackageOption &pkg, const std::string &current) {
    if (pkg.id == "temurin-jdk8") {
        std::string ext = hostOsToken() == "windows" ? ".zip" : ".tar.gz";
        return "OpenJDK8U-jdk_" + hostArchToken() + "_" + hostOsToken() + "_hotspot_8u492b09" + ext;
    }
    return current;
}

static std::string defaultDownloadUrlForPackage(const PackageOption &pkg, const std::string &archiveName) {
    if (pkg.id == "temurin-jdk8") {
        return "https://github.com/adoptium/temurin8-binaries/releases/download/jdk8u492-b09/" + archiveName;
    }
    if (pkg.id == "apache-ant") {
        return "https://archive.apache.org/dist/ant/binaries/apache-ant-1.10.16-bin.zip";
    }
    if (pkg.id == "proguard") {
        return "https://github.com/Guardsquare/proguard/releases/download/v7.9.1/proguard-7.9.1.zip";
    }
    if (pkg.id == "microemulator") {
        return "https://sourceforge.net/projects/microemulator/files/microemulator/2.0.4/microemulator-2.0.4.zip/download";
    }
    return "";
}

static std::string defaultMirrorUrlForPackage(const PackageOption &pkg, const std::string &archiveName) {
    if (pkg.type == "emulator") {
        return "https://ibifs.ddns.net/%D0%9F%D1%80%D0%BE%D1%87%D0%B5%D0%B5/j2me-devspace/emulators/" + archiveName;
    }
    if (pkg.type == "jdk" || pkg.type == "ant" || pkg.type == "proguard" || pkg.type == "wtk") {
        return "https://ibifs.ddns.net/%D0%9F%D1%80%D0%BE%D1%87%D0%B5%D0%B5/j2me-devspace/sdk/" + archiveName;
    }
    return "";
}

static std::vector<PackageOption> readPackageIndex(const fs::path &ws) {
    std::vector<PackageOption> out;
    fs::path indexPath = ws / "packages" / "index.json";
    if (!fs::exists(indexPath)) indexPath = ws / "packages" / "index.example.json";
    json idx = readJsonFile(indexPath);
    if (!idx.is_object() || jstr(idx, "schema") != "j2me.packages/1" || !idx.contains("packages") || !idx.at("packages").is_array()) {
        return out;
    }
    for (const auto &p : idx.at("packages")) {
        if (!p.is_object()) continue;
        PackageOption opt;
        opt.id = jstr(p, "id");
        opt.type = jstr(p, "type");
        opt.version = jstr(p, "version");
        opt.label = jstr(p, "label", opt.id);
        opt.source = jstr(p, "source");
        opt.url = jstr(p, "url");
        opt.urlTemplate = jstr(p, "url_template");
        opt.mirrorUrl = jstr(p, "mirror_url");
        opt.mirrorUrlTemplate = jstr(p, "mirror_url_template");
        opt.archive = jstr(p, "archive");
        opt.manualUrl = jstr(p, "manual_url");
        opt.notes = jstr(p, "notes");
        opt.supportedOs = jsonArrayStrings(p, "supported_os");
        opt.supportedArch = jsonArrayStrings(p, "supported_arch");
        opt.selectedByDefault = p.contains("default") && p.at("default").is_boolean() && p.at("default").get<bool>();
        opt.manual = p.contains("manual") && p.at("manual").is_boolean() && p.at("manual").get<bool>();
        if (!opt.id.empty() && !opt.type.empty() && !opt.version.empty()) out.push_back(opt);
    }
    return out;
}

static bool stringListContains(const std::vector<std::string> &xs, const std::string &x) {
    return std::find(xs.begin(), xs.end(), x) != xs.end();
}

static bool packageSupportedOnHost(const PackageOption &pkg) {
    if (!pkg.supportedOs.empty() && !stringListContains(pkg.supportedOs, hostOsToken())) return false;
    if (!pkg.supportedArch.empty() && !stringListContains(pkg.supportedArch, hostArchToken())) return false;
    return true;
}

static std::vector<PackageOption> packagesByType(const std::vector<PackageOption> &all, const std::string &type) {
    std::vector<PackageOption> out;
    for (const auto &p : all) if (p.type == type && packageSupportedOnHost(p)) out.push_back(p);
    return out;
}

static std::string packageDisplayName(const PackageOption &p) {
    return p.id + "-" + p.version;
}

static bool dirHasFiles(const fs::path &dir) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) return false;
    return fs::directory_iterator(dir) != fs::directory_iterator();
}

static fs::path singleExtractedRoot(const fs::path &dir) {
    fs::path only;
    size_t count = 0;
    if (!fs::exists(dir)) return {};
    for (const auto &e : fs::directory_iterator(dir)) {
        if (!e.is_directory()) continue;
        only = e.path();
        ++count;
    }
    return count == 1 ? only : dir;
}

static int installCurrentLink(const fs::path &base, const fs::path &versionDir) {
    fs::path cur = base / "current";
    std::error_code ec;
    fs::remove_all(cur, ec);
    fs::create_directory_symlink(versionDir.filename(), cur, ec);
    if (!ec) return 0;
    ec.clear();
    copyDirIfExists(versionDir, cur);
    return fs::exists(cur) ? 0 : 2;
}

static int downloadFile(const std::string &url, const fs::path &dest) {
    fs::create_directories(dest.parent_path());
#ifdef _WIN32
    std::string cmd = "curl.exe -L --fail --retry 3 -A j2me-devspace -o " + shellQuote(dest) + " " + shellQuote(url);
#else
    std::string cmd = "curl -L --fail --retry 3 -A j2me-devspace -o " + shellQuote(dest) + " " + shellQuote(url);
#endif
    return runCommand(cmd);
}

static int downloadFileWithMirror(const std::string &url, const std::string &mirrorUrl, const fs::path &dest) {
    int rc = downloadFile(url, dest);
    if (rc == 0 || mirrorUrl.empty()) return rc;

    std::cerr << "\033[33mWarning: download from URL " << url
              << " failed, retrying from j2me-devspace developer mirror.\033[0m\n";
    return downloadFile(mirrorUrl, dest);
}

static int extractArchivePortable(const fs::path &archive, const fs::path &dest) {
    fs::create_directories(dest);
    std::string archiveName = archive.filename().string();
    int rc = 2;
#ifdef _WIN32
    rc = runCommand("tar -xf " + shellQuote(archive) + " -C " + shellQuote(dest));
    if (rc == 0) return 0;
    if (endsWith(archiveName, ".zip")) {
        rc = runCommand("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath " + powershellQuote(archive.string()) + " -DestinationPath " + powershellQuote(dest.string()) + " -Force\"");
        if (rc == 0) return 0;
    }
#else
    if (endsWith(archiveName, ".zip")) {
        rc = runCommand("unzip -q " + shellQuote(archive) + " -d " + shellQuote(dest));
        if (rc == 0) return 0;
    } else {
        rc = runCommand("tar -xf " + shellQuote(archive) + " -C " + shellQuote(dest));
        if (rc == 0) return 0;
    }
#endif
    return runCommand("cd " + shellQuote(dest) + " && cmake -E tar xf " + shellQuote(archive));
}

static int installPackage(const fs::path &ws, const PackageOption &pkg) {
    fs::path base = ws / "sdk" / pkg.type;
    fs::path dest = base / pkg.version;
    fs::create_directories(base);

    if (pkg.manual) {
        std::cout << "\nManual package: " << packageDisplayName(pkg) << "\n";
        if (!pkg.manualUrl.empty()) std::cout << "Download: " << pkg.manualUrl << "\n";
        if (!pkg.notes.empty()) std::cout << pkg.notes << "\n";
        std::cout << "Expected directory: " << dest << "\n";
        return 0;
    }

    if (dirHasFiles(dest)) {
        std::cout << packageDisplayName(pkg) << " already has files: " << dest << "\n";
        std::cout << "Reinstall? [y/N]: ";
        std::string answer;
        std::getline(std::cin, answer);
        if (trim(answer) != "y" && trim(answer) != "Y") {
            installCurrentLink(base, dest);
            return 0;
        }
        std::error_code ec;
        fs::remove_all(dest, ec);
    }

    std::string archiveName = pkg.archive.empty() ? packageDisplayName(pkg) : expandPackageUrl(pkg.archive);
    if (archiveName.find('.') == std::string::npos) {
#ifdef _WIN32
        archiveName += ".zip";
#else
        archiveName += ".tar.gz";
#endif
    }
    archiveName = defaultArchiveNameForPackage(pkg, archiveName);
    std::string url = expandPackageUrl(!pkg.url.empty() ? pkg.url : pkg.urlTemplate);
    url = replaceAll(url, "{archive}", archiveName);
    if (url.empty()) url = defaultDownloadUrlForPackage(pkg, archiveName);
    std::string mirrorUrl = expandPackageUrl(!pkg.mirrorUrl.empty() ? pkg.mirrorUrl : pkg.mirrorUrlTemplate);
    mirrorUrl = replaceAll(mirrorUrl, "{archive}", archiveName);
    if (mirrorUrl.empty()) mirrorUrl = defaultMirrorUrlForPackage(pkg, archiveName);
    if (url.empty() && !mirrorUrl.empty()) {
        url = mirrorUrl;
        mirrorUrl.clear();
    }
    if (url.empty()) {
        std::cerr << "No download URL for " << packageDisplayName(pkg) << "\n";
        return 2;
    }
    fs::path archivePath = ws / "cache" / archiveName;
    fs::path extractDir = ws / "temp" / ("extract-" + pkg.id + "-" + pkg.version);
    std::error_code ec;
    fs::remove_all(extractDir, ec);

    std::cout << "\nDownloading " << packageDisplayName(pkg) << "\n";
    std::cout << "URL: " << url << "\n";
    if (!mirrorUrl.empty()) std::cout << "Mirror: " << mirrorUrl << "\n";
    if (int rc = downloadFileWithMirror(url, mirrorUrl, archivePath)) return rc;

    if (endsWith(archiveName, ".bin.sh") || endsWith(archiveName, ".sh")) {
        fs::create_directories(dest);
        fs::path installer = dest / archiveName;
        fs::copy_file(archivePath, installer, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Failed to store installer script in " << dest << ": " << ec.message() << "\n";
            return 2;
        }
#ifndef _WIN32
        fs::permissions(installer,
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add,
                        ec);
#endif
        writeJsonFile(dest / "j2me.package.json", json{
            {"schema", "j2me.package/1"},
            {"id", pkg.id},
            {"type", pkg.type},
            {"version", pkg.version},
            {"source", pkg.source},
            {"url", url},
            {"installer_only", true},
            {"installer", archiveName}
        });
        installCurrentLink(base, dest);
        std::cout << "Downloaded installer script: " << installer << "\n";
        std::cout << "This legacy package still requires running the vendor installer manually.\n";
        return 0;
    }

    std::cout << "Extracting to " << dest << "\n";
    if (int rc = extractArchivePortable(archivePath, extractDir)) return rc;

    fs::path root = singleExtractedRoot(extractDir);
    if (root.empty() || !fs::exists(root)) {
        std::cerr << "Archive extraction produced no installable directory.\n";
        return 2;
    }
    fs::create_directories(dest.parent_path());
    fs::rename(root, dest, ec);
    if (ec) {
        ec.clear();
        fs::create_directories(dest, ec);
        if (!copyDirIfExists(root, dest)) {
            std::cerr << "Failed to install extracted package into " << dest << "\n";
            return 2;
        }
    }
    fs::remove_all(extractDir, ec);
    writeJsonFile(dest / "j2me.package.json", json{
        {"schema", "j2me.package/1"},
        {"id", pkg.id},
        {"type", pkg.type},
        {"version", pkg.version},
        {"source", pkg.source},
        {"url", url}
    });
    installCurrentLink(base, dest);
    std::cout << "Installed: " << dest << "\n";
    return 0;
}

static int installEmulatorPackage(const fs::path &ws, const PackageOption &pkg) {
    fs::path dest = ws / "emulators" / pkg.id;
    fs::create_directories(dest.parent_path());

    if (pkg.manual) {
        std::cout << "\nManual emulator: " << packageDisplayName(pkg) << "\n";
        if (!pkg.manualUrl.empty()) std::cout << "Download: " << pkg.manualUrl << "\n";
        if (!pkg.notes.empty()) std::cout << pkg.notes << "\n";
        std::cout << "Expected directory: " << dest << "\n";
        return 0;
    }

    if (dirHasFiles(dest)) {
        std::cout << packageDisplayName(pkg) << " already has files: " << dest << "\n";
        std::cout << "Reinstall? [y/N]: ";
        std::string answer;
        std::getline(std::cin, answer);
        if (trim(answer) != "y" && trim(answer) != "Y") return 0;
        std::error_code ec;
        fs::remove_all(dest, ec);
    }

    std::string archiveName = pkg.archive.empty() ? packageDisplayName(pkg) + ".zip" : expandPackageUrl(pkg.archive);
    archiveName = defaultArchiveNameForPackage(pkg, archiveName);
    std::string url = expandPackageUrl(!pkg.url.empty() ? pkg.url : pkg.urlTemplate);
    url = replaceAll(url, "{archive}", archiveName);
    if (url.empty()) url = defaultDownloadUrlForPackage(pkg, archiveName);
    std::string mirrorUrl = expandPackageUrl(!pkg.mirrorUrl.empty() ? pkg.mirrorUrl : pkg.mirrorUrlTemplate);
    mirrorUrl = replaceAll(mirrorUrl, "{archive}", archiveName);
    if (mirrorUrl.empty()) mirrorUrl = defaultMirrorUrlForPackage(pkg, archiveName);
    if (url.empty() && !mirrorUrl.empty()) {
        url = mirrorUrl;
        mirrorUrl.clear();
    }
    if (url.empty()) {
        std::cerr << "No download URL for " << packageDisplayName(pkg) << "\n";
        return 2;
    }
    fs::path archivePath = ws / "cache" / archiveName;
    fs::path extractDir = ws / "temp" / ("extract-" + pkg.id + "-" + pkg.version);
    std::error_code ec;
    fs::remove_all(extractDir, ec);

    std::cout << "\nDownloading " << packageDisplayName(pkg) << "\n";
    std::cout << "URL: " << url << "\n";
    if (!mirrorUrl.empty()) std::cout << "Mirror: " << mirrorUrl << "\n";
    if (int rc = downloadFileWithMirror(url, mirrorUrl, archivePath)) return rc;

    std::cout << "Extracting to " << dest << "\n";
    if (int rc = extractArchivePortable(archivePath, extractDir)) return rc;

    fs::path root = singleExtractedRoot(extractDir);
    if (root.empty() || !fs::exists(root)) {
        std::cerr << "Archive extraction produced no installable directory.\n";
        return 2;
    }
    fs::rename(root, dest, ec);
    if (ec) {
        ec.clear();
        fs::create_directories(dest, ec);
        if (!copyDirIfExists(root, dest)) {
            std::cerr << "Failed to install extracted emulator into " << dest << "\n";
            return 2;
        }
    }
    fs::remove_all(extractDir, ec);
    writeJsonFile(dest / "j2me.package.json", json{
        {"schema", "j2me.package/1"},
        {"id", pkg.id},
        {"type", pkg.type},
        {"version", pkg.version},
        {"source", pkg.source},
        {"url", url}
    });
    std::cout << "Installed: " << dest << "\n";
    return 0;
}

static fs::path resolveExecutablePath(const char *argv0) {
    fs::path exe = argv0 ? fs::path(argv0) : fs::path();
    if (exe.empty()) return {};
    if (exe.is_absolute() || exe.has_parent_path()) return absPath(exe);

    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv) return absPath(exe);
    std::stringstream ss(pathEnv);
    std::string dir;
#ifdef _WIN32
    const char sep = ';';
    const std::string exeName = exe.string().find(".exe") == std::string::npos ? exe.string() + ".exe" : exe.string();
#else
    const char sep = ':';
    const std::string exeName = exe.string();
#endif
    while (std::getline(ss, dir, sep)) {
        fs::path candidate = fs::path(dir) / exeName;
        if (fs::exists(candidate)) return absPath(candidate);
    }
    return absPath(exe);
}

static std::vector<int> parseVersionParts(std::string v) {
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(v.begin());
    for (char &c : v) {
        if (!std::isdigit(static_cast<unsigned char>(c))) c = '.';
    }
    std::vector<int> out;
    std::stringstream ss(v);
    std::string part;
    while (std::getline(ss, part, '.')) {
        if (!part.empty()) out.push_back(std::stoi(part));
    }
    while (out.size() < 3) out.push_back(0);
    return out;
}

static int compareVersions(const std::string &a, const std::string &b) {
    auto av = parseVersionParts(a);
    auto bv = parseVersionParts(b);
    size_t n = std::max(av.size(), bv.size());
    av.resize(n);
    bv.resize(n);
    for (size_t i = 0; i < n; ++i) {
        if (av[i] < bv[i]) return -1;
        if (av[i] > bv[i]) return 1;
    }
    return 0;
}

static std::string updateAssetNameForHost() {
    if (hostOsToken() == "linux") return "j2me-devspace-linux-x64.zip";
    if (hostOsToken() == "mac") return "j2me-devspace-macos-x64.zip";
    if (hostOsToken() == "windows") return "j2me-devspace-windows-x64.zip";
    return "";
}

static bool isConfigFile(const fs::path &p) {
    std::string ext = p.extension().string();
    return ext == ".json" || ext == ".yml" || ext == ".yaml";
}

static void backupConfigIfNeeded(const fs::path &dest) {
    if (!fs::exists(dest) || !isConfigFile(dest)) return;
    fs::path backup(dest.string() + ".bak");
    std::error_code ec;
    fs::copy_file(dest, backup, fs::copy_options::overwrite_existing, ec);
}

static int copyReleaseTree(const fs::path &from, const fs::path &toolHome, const fs::path &exePath) {
    std::error_code ec;
    for (auto &entry : fs::recursive_directory_iterator(from)) {
        auto rel = fs::relative(entry.path(), from, ec);
        if (ec || rel.empty()) continue;
        if (rel == fs::path("j2me") || rel == fs::path("j2me.exe")) continue;

        fs::path dest = toolHome / rel;
        if (entry.is_directory()) {
            fs::create_directories(dest, ec);
        } else if (entry.is_regular_file()) {
            fs::create_directories(dest.parent_path(), ec);
            backupConfigIfNeeded(dest);
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                std::cerr << "Failed to update " << dest << ": " << ec.message() << "\n";
                return 2;
            }
        }
    }

    fs::path newExe = from / (hostOsToken() == "windows" ? "j2me.exe" : "j2me");
    if (fs::exists(newExe) && !exePath.empty()) {
        fs::path exeBackup = exePath.string() + ".bak";
        fs::copy_file(exePath, exeBackup, fs::copy_options::overwrite_existing, ec);
        ec.clear();
        fs::copy_file(newExe, exePath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Failed to replace executable " << exePath << ": " << ec.message() << "\n";
            return 2;
        }
#ifndef _WIN32
        fs::permissions(exePath,
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add,
                        ec);
#endif
    }
    return 0;
}

static void updateInstalledCompletions(const fs::path &releaseRoot, const fs::path &toolHome) {
    if (toolHome.filename().string() != "j2me-devspace") return;

    fs::path shareDir = toolHome.parent_path();
    std::vector<std::pair<fs::path, fs::path>> files = {
        {releaseRoot / "completions" / "bash" / "j2me", shareDir / "bash-completion" / "completions" / "j2me"},
        {releaseRoot / "completions" / "zsh" / "_j2me", shareDir / "zsh" / "site-functions" / "_j2me"},
        {releaseRoot / "completions" / "fish" / "j2me.fish", shareDir / "fish" / "vendor_completions.d" / "j2me.fish"}
    };

    std::error_code ec;
    for (const auto &[src, dest] : files) {
        if (!fs::exists(src)) continue;
        fs::create_directories(dest.parent_path(), ec);
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Warning: failed to update completion file " << dest << ": " << ec.message() << "\n";
            ec.clear();
        }
    }
}

static int selfUpdate(const fs::path &toolHome, const AppMeta &meta, const char *argv0) {
    std::string api = "https://api.github.com/repos/" + meta.githubRepository + "/releases/latest";
    fs::path tempBase = fs::temp_directory_path() / ("j2me-devspace-update-" + std::to_string(std::time(nullptr)));
    fs::path releaseJson = tempBase / "release.json";
    fs::path archive = tempBase / "release.zip";
    fs::path extractDir = tempBase / "extract";
    fs::create_directories(tempBase);

    std::cout << "Checking GitHub Releases: " << api << "\n";
    if (int rc = downloadFile(api, releaseJson)) {
        std::cerr << "Could not read latest GitHub Release. The repository may not have published releases yet.\n";
        return rc;
    }

    auto release = readJsonFileStrict(releaseJson);
    if (!release || !release->is_object()) {
        std::cerr << "Failed to parse GitHub release metadata.\n";
        return 2;
    }
    std::string latest = jstr(*release, "tag_name");
    if (!latest.empty() && latest[0] == 'v') latest.erase(latest.begin());
    if (latest.empty()) {
        std::cerr << "GitHub release does not contain tag_name.\n";
        return 2;
    }

    std::cout << "Current version: " << meta.version << "\n";
    std::cout << "Latest version:  " << latest << "\n";
    if (compareVersions(meta.version, latest) >= 0) {
        std::cout << "Already up to date.\n";
        return 0;
    }

    std::string wanted = updateAssetNameForHost();
    std::string assetUrl;
    if (release->contains("assets") && release->at("assets").is_array()) {
        for (const auto &asset : release->at("assets")) {
            if (!asset.is_object()) continue;
            if (jstr(asset, "name") == wanted) {
                assetUrl = jstr(asset, "browser_download_url");
                break;
            }
        }
    }
    if (assetUrl.empty()) {
        std::cerr << "No release asset found for this host: " << wanted << "\n";
        return 2;
    }

    std::cout << "Download: " << assetUrl << "\n";
    std::cout << "Install update? [y/N]: ";
    std::string answer;
    std::getline(std::cin, answer);
    if (trim(answer) != "y" && trim(answer) != "Y") {
        std::cout << "Cancelled.\n";
        return 0;
    }

    if (int rc = downloadFile(assetUrl, archive)) return rc;
    if (int rc = extractArchivePortable(archive, extractDir)) return rc;
    fs::path root = singleExtractedRoot(extractDir);
    if (root.empty() || !fs::exists(root)) {
        std::cerr << "Release archive did not contain an installable directory.\n";
        return 2;
    }

    fs::path exePath = resolveExecutablePath(argv0);
    if (int rc = copyReleaseTree(root, toolHome, exePath)) return rc;
    updateInstalledCompletions(root, toolHome);
    std::cout << "Updated J2ME DevSpace to " << latest << ".\n";
    return 0;
}

static bool copyDirIfExists(const fs::path &from, const fs::path &to) {
    std::error_code ec;
    if (!fs::exists(from)) return false;
    fs::create_directories(to, ec);
    for (auto &entry : fs::recursive_directory_iterator(from)) {
        auto rel = fs::relative(entry.path(), from, ec);
        if (ec) continue;
        auto dest = to / rel;
        if (entry.is_directory()) {
            fs::create_directories(dest, ec);
        } else if (entry.is_regular_file()) {
            fs::create_directories(dest.parent_path(), ec);
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        }
    }
    return true;
}

static std::string normalizeTemplateFilename(std::string name) {
    size_t pos = name.rfind(".example.");
    if (pos != std::string::npos) {
        name.replace(pos, std::string(".example").size(), "");
    }
    pos = name.rfind(".template.");
    if (pos != std::string::npos) {
        name.replace(pos, std::string(".template").size(), "");
    }
    return name;
}

static bool copyTemplateDirIfExists(const fs::path &from, const fs::path &to) {
    std::error_code ec;
    if (!fs::exists(from)) return false;
    fs::create_directories(to, ec);
    for (auto &entry : fs::recursive_directory_iterator(from)) {
        auto rel = fs::relative(entry.path(), from, ec);
        if (ec) continue;

        fs::path normalizedRel;
        for (const auto &part : rel) {
            normalizedRel /= normalizeTemplateFilename(part.string());
        }
        auto dest = to / normalizedRel;
        if (entry.is_directory()) {
            fs::create_directories(dest, ec);
        } else if (entry.is_regular_file()) {
            fs::create_directories(dest.parent_path(), ec);
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        }
    }
    return true;
}

static std::map<std::string, std::string> readToolYml(const fs::path &p) {
    std::map<std::string, std::string> m;
    try {
        YAML::Node y = YAML::LoadFile(p.string());
        for (const auto &item : y) {
            if (item.first.IsScalar() && item.second.IsScalar()) {
                m[item.first.as<std::string>()] = item.second.as<std::string>();
            }
        }
    } catch (const std::exception &e) {
        m["_error"] = e.what();
    }
    return m;
}

static std::string safeIdent(std::string s) {
    if (s.empty()) return "app";
    for (char &c : s) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') c = '_';
    if (std::isdigit(static_cast<unsigned char>(s[0]))) s = "app_" + s;
    return s;
}

static std::string packageFromClass(const std::string &fqcn) {
    auto pos = fqcn.rfind('.');
    if (pos == std::string::npos) return "";
    return fqcn.substr(0, pos);
}

static fs::path packagePath(const fs::path &base, const std::string &pkg) {
    fs::path p = base;
    std::stringstream ss(pkg);
    std::string part;
    while (std::getline(ss, part, '.')) if (!part.empty()) p /= part;
    return p;
}

static std::vector<std::string> workspaceOwnedDirs() {
    return {"projects", "sdk", "emulators", "tools", "profiles", "packages", "cache", "temp", "docs"};
}

static json defaultWorkspaceJson(const AppMeta &meta) {
    return json{
        {"schema", "j2me.workspace/1"},
        {"name", "J2ME DevSpace Workspace"},
        {"created_at", nowIsoLocal()},
        {"created_by", {
            {"name", meta.name},
            {"version", meta.version},
            {"channel", meta.channel},
            {"codename", meta.codename}
        }},
        {"default_profile", meta.defaultProfile},
        {"dirs", {
            {"projects", "projects"},
            {"sdk", "sdk"},
            {"emulators", "emulators"},
            {"tools", "tools"},
            {"profiles", "profiles"},
            {"packages", "packages"},
            {"cache", "cache"},
            {"temp", "temp"},
            {"docs", "docs"}
        }}
    };
}

static json defaultProjectJson(const std::string &name, const std::string &defaultProfile) {
    std::string ident = safeIdent(name);
    return json{
        {"schema", "j2me.proj/1"},
        {"name", name},
        {"midlet_name", name},
        {"midlet_class", "com.ilyabot." + ident + ".MainMIDlet"},
        {"version", "0.1.0"},
        {"vendor", "IlyaBOT"},
        {"jar_name", name + ".jar"},
        {"icon", "/icons/icon32.png"},
        {"permissions", json::array({"javax.microedition.io.Connector.http"})},
        {"default_profile", defaultProfile}
    };
}

static json defaultProfileJson() {
    return json{
        {"schema", "j2me.profile/1"},
        {"name", "lg-gs290"},
        {"device_name", "LG GS290 Cookie Fresh"},
        {"screen_width", 240},
        {"screen_height", 400},
        {"touch", true},
        {"midp", "2.1"},
        {"cldc", "1.1"},
        {"heap_mb", 4},
        {"jar_size_limit_kb", 1024},
        {"emulator", "freej2me-plus"},
        {"wtk_device", "DefaultColorPhone"}
    };
}

static void cmdHelp(const AppMeta &meta) {
    std::cout << formatAppVersion(meta) << "\n"
              << "Usage:\n"
              << "  j2me --version | -V\n"
              << "  j2me --update\n"
              << "  j2me --workspace setup [DIR]\n"
              << "  j2me --workspace update [DIR]\n"
              << "  j2me --workspace remove [DIR] [--yes]\n"
              << "  j2me --workspace info\n"
              << "  j2me --project create <NAME> | -P create <NAME>\n"
              << "  j2me --project remove <NAME>\n"
              << "  j2me --project list\n"
              << "  j2me --project properties <NAME>\n"
              << "  j2me --sdk setup | --sdk list | -S list\n"
              << "  j2me --tools list | -T list\n"
              << "  j2me --tools <NAME> [args...] | -T <NAME> [args...]\n"
              << "  j2me --tools update [NAME]\n"
              << "  j2me --build [PROJECT] [--profile NAME] [--jdk VERSION] [--run-after-build]\n"
              << "  j2me --emulator setup\n"
              << "  j2me --run [PROJECT] [--profile NAME]\n\n"
              << "Most commands require a workspace. Create one with: j2me --workspace setup <DIR>\n";
}

static int cmdExampleUsage(const std::string &group) {
    std::cerr << "Example Usage:\n";
    if (group == "--workspace") {
        std::cerr << "  j2me --workspace setup [DIR]\n"
                  << "  j2me --workspace update [DIR]\n"
                  << "  j2me --workspace remove [DIR] [--yes]\n"
                  << "  j2me --workspace info\n";
        return 2;
    }
    if (group == "--project" || group == "-P") {
        std::cerr << "  j2me --project create <NAME>\n"
                  << "  j2me --project remove <NAME>\n"
                  << "  j2me --project list\n"
                  << "  j2me --project properties <NAME>\n"
                  << "  j2me -P create <NAME>\n";
        return 2;
    }
    if (group == "--sdk" || group == "-S") {
        std::cerr << "  j2me --sdk setup\n"
                  << "  j2me --sdk list\n"
                  << "  j2me -S list\n";
        return 2;
    }
    if (group == "--tools" || group == "-T") {
        std::cerr << "  j2me --tools list\n"
                  << "  j2me --tools update [NAME]\n"
                  << "  j2me --tools <NAME> [args...]\n"
                  << "  j2me -T <NAME> [args...]\n";
        return 2;
    }
    if (group == "--emulator") {
        std::cerr << "  j2me --emulator setup\n";
        return 2;
    }
    std::cerr << "  j2me --help\n";
    return 2;
}

static int workspaceSetup(const fs::path &toolHome, const AppMeta &meta, const fs::path &targetArg) {
    fs::path target = targetArg.empty() ? fs::current_path() : targetArg;
    target = absPath(target);
    fs::create_directories(target);

    fs::path marker = target / "j2me.workspace.json";
    if (fs::exists(marker)) {
        json old = readJsonFile(marker);
        if (jstr(old, "schema") == "j2me.workspace/1") {
            std::cout << "Workspace already exists: " << target << "\n";
        } else {
            std::cerr << "Refusing to overwrite foreign j2me.workspace.json in " << target << "\n";
            return 2;
        }
    } else {
        writeJsonFile(marker, defaultWorkspaceJson(meta));
    }

    for (const auto &d : workspaceOwnedDirs()) fs::create_directories(target / d);
    fs::create_directories(target / "sdk" / "jdk");
    fs::create_directories(target / "sdk" / "wtk");
    fs::create_directories(target / "sdk" / "ant");
    fs::create_directories(target / "sdk" / "proguard");
    fs::create_directories(target / "emulators" / "freej2me-plus");
    fs::create_directories(target / "emulators" / "microemulator");

    copyTemplateDirIfExists(toolHome / "templates" / "tools", target / "tools");
    copyTemplateDirIfExists(toolHome / "templates" / "profiles", target / "profiles");
    copyTemplateDirIfExists(toolHome / "templates" / "packages", target / "packages");
    fs::path demoProject = target / "projects" / "j2me_devspace-demo";
    if (!fs::exists(demoProject)) {
        copyTemplateDirIfExists(toolHome / "templates" / "project" / "app" / "demo", demoProject);
    }
    if (fs::exists(toolHome / "templates" / "README.workspace.md")) {
        fs::copy_file(toolHome / "templates" / "README.workspace.md", target / "README.workspace.md", fs::copy_options::overwrite_existing);
    }
    if (!fs::exists(target / "profiles" / "lg-gs290.json")) {
        writeJsonFile(target / "profiles" / "lg-gs290.json", defaultProfileJson());
    }

    std::cout << "Workspace ready: " << target << "\n";
    std::cout << "Demo project: projects/j2me_devspace-demo\n";
    std::cout << "Next:\n  cd " << target << "\n  j2me --workspace info\n";
    return 0;
}

static int workspaceUpdate(const fs::path &toolHome, const AppMeta &meta, const fs::path &targetArg) {
    fs::path target = targetArg.empty() ? findWorkspaceRoot() : absPath(targetArg);
    if (target.empty()) {
        std::cerr << "J2ME workspace not found. Run inside a workspace or pass workspace directory:\n"
                  << "  j2me --workspace update <directory>\n";
        return 2;
    }

    fs::path marker = target / "j2me.workspace.json";
    auto markerJson = readJsonFileStrict(marker, "j2me.workspace/1");
    if (!markerJson) {
        std::cerr << "Not a J2ME DevSpace workspace: " << target << "\n";
        return 2;
    }

    for (const auto &d : workspaceOwnedDirs()) fs::create_directories(target / d);
    fs::create_directories(target / "sdk" / "jdk");
    fs::create_directories(target / "sdk" / "wtk");
    fs::create_directories(target / "sdk" / "ant");
    fs::create_directories(target / "sdk" / "proguard");
    fs::create_directories(target / "emulators" / "freej2me-plus");
    fs::create_directories(target / "emulators" / "microemulator");

    copyTemplateDirIfExists(toolHome / "templates" / "tools", target / "tools");
    copyTemplateDirIfExists(toolHome / "templates" / "profiles", target / "profiles");
    copyTemplateDirIfExists(toolHome / "templates" / "packages", target / "packages");
    fs::remove(target / "packages" / "index.example.json");

    fs::path demoProject = target / "projects" / "j2me_devspace-demo";
    if (!fs::exists(demoProject)) {
        copyTemplateDirIfExists(toolHome / "templates" / "project" / "app" / "demo", demoProject);
    }
    if (fs::exists(toolHome / "templates" / "README.workspace.md")) {
        fs::copy_file(toolHome / "templates" / "README.workspace.md", target / "README.workspace.md", fs::copy_options::overwrite_existing);
    }
    if (!fs::exists(target / "profiles" / "lg-gs290.json")) {
        writeJsonFile(target / "profiles" / "lg-gs290.json", defaultProfileJson());
    }

    json updated = *markerJson;
    updated["updated_at"] = nowIsoLocal();
    updated["updated_by"] = {
        {"name", meta.name},
        {"version", meta.version},
        {"channel", meta.channel},
        {"codename", meta.codename}
    };
    writeJsonFile(marker, updated);

    std::vector<std::string> expected = workspaceOwnedDirs();
    std::sort(expected.begin(), expected.end());
    for (const auto &entry : fs::directory_iterator(target)) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        if (std::binary_search(expected.begin(), expected.end(), name)) continue;

        std::cout << "Extra directory found: " << name << "\n";
        std::cout << "Delete it? [y/N]: ";
        std::string answer;
        std::getline(std::cin, answer);
        if (trim(answer) == "y" || trim(answer) == "Y") {
            fs::remove_all(entry.path());
            std::cout << "Removed: " << name << "\n";
        } else {
            std::cout << "Kept: " << name << "\n";
        }
    }

    std::cout << "Workspace updated: " << target << "\n";
    return 0;
}

static int workspaceRemove(const fs::path &targetArg, bool yes) {
    fs::path target = targetArg.empty() ? fs::current_path() : targetArg;
    target = absPath(target);
    fs::path marker = target / "j2me.workspace.json";
    if (!fs::exists(marker) || jstr(readJsonFile(marker), "schema") != "j2me.workspace/1") {
        std::cerr << "Not a J2ME DevSpace workspace: " << target << "\n";
        std::cerr << "Refusing to delete anything without j2me.workspace.json marker.\n";
        return 2;
    }

    std::cout << "This will remove only J2ME DevSpace-owned content from:\n  " << target << "\n";
    std::cout << "Directories/files:\n";
    for (const auto &d : workspaceOwnedDirs()) std::cout << "  " << d << "/\n";
    std::cout << "  j2me.workspace.json\n  README.workspace.md\n";
    if (!yes) {
        std::cout << "Type YES to continue: ";
        std::string ok;
        std::getline(std::cin, ok);
        if (ok != "YES") { std::cout << "Cancelled.\n"; return 0; }
    }
    for (const auto &d : workspaceOwnedDirs()) fs::remove_all(target / d);
    fs::remove(target / "j2me.workspace.json");
    fs::remove(target / "README.workspace.md");
    std::cout << "Workspace content removed. Other files in the directory were left untouched.\n";
    return 0;
}

static size_t countProjectDirs(const fs::path &ws) {
    size_t count = 0;
    fs::path dir = ws / "projects";
    if (!fs::exists(dir)) return 0;
    for (auto &e : fs::directory_iterator(dir)) {
        if (e.is_directory() && fs::exists(e.path() / "j2me.proj.json")) ++count;
    }
    return count;
}

static void listSubdirsCompact(const fs::path &dir) {
    if (!fs::exists(dir)) { std::cout << "none"; return; }
    bool any = false;
    for (auto &e : fs::directory_iterator(dir)) {
        if (e.is_directory() || fs::is_symlink(e.path())) {
            if (any) std::cout << ", ";
            std::cout << e.path().filename().string();
            any = true;
        }
    }
    if (!any) std::cout << "none";
}

static int workspaceInfo(const fs::path &ws) {
    if (int rc = requireWorkspace(ws)) return rc;
    json marker = readJsonFile(ws / "j2me.workspace.json");
    std::cout << "Workspace: " << ws << "\n";
    std::cout << "Name: " << jstr(marker, "name", "J2ME DevSpace Workspace") << "\n";
    std::cout << "Created: " << jstr(marker, "created_at", "unknown") << "\n";
    std::cout << "Default profile: " << jstr(marker, "default_profile", "lg-gs290") << "\n";
    std::cout << "Projects: " << countProjectDirs(ws) << "\n";

    std::cout << "\nSDK packages:\n";
    std::cout << "  JDK: "; listSubdirsCompact(ws / "sdk" / "jdk"); std::cout << "\n";
    std::cout << "  WTK/JavaME: "; listSubdirsCompact(ws / "sdk" / "wtk"); std::cout << "\n";
    std::cout << "  Ant: "; listSubdirsCompact(ws / "sdk" / "ant"); std::cout << "\n";
    std::cout << "  ProGuard: "; listSubdirsCompact(ws / "sdk" / "proguard"); std::cout << "\n";

    std::cout << "\nEmulators:\n";
    std::cout << "  FreeJ2ME-Plus: " << (fs::exists(ws / "emulators" / "freej2me-plus" / "freej2me.jar") ? "configured" : "not configured") << "\n";
    std::cout << "  MicroEmulator: " << (fs::exists(ws / "emulators" / "microemulator" / "microemulator.jar") ? "configured" : "not configured") << "\n";

    std::cout << "\nProfile files: ";
    if (fs::exists(ws / "profiles")) {
        bool first = true;
        for (auto &e : fs::directory_iterator(ws / "profiles")) {
            if (e.path().extension() == ".json") {
                if (!first) std::cout << ", ";
                std::cout << e.path().stem().string();
                first = false;
            }
        }
        if (first) std::cout << "none";
        std::cout << "\n";
    } else {
        std::cout << "none\n";
    }

    std::cout << "\nTools:\n";
    fs::path tools = ws / "tools";
    if (!fs::exists(tools)) { std::cout << "  none\n"; return 0; }
    bool any = false;
    for (auto &e : fs::directory_iterator(tools)) {
        if (!e.is_directory()) continue;
        fs::path meta = e.path() / "j2me.tool.yml";
        if (!fs::exists(meta)) continue;
        auto y = readToolYml(meta);
        std::cout << "  " << e.path().filename().string();
        if (y.count("version")) std::cout << " " << y["version"];
        if (y.count("description")) std::cout << " - " << y["description"];
        if (y.count("_error")) std::cout << " - YAML error: " << y["_error"];
        std::cout << "\n";
        any = true;
    }
    if (!any) std::cout << "  none\n";
    return 0;
}

static fs::path currentProjectDir(const fs::path &ws) {
    fs::path p = absPath(fs::current_path());
    if (!startsWithPath(p, ws)) return {};
    for (;;) {
        if (fs::exists(p / "j2me.proj.json")) return p;
        if (p == ws || !p.has_parent_path() || p == p.parent_path()) break;
        p = p.parent_path();
    }
    return {};
}

static fs::path resolveProject(const fs::path &ws, const std::string &name) {
    if (!name.empty()) {
        if (!isSafeName(name)) return {};
        return ws / "projects" / name;
    }
    fs::path cur = currentProjectDir(ws);
    if (!cur.empty()) return cur;
    return {};
}

static int projectCreate(const fs::path &ws, const std::string &name) {
    if (rejectUnsafeName("project", name)) return 2;
    fs::path p = ws / "projects" / name;
    if (fs::exists(p)) { std::cerr << "Project already exists: " << p << "\n"; return 2; }
    json wsjson = readJsonFile(ws / "j2me.workspace.json");
    std::string defaultProfile = jstr(wsjson, "default_profile", "lg-gs290");
    if (!isSafeName(defaultProfile)) defaultProfile = "lg-gs290";
    std::string ident = safeIdent(name);
    fs::create_directories(p / "src" / "com" / "ilyabot" / ident);
    fs::create_directories(p / "res" / "icons");
    fs::create_directories(p / "build");
    fs::create_directories(p / "dist");

    writeJsonFile(p / "j2me.proj.json", defaultProjectJson(name, defaultProfile));
    fs::path globalProfile = ws / "profiles" / (defaultProfile + ".json");
    if (fs::exists(globalProfile)) {
        fs::copy_file(globalProfile, p / ("j2me.profile-" + defaultProfile + ".json"), fs::copy_options::overwrite_existing);
    } else {
        writeJsonFile(p / ("j2me.profile-" + defaultProfile + ".json"), defaultProfileJson());
    }

    std::string java = "package com.ilyabot." + ident + ";\n\n"
        "import javax.microedition.midlet.*;\n"
        "import javax.microedition.lcdui.*;\n\n"
        "public class MainMIDlet extends MIDlet implements CommandListener {\n"
        "    private Display display;\n"
        "    private final Command exitCommand = new Command(\"Exit\", Command.EXIT, 0);\n\n"
        "    public void startApp() {\n"
        "        display = Display.getDisplay(this);\n"
        "        Form form = new Form(\"" + javaStringEscape(name) + "\");\n"
        "        form.append(\"Hello from J2ME DevSpace. Default profile: " + javaStringEscape(defaultProfile) + ".\");\n"
        "        form.addCommand(exitCommand);\n"
        "        form.setCommandListener(this);\n"
        "        display.setCurrent(form);\n"
        "    }\n\n"
        "    public void pauseApp() {}\n\n"
        "    public void destroyApp(boolean unconditional) {}\n\n"
        "    public void commandAction(Command c, Displayable d) {\n"
        "        if (c == exitCommand) notifyDestroyed();\n"
        "    }\n"
        "}\n";
    writeAll(p / "src" / "com" / "ilyabot" / ident / "MainMIDlet.java", java);

    std::cout << "Created project: " << p << "\n";
    return 0;
}

static int projectList(const fs::path &ws) {
    fs::path dir = ws / "projects";
    if (!fs::exists(dir)) { std::cout << "No projects directory.\n"; return 0; }
    bool any = false;
    for (auto &e : fs::directory_iterator(dir)) {
        if (e.is_directory() && fs::exists(e.path() / "j2me.proj.json")) {
            std::cout << e.path().filename().string() << "\n";
            any = true;
        }
    }
    if (!any) std::cout << "No projects.\n";
    return 0;
}

static int projectRemove(const fs::path &ws, const std::string &name) {
    if (rejectUnsafeName("project", name)) return 2;
    fs::path p = ws / "projects" / name;
    if (!fs::exists(p / "j2me.proj.json")) { std::cerr << "Project not found: " << name << "\n"; return 2; }
    std::cout << "Remove " << p << " ? Type YES: ";
    std::string ok;
    std::getline(std::cin, ok);
    if (ok != "YES") { std::cout << "Cancelled.\n"; return 0; }
    fs::remove_all(p);
    std::cout << "Removed.\n";
    return 0;
}

static int projectProperties(const fs::path &ws, const std::string &name) {
    if (rejectUnsafeName("project", name)) return 2;
    fs::path p = ws / "projects" / name / "j2me.proj.json";
    if (!fs::exists(p)) { std::cerr << "Project not found: " << name << "\n"; return 2; }
    auto strict = readJsonFileStrict(p, "j2me.proj/1");
    if (!strict) { std::cerr << "Invalid project metadata: " << p << "\n"; return 2; }
    json j = *strict;
    std::map<std::string, std::string> vals;
    vals["name"] = jstr(j, "name", name);
    vals["midlet_name"] = jstr(j, "midlet_name", name);
    vals["midlet_class"] = jstr(j, "midlet_class", "com.ilyabot." + safeIdent(name) + ".MainMIDlet");
    vals["version"] = jstr(j, "version", "0.1.0");
    vals["vendor"] = jstr(j, "vendor", "IlyaBOT");
    vals["jar_name"] = jstr(j, "jar_name", name + ".jar");
    vals["icon"] = jstr(j, "icon", "/icons/icon32.png");
    vals["default_profile"] = jstr(j, "default_profile", "lg-gs290");

    for (auto &kv : vals) {
        std::cout << kv.first << " [" << kv.second << "]: ";
        std::string v;
        std::getline(std::cin, v);
        if (!trim(v).empty()) kv.second = trim(v);
    }

    auto perms = jsonArrayStrings(j, "permissions");
    std::cout << "permissions comma-separated [";
    for (size_t i=0;i<perms.size();++i) std::cout << (i?",":"") << perms[i];
    std::cout << "]: ";
    std::string ps;
    std::getline(std::cin, ps);
    if (!trim(ps).empty()) {
        perms.clear();
        std::stringstream ss(ps);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = trim(item);
            if (!item.empty()) perms.push_back(descriptorValue(item));
        }
    }

    if (!isSafeJavaQualifiedName(vals["midlet_class"])) {
        std::cerr << "Invalid midlet_class. Expected Java qualified class name, for example com.example.MainMIDlet.\n";
        return 2;
    }
    if (!isSafeFileName(vals["jar_name"], ".jar")) {
        std::cerr << "Invalid jar_name. Use a local .jar file name without path separators.\n";
        return 2;
    }
    if (!isSafeName(vals["default_profile"])) {
        std::cerr << "Invalid default_profile name.\n";
        return 2;
    }

    json out;
    out["schema"] = "j2me.proj/1";
    out["name"] = descriptorValue(vals["name"]);
    out["midlet_name"] = descriptorValue(vals["midlet_name"]);
    out["midlet_class"] = vals["midlet_class"];
    out["version"] = descriptorValue(vals["version"]);
    out["vendor"] = descriptorValue(vals["vendor"]);
    out["jar_name"] = vals["jar_name"];
    out["icon"] = descriptorValue(vals["icon"]);
    out["permissions"] = perms;
    out["default_profile"] = vals["default_profile"];
    writeJsonFile(p, out);
    std::cout << "Updated " << p << "\n";
    return 0;
}

static int sdkList(const fs::path &ws) {
    fs::path sdk = ws / "sdk";
    std::cout << "SDK directory: " << sdk << "\n";
    for (auto sub : {"jdk", "wtk", "ant", "proguard"}) {
        fs::path d = sdk / sub;
        std::cout << "\n[" << sub << "]\n";
        if (!fs::exists(d)) { std::cout << "  not installed\n"; continue; }
        bool any = false;
        for (auto &e: fs::directory_iterator(d)) {
            if (e.is_directory() || fs::is_symlink(e.path())) {
                std::cout << "  " << e.path().filename().string() << "\n";
                any = true;
            }
        }
        if (!any) std::cout << "  none\n";
    }
    return 0;
}

static int sdkSetup(const fs::path &ws) {
    fs::create_directories(ws / "sdk" / "jdk");
    fs::create_directories(ws / "sdk" / "wtk");
    fs::create_directories(ws / "sdk" / "ant");
    fs::create_directories(ws / "sdk" / "proguard");

    std::vector<PackageOption> all = readPackageIndex(ws);
    if (all.empty()) {
        std::cerr << "Package index not found or invalid: " << (ws / "packages" / "index.json") << "\n";
        return 2;
    }

    std::cout << "J2ME DevSpace SDK Setup Tool.\n"
              << "Select needed SDK and versions.\n"
              << "Input: Enter = default, number = select, 0 = skip, b = previous, q = cancel.\n\n";

    std::vector<std::string> steps = {"jdk", "ant", "proguard", "wtk"};
    std::map<std::string, PackageOption> selected;
    size_t step = 0;
    while (step < steps.size()) {
        std::string type = steps[step];
        auto opts = packagesByType(all, type);
        if (opts.empty()) { ++step; continue; }

        size_t def = 0;
        for (size_t i = 0; i < opts.size(); ++i) {
            if (opts[i].selectedByDefault) { def = i; break; }
        }

        std::cout << "\n";
        if (type == "jdk") std::cout << "Java Development Kit (JDK):\n";
        else if (type == "ant") std::cout << "Apache Ant:\n";
        else if (type == "proguard") std::cout << "ProGuard:\n";
        else if (type == "wtk") std::cout << "Sun Java Wireless Toolkit:\n";
        else std::cout << type << ":\n";

        std::cout << "  0) skip\n";
        for (size_t i = 0; i < opts.size(); ++i) {
            std::cout << "  " << (i + 1) << ") "
                      << (i == def ? "[*] " : "[ ] ")
                      << opts[i].id << "-" << opts[i].version;
            if (opts[i].manual) std::cout << " (manual)";
            std::cout << "\n";
            if (!opts[i].notes.empty()) std::cout << "      " << opts[i].notes << "\n";
        }
        std::cout << "Select: ";

        std::string input;
        std::getline(std::cin, input);
        input = trim(input);
        if (input == "q" || input == "Q") {
            std::cout << "Cancelled.\n";
            return 0;
        }
        if (input == "b" || input == "B") {
            if (step == 0) {
                std::cout << "Cancelled.\n";
                return 0;
            }
            selected.erase(steps[step - 1]);
            --step;
            continue;
        }
        if (input == "0") {
            selected.erase(type);
            ++step;
            continue;
        }

        size_t choice = def;
        if (!input.empty()) {
            try {
                int n = std::stoi(input);
                if (n < 1 || static_cast<size_t>(n) > opts.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }
                choice = static_cast<size_t>(n - 1);
            } catch (const std::exception &) {
                std::cout << "Invalid selection.\n";
                continue;
            }
        }
        selected[type] = opts[choice];
        ++step;
    }

    if (selected.empty()) {
        std::cout << "No SDK packages selected.\n";
        return 0;
    }

    std::cout << "\nInstallation plan:\n";
    for (const auto &type : steps) {
        auto it = selected.find(type);
        if (it == selected.end()) continue;
        const auto &p = it->second;
        std::cout << "  " << p.type << ": " << packageDisplayName(p);
        if (p.manual) std::cout << " (manual)";
        std::cout << "\n";
    }
    std::cout << "Continue? [y/N]: ";
    std::string yes;
    std::getline(std::cin, yes);
    yes = trim(yes);
    if (yes != "y" && yes != "Y") {
        std::cout << "Cancelled.\n";
        return 0;
    }

    for (const auto &type : steps) {
        auto it = selected.find(type);
        if (it == selected.end()) continue;
        if (int rc = installPackage(ws, it->second)) return rc;
    }
    std::cout << "\nSDK setup finished.\n";
    return 0;
}

static int toolsList(const fs::path &ws) {
    fs::path tools = ws / "tools";
    if (!fs::exists(tools)) { std::cout << "No tools directory.\n"; return 0; }
    bool any = false;
    for (auto &e : fs::directory_iterator(tools)) {
        if (!e.is_directory()) continue;
        fs::path meta = e.path() / "j2me.tool.yml";
        if (!fs::exists(meta)) continue;
        auto y = readToolYml(meta);
        std::cout << e.path().filename().string();
        if (y.count("version")) std::cout << " " << y["version"];
        if (y.count("description")) std::cout << " - " << y["description"];
        if (y.count("_error")) std::cout << " - YAML error: " << y["_error"];
        std::cout << "\n";
        any = true;
    }
    if (!any) std::cout << "No tools.\n";
    return 0;
}

static int toolRun(const fs::path &ws, const std::string &name, const std::vector<std::string> &extra) {
    if (name == "update") {
        std::cout << "Tool auto-update skeleton is present. Real updater must download GitHub release archive, backup old tool to tools/<name>/temp, then verify j2me.tool.yml version.\n";
        return 0;
    }
    if (rejectUnsafeName("tool", name)) return 2;
    fs::path main = ws / "tools" / name / "main.py";
    if (!fs::exists(main)) { std::cerr << "Tool not found in workspace: " << name << "\n"; return 2; }
#ifdef _WIN32
    std::string py = "python";
#else
    std::string py = "python3";
#endif
    std::string cmd = py + " " + shellQuote(main);
    for (const auto &a : extra) cmd += " " + shellQuote(a);
    return runCommand(cmd);
}

static std::vector<std::string> profilesInProject(const fs::path &p) {
    std::vector<std::string> out;
    if (!fs::exists(p)) return out;
    for (auto &e : fs::directory_iterator(p)) {
        std::string fn = e.path().filename().string();
        const std::string prefix = "j2me.profile-";
        const std::string suffix = ".json";
        if (fn.rfind(prefix, 0) == 0 && fn.size() > prefix.size() + suffix.size()) {
            out.push_back(fn.substr(prefix.size(), fn.size() - prefix.size() - suffix.size()));
        }
    }
    return out;
}

static std::string pickProfile(const fs::path &p, const std::string &requested) {
    if (!requested.empty()) return isSafeName(requested) ? requested : "";
    auto profs = profilesInProject(p);
    if (profs.empty()) return "";
    if (profs.size() == 1) return profs[0];
    std::cout << "Found multiple j2me project profiles:\n";
    for (auto &x : profs) std::cout << "  " << x << "\n";
    std::cout << "Profile: ";
    std::string x;
    std::getline(std::cin, x);
    x = trim(x);
    return isSafeName(x) ? x : "";
}

static std::string manifestFrom(const std::string &projText, const std::string &profileText, const AppMeta &meta, const std::string &buildDate) {
    json proj = parseJsonText(projText);
    json profile = parseJsonText(profileText);
    std::string name = descriptorValue(jstr(proj, "midlet_name", "MIDlet"));
    std::string cls = descriptorValue(jstr(proj, "midlet_class", "MainMIDlet"));
    std::string icon = descriptorValue(jstr(proj, "icon", "/icon.png"));
    std::string vendor = descriptorValue(jstr(proj, "vendor", "Unknown"));
    std::string version = descriptorValue(jstr(proj, "version", "0.1.0"));
    std::string midp = descriptorValue(jstr(profile, "midp", "2.1"));
    std::string cldc = descriptorValue(jstr(profile, "cldc", "1.1"));
    std::ostringstream out;
    std::string buildInfo = name + " built " + buildDate + " with " + meta.name + " " + meta.version + " " + meta.codename + ".";
    out << "MIDlet-1: " << name << "," << icon << "," << cls << "\n";
    out << "MIDlet-Name: " << name << "\n";
    out << "MIDlet-Version: " << version << "\n";
    out << "MIDlet-Vendor: " << vendor << "\n";
    out << "MicroEdition-Profile: MIDP-" << midp << "\n";
    out << "MicroEdition-Configuration: CLDC-" << cldc << "\n";
    out << "J2ME-DevSpace-Name: " << descriptorValue(meta.name) << "\n";
    out << "J2ME-DevSpace-Version: " << descriptorValue(meta.version) << "\n";
    out << "J2ME-DevSpace-Channel: " << descriptorValue(meta.channel) << "\n";
    out << "J2ME-DevSpace-Codename: " << descriptorValue(meta.codename) << "\n";
    out << "J2ME-Build-Date: " << buildDate << "\n";
    out << "J2ME-Build-Info: " << buildInfo << "\n";
    auto perms = jsonArrayStrings(proj, "permissions");
    if (!perms.empty()) {
        out << "MIDlet-Permissions: ";
        for (size_t i = 0; i < perms.size(); ++i) out << (i ? ", " : "") << descriptorValue(perms[i]);
        out << "\n";
    }
    return out.str();
}

static std::string jadFrom(const std::string &projText, const std::string &profileText, long jarSize, const AppMeta &meta, const std::string &buildDate) {
    json proj = parseJsonText(projText);
    std::string name = descriptorValue(jstr(proj, "midlet_name", "MIDlet"));
    std::string jar = descriptorValue(jstr(proj, "jar_name", name + ".jar"));
    std::ostringstream out;
    out << manifestFrom(projText, profileText, meta, buildDate);
    out << "MIDlet-Jar-URL: " << jar << "\n";
    out << "MIDlet-Jar-Size: " << jarSize << "\n";
    return out.str();
}

static std::string buildInfoJava(const std::string &pkg, const std::string &midletName, const AppMeta &meta, const std::string &buildDate) {
    std::ostringstream out;
    if (!pkg.empty()) out << "package " << pkg << ";\n\n";
    out << "public final class BuildInfo {\n";
    out << "    public static final String MIDLET_NAME = \"" << javaStringEscape(midletName) << "\";\n";
    out << "    public static final String SDK_NAME = \"" << javaStringEscape(meta.name) << "\";\n";
    out << "    public static final String SDK_VERSION = \"" << javaStringEscape(meta.version) << "\";\n";
    out << "    public static final String SDK_CHANNEL = \"" << javaStringEscape(meta.channel) << "\";\n";
    out << "    public static final String SDK_CODENAME = \"" << javaStringEscape(meta.codename) << "\";\n";
    out << "    public static final String BUILD_DATE = \"" << javaStringEscape(buildDate) << "\";\n";
    out << "    public static final String BUILD_INFO = \"" << javaStringEscape(midletName + " built " + buildDate + " with " + meta.name + " " + meta.version + " " + meta.codename + ".") << "\";\n";
    out << "    private BuildInfo() {}\n";
    out << "}\n";
    return out.str();
}

static std::string antBuildXml(const std::string &projectName, const std::string &jarName) {
    return std::string(R"XML(<project name=")XML") + projectName + R"XML(" default="dist">
    <property environment="env"/>
    <property name="src.dir" value="src"/>
    <property name="generated.src.dir" value="build/generated-src"/>
    <property name="res.dir" value="res"/>
    <property name="build.dir" value="build"/>
    <property name="classes.dir" value="build/classes"/>
    <property name="preverified.dir" value="build/preverified"/>
    <property name="dist.dir" value="dist"/>
    <property name="manifest.file" value="build/generated/MANIFEST.MF"/>
    <property name="jar.file" value="dist/)XML" + jarName + R"XML("/>
    <property name="wtk.home" value="${env.WTK_HOME}"/>

    <target name="clean">
        <delete dir="${classes.dir}"/>
        <delete dir="${preverified.dir}"/>
        <delete dir="${dist.dir}"/>
    </target>

    <target name="compile">
        <mkdir dir="${classes.dir}"/>
        <javac destdir="${classes.dir}" includeantruntime="false" source="1.3" target="1.1">
            <src path="${src.dir}"/>
            <src path="${generated.src.dir}"/>
            <classpath>
                <pathelement location="${wtk.home}/lib/cldcapi11.jar"/>
                <pathelement location="${wtk.home}/lib/midpapi21.jar"/>
            </classpath>
        </javac>
    </target>

    <target name="preverify" depends="compile">
        <mkdir dir="${preverified.dir}"/>
        <exec executable="${wtk.home}/bin/preverify" failonerror="true">
            <arg value="-classpath"/>
            <arg value="${wtk.home}/lib/cldcapi11.jar${path.separator}${wtk.home}/lib/midpapi21.jar${path.separator}${classes.dir}"/>
            <arg value="-d"/>
            <arg value="${preverified.dir}"/>
            <arg value="${classes.dir}"/>
        </exec>
    </target>

    <target name="dist" depends="preverify">
        <mkdir dir="${dist.dir}"/>
        <jar destfile="${jar.file}" manifest="${manifest.file}">
            <fileset dir="${preverified.dir}"/>
            <fileset dir="${res.dir}" erroronmissingdir="false"/>
        </jar>
    </target>
</project>
)XML";
}

static int runProject(const fs::path &ws, const std::string &projectArg, const std::string &profileArg);

static int buildProject(const fs::path &ws, const AppMeta &meta, const std::string &projectArg, const std::string &profileArg, const std::string &jdkArg, bool runAfter) {
    if (!projectArg.empty() && rejectUnsafeName("project", projectArg)) return 2;
    if (!profileArg.empty() && rejectUnsafeName("profile", profileArg)) return 2;
    if (!jdkArg.empty() && rejectUnsafeName("JDK", jdkArg)) return 2;
    fs::path p = resolveProject(ws, projectArg);
    if (p.empty() || !fs::exists(p / "j2me.proj.json")) {
        std::cerr << "No j2me project found. Run inside project directory or pass project name.\n";
        return 2;
    }
    std::string profileName = pickProfile(p, profileArg);
    if (profileName.empty() || !fs::exists(p / ("j2me.profile-" + profileName + ".json"))) {
        std::cerr << "No valid build profile found. Expected j2me.profile-<name>.json\n";
        return 2;
    }
    auto projStrict = readJsonFileStrict(p / "j2me.proj.json", "j2me.proj/1");
    auto profStrict = readJsonFileStrict(p / ("j2me.profile-" + profileName + ".json"), "j2me.profile/1");
    if (!projStrict) { std::cerr << "Invalid project metadata: " << (p / "j2me.proj.json") << "\n"; return 2; }
    if (!profStrict) { std::cerr << "Invalid profile metadata: " << (p / ("j2me.profile-" + profileName + ".json")) << "\n"; return 2; }
    std::string proj = projStrict->dump();
    std::string prof = profStrict->dump();
    json projJson = *projStrict;
    std::string name = jstr(projJson, "name", p.filename().string());
    std::string midletClass = jstr(projJson, "midlet_class", "MainMIDlet");
    std::string jar = jstr(projJson, "jar_name", name + ".jar");
    if (!isSafeJavaQualifiedName(midletClass)) {
        std::cerr << "Invalid midlet_class in project metadata: " << midletClass << "\n";
        return 2;
    }
    if (!isSafeFileName(jar, ".jar")) {
        std::cerr << "Invalid jar_name in project metadata: " << jar << "\n";
        return 2;
    }

    std::string buildDate = nowIsoLocal();
    fs::create_directories(p / "build" / "generated");
    fs::create_directories(p / "build" / "generated-src");
    fs::create_directories(p / "dist");
    writeAll(p / "build" / "generated" / "MANIFEST.MF", manifestFrom(proj, prof, meta, buildDate));
    std::string pkg = packageFromClass(midletClass);
    writeAll(packagePath(p / "build" / "generated-src", pkg) / "BuildInfo.java", buildInfoJava(pkg, name, meta, buildDate));
    writeAll(p / "build.xml", antBuildXml(xmlEscape(name), xmlEscape(jar)));

    fs::path ant = ws / "sdk" / "ant" / "current" / "bin" / (std::string("ant") +
#ifdef _WIN32
    ".bat"
#else
    ""
#endif
    );
    std::string antCmd = fs::exists(ant) ? shellQuote(ant) : "ant";

    fs::path wtk = ws / "sdk" / "wtk" / "current";
    std::string cmd;
#ifdef _WIN32
    cmd = "cd /d " + shellQuote(p) + " && ";
    if (!jdkArg.empty()) cmd += "set \"JAVA_HOME=" + (ws / "sdk" / "jdk" / jdkArg).string() + "\" && ";
    if (fs::exists(wtk)) cmd += "set \"WTK_HOME=" + wtk.string() + "\" && ";
    cmd += antCmd + " clean dist";
#else
    cmd = "cd " + shellQuote(p) + " && ";
    if (!jdkArg.empty()) cmd += "JAVA_HOME=" + shellQuote(ws / "sdk" / "jdk" / jdkArg) + " ";
    if (fs::exists(wtk)) cmd += "WTK_HOME=" + shellQuote(wtk) + " ";
    cmd += antCmd + " clean dist";
#endif
    int rc = runCommand(cmd);
    if (rc != 0) return rc;

    fs::path jarPath = p / "dist" / jar;
    long size = fs::exists(jarPath) ? static_cast<long>(fs::file_size(jarPath)) : 0;
    std::string jadName = fs::path(jar).replace_extension(".jad").filename().string();
    writeAll(p / "dist" / jadName, jadFrom(proj, prof, size, meta, buildDate));
    std::cout << "Built: " << jarPath << "\n";
    std::cout << "JAD:   " << (p / "dist" / jadName) << "\n";
    if (runAfter) return runProject(ws, name, profileName);
    return 0;
}

static int emulatorSetup(const fs::path &ws) {
    fs::create_directories(ws / "emulators" / "freej2me-plus");
    fs::create_directories(ws / "emulators" / "microemulator");

    std::vector<PackageOption> all = readPackageIndex(ws);
    auto opts = packagesByType(all, "emulator");
    if (opts.empty()) {
        std::cerr << "No emulator packages found in " << (ws / "packages" / "index.json") << "\n";
        return 2;
    }

    size_t def = 0;
    for (size_t i = 0; i < opts.size(); ++i) {
        if (opts[i].selectedByDefault) { def = i; break; }
    }

    std::cout << "J2ME DevSpace Emulator Setup Tool.\n"
              << "Default run order: FreeJ2ME-Plus AWT -> MicroEmulator -> WTK emulator.\n\n"
              << "Select emulator to install:\n"
              << "  0) skip\n";
    for (size_t i = 0; i < opts.size(); ++i) {
        std::cout << "  " << (i + 1) << ") "
                  << (i == def ? "[*] " : "[ ] ")
                  << opts[i].id << "-" << opts[i].version;
        if (opts[i].manual) std::cout << " (manual)";
        std::cout << "\n";
        if (!opts[i].notes.empty()) std::cout << "      " << opts[i].notes << "\n";
    }
    std::cout << "Select: ";

    std::string input;
    std::getline(std::cin, input);
    input = trim(input);
    if (input == "q" || input == "Q" || input == "0") {
        std::cout << "Cancelled.\n";
        return 0;
    }

    size_t choice = def;
    if (!input.empty()) {
        try {
            int n = std::stoi(input);
            if (n < 1 || static_cast<size_t>(n) > opts.size()) {
                std::cerr << "Invalid emulator selection.\n";
                return 2;
            }
            choice = static_cast<size_t>(n - 1);
        } catch (const std::exception &) {
            std::cerr << "Invalid emulator selection.\n";
            return 2;
        }
    }

    const auto &pkg = opts[choice];
    std::cout << "\nInstallation plan:\n"
              << "  emulator: " << packageDisplayName(pkg);
    if (pkg.manual) std::cout << " (manual)";
    std::cout << "\nContinue? [y/N]: ";
    std::string yes;
    std::getline(std::cin, yes);
    yes = trim(yes);
    if (yes != "y" && yes != "Y") {
        std::cout << "Cancelled.\n";
        return 0;
    }

    if (int rc = installEmulatorPackage(ws, pkg)) return rc;
    std::cout << "\nEmulator setup finished.\n";
    return 0;
}

static int runProject(const fs::path &ws, const std::string &projectArg, const std::string &profileArg) {
    if (!projectArg.empty() && rejectUnsafeName("project", projectArg)) return 2;
    if (!profileArg.empty() && rejectUnsafeName("profile", profileArg)) return 2;
    fs::path p = resolveProject(ws, projectArg);
    if (p.empty() || !fs::exists(p / "j2me.proj.json")) {
        std::cerr << "No j2me project found.\n";
        return 2;
    }
    std::string profileName = pickProfile(p, profileArg);
    (void)profileName;
    auto strict = readJsonFileStrict(p / "j2me.proj.json", "j2me.proj/1");
    if (!strict) { std::cerr << "Invalid project metadata: " << (p / "j2me.proj.json") << "\n"; return 2; }
    json proj = *strict;
    std::string name = jstr(proj, "name", p.filename().string());
    std::string jar = jstr(proj, "jar_name", name + ".jar");
    if (!isSafeFileName(jar, ".jar")) {
        std::cerr << "Invalid jar_name in project metadata: " << jar << "\n";
        return 2;
    }
    fs::path jarPath = p / "dist" / jar;
    fs::path jadPath = p / "dist" / fs::path(jar).replace_extension(".jad").filename();
    if (!fs::exists(jarPath)) { std::cerr << "JAR not found. Build first: j2me --build " << name << "\n"; return 2; }

    fs::path freej2me = ws / "emulators" / "freej2me-plus" / "freej2me.jar";
    if (fs::exists(freej2me)) {
        return runCommand("java -jar " + shellQuote(freej2me) + " " + shellQuote(jarPath));
    }
    fs::path micro = ws / "emulators" / "microemulator" / "microemulator.jar";
    if (fs::exists(micro)) {
        fs::path target = fs::exists(jadPath) ? jadPath : jarPath;
        return runCommand("java -jar " + shellQuote(micro) + " " + shellQuote(target));
    }
    fs::path wtkEmu = ws / "sdk" / "wtk" / "current" / "bin" / (std::string("emulator") +
#ifdef _WIN32
    ".exe"
#else
    ""
#endif
    );
    if (fs::exists(wtkEmu) && fs::exists(jadPath)) {
        return runCommand(shellQuote(wtkEmu) + " -Xdescriptor:" + shellQuote(jadPath));
    }
    std::cerr << "No emulator configured. Run: j2me --emulator setup\n";
    return 2;
}

int main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    fs::path toolHome = findToolHome(argc > 0 ? argv[0] : nullptr);
    AppMeta meta = loadAppMeta(toolHome);

    if (args.empty() || hasArg(args, "--help") || hasArg(args, "-h")) { cmdHelp(meta); return 0; }
    if (hasArg(args, "--version") || hasArg(args, "-V")) { std::cout << formatAppVersion(meta) << "\n"; return 0; }
    if (args[0] == "--update") return selfUpdate(toolHome, meta, argc > 0 ? argv[0] : nullptr);

    if (args.size() == 1 && (
        args[0] == "--workspace" ||
        args[0] == "--project" || args[0] == "-P" ||
        args[0] == "--sdk" || args[0] == "-S" ||
        args[0] == "--tools" || args[0] == "-T" ||
        args[0] == "--emulator")) {
        return cmdExampleUsage(args[0]);
    }

    if (args[0] == "--workspace" && args.size() >= 2) {
        std::string action = args[1];
        if (action == "setup") {
            fs::path dir;
            if (args.size() >= 3 && args[2].rfind("--", 0) != 0) dir = args[2];
            return workspaceSetup(toolHome, meta, dir);
        }
        if (action == "update") {
            fs::path dir;
            if (args.size() >= 3 && args[2].rfind("--", 0) != 0) dir = args[2];
            return workspaceUpdate(toolHome, meta, dir);
        }
        if (action == "remove") {
            fs::path dir;
            if (args.size() >= 3 && args[2].rfind("--", 0) != 0) dir = args[2];
            return workspaceRemove(dir, hasArg(args, "--yes"));
        }
        if (action == "info") return workspaceInfo(findWorkspaceRoot());
        return cmdExampleUsage(args[0]);
    }

    fs::path ws = findWorkspaceRoot();
    if (int rc = requireWorkspace(ws)) return rc;

    if ((args[0] == "--project" || args[0] == "-P") && args.size() >= 2) {
        std::string action = args[1];
        if (action == "create" && args.size() >= 3) return projectCreate(ws, args[2]);
        if (action == "remove" && args.size() >= 3) return projectRemove(ws, args[2]);
        if (action == "list") return projectList(ws);
        if (action == "properties" && args.size() >= 3) return projectProperties(ws, args[2]);
        return cmdExampleUsage(args[0]);
    }

    if ((args[0] == "--sdk" || args[0] == "-S") && args.size() >= 2) {
        if (args[1] == "setup") return sdkSetup(ws);
        if (args[1] == "list") return sdkList(ws);
        return cmdExampleUsage(args[0]);
    }

    if ((args[0] == "--tools" || args[0] == "-T") && args.size() >= 2) {
        if (args[1] == "list") return toolsList(ws);
        std::vector<std::string> extra;
        for (size_t i = 2; i < args.size(); ++i) extra.push_back(args[i]);
        return toolRun(ws, args[1], extra);
    }

    if (args[0] == "--build") {
        std::string projectArg;
        if (args.size() >= 2 && args[1].rfind("--", 0) != 0) projectArg = args[1];
        std::string profile = argAfter(args, "--profile", "");
        std::string jdk = argAfter(args, "--jdk", "");
        bool runAfter = hasArg(args, "--run-after-build");
        return buildProject(ws, meta, projectArg, profile, jdk, runAfter);
    }

    if (args[0] == "--emulator" && args.size() >= 2) {
        if (args[1] == "setup") return emulatorSetup(ws);
        return cmdExampleUsage(args[0]);
    }

    if (args[0] == "--run") {
        std::string projectArg;
        if (args.size() >= 2 && args[1].rfind("--", 0) != 0) projectArg = args[1];
        return runProject(ws, projectArg, argAfter(args, "--profile", ""));
    }

    std::cerr << "Unknown command.\n";
    cmdHelp(meta);
    return 2;
}
