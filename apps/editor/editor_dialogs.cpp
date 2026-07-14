// Native file dialogs + process spawning. Isolated in its own TU:
// <windows.h> redefines several raylib symbols (CloseWindow, DrawText,
// Rectangle, ...), so no raylib here.

#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace figoedit {

#ifdef _WIN32

static constexpr const char* kFilter =
    "Figma files (*.fig;*.json)\0*.fig;*.json\0"
    "Figma binary (*.fig)\0*.fig\0"
    "JSON (*.json)\0*.json\0"
    "All files (*.*)\0*.*\0";

std::string showOpenFileDialog() {
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open Figma file";
    // NOCHANGEDIR: the dialog must not change the process cwd (relative
    // asset paths would silently break).
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return {};
    return path;
}

std::string showSaveFileDialog(const std::string& suggested) {
    char path[MAX_PATH] = {};
    suggested.copy(path, sizeof(path) - 1);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = "JSON (*.json)\0*.json\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save as";
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&ofn)) return {};
    return path;
}

#elif defined(__APPLE__)

// macOS: no AppKit linkage in this TU — drive the native panels through
// osascript (a real modal Finder chooser). Blocks like GetOpenFileName does.
#include <cstdio>

static std::string runOsascript(const std::string& script) {
    const std::string cmd = "osascript -e '" + script + "' 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return {};
    std::string out;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);  // nonzero (user cancelled) just yields empty out
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

std::string showOpenFileDialog() {
    // Filter to .fig / .json; cancel -> osascript errors -> empty string.
    return runOsascript(
        "POSIX path of (choose file with prompt \"Open Figma file\" "
        "of type {\"fig\", \"json\"})");
}

std::string showSaveFileDialog(const std::string& suggested) {
    std::string base = suggested;
    if (auto pos = base.find_last_of('/'); pos != std::string::npos)
        base = base.substr(pos + 1);
    if (base.empty()) base = "design.json";
    for (char& c : base) if (c == '\'' || c == '"') c = '_';  // keep AppleScript sane
    return runOsascript(
        "POSIX path of (choose file name with prompt \"Save as\" "
        "default name \"" + base + "\")");
}

#else

std::string showOpenFileDialog() { return {}; }
std::string showSaveFileDialog(const std::string&) { return {}; }

#endif

// ---- fire-and-forget child process (Play button) ----------------------------

#ifdef _WIN32

bool spawnDetached(const std::string& exe, const std::vector<std::string>& args) {
    std::string cmd = "\"" + exe + "\"";
    for (const auto& a : args) cmd += " \"" + a + "\"";
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    // New process group: the child shares our console (its logs stay visible)
    // but does not die with a Ctrl+C aimed at the editor.
    if (!CreateProcessA(exe.c_str(), buf.data(), nullptr, nullptr, FALSE,
                        CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

#else

bool spawnDetached(const std::string& exe, const std::vector<std::string>& args) {
    // Double fork so the grandchild is reparented to init — no zombie to reap
    // and it outlives the editor.
    const pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (fork() == 0) {
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(exe.c_str()));
            for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
            argv.push_back(nullptr);
            execv(exe.c_str(), argv.data());
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
}

#endif

}  // namespace figoedit
