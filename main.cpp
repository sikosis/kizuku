#include <cerrno>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

#ifndef KIZUKU_VERSION
#define KIZUKU_VERSION "0.1"
#endif

constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kAmber = "\033[38;2;232;183;106m";
constexpr const char* kPurple = "\033[38;2;111;58;99m";
constexpr const char* kName = "Kizuku";
constexpr const char* kVersion = KIZUKU_VERSION;

bool gUseHum = false;

struct Step {
    std::string heading;
    std::string question;
    std::string colour;
    std::vector<std::string> commands;
};

struct Theme {
    const char* name;
    const char* updateCode;
    const char* clean;
    const char* build;
    const char* test;
    const char* run;
};

// Built-in palettes are data-only so more themes can be added without changing
// how build steps are assembled.
constexpr std::array<Theme, 3> kThemes = {{
    {
        "Kizuku",
        "#f2cc60",
        "#e8b76a",
        "#d98a4e",
        "#d98a3e",
        "#6f3a63",
    },
    {
        "Coast",
        "#73d2de",
        "#52b2cf",
        "#2e86ab",
        "#33658a",
        "#7b6dba",
    },
    {
        "Sakura",
        "#ffb3c6",
        "#ff8fab",
        "#fb6f92",
        "#c77dff",
        "#7b2cbf",
    },
}};

struct BuildFile {
    std::string title;
    std::string titleColour;
    std::string directory;
    fs::path output;
    std::vector<Step> steps;
};

struct GitFile {
    std::string projectName;
    fs::path output;
    std::string headingColour;
    std::string informationColour;
    std::string errorColour;
};

struct WizardResult {
    BuildFile buildFile;
    std::optional<GitFile> gitFile;
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string randomColour() {
    static std::mt19937 generator(std::random_device{}());
    static std::uniform_int_distribution<int> distribution(1, 255);
    return std::to_string(distribution(generator));
}

std::string shellSingleQuoted(const std::string& value);

std::string shellDoubleQuoted(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char character : value) {
        if (character == '"' || character == '\\' || character == '$' || character == '`') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

bool executableInPath(const std::string& executable) {
    const char* pathValue = std::getenv("PATH");
    if (pathValue == nullptr) {
        return false;
    }

    std::string paths(pathValue);
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(':', start);
        const auto directory = paths.substr(start, end == std::string::npos ? end : end - start);
        const fs::path candidate = (directory.empty() ? fs::path(".") : fs::path(directory)) / executable;
        if (::access(candidate.c_str(), X_OK) == 0) {
            return true;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

std::string humCommand(const std::vector<std::string>& arguments) {
    std::string command = "hum";
    for (const auto& argument : arguments) {
        command += " " + shellSingleQuoted(argument);
    }
    return command;
}

std::optional<std::string> captureHum(const std::vector<std::string>& arguments) {
    FILE* pipe = ::popen(humCommand(arguments).c_str(), "r");
    if (pipe == nullptr) {
        return std::nullopt;
    }

    std::string output;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int status = ::pclose(pipe);
    if (status != 0) {
        return std::nullopt;
    }
    return trim(output);
}

std::string prompt(const std::string& label, const std::string& fallback = {}) {
    if (gUseHum) {
        std::vector<std::string> arguments = {"input", "--prompt", label + ": ", "--no-show-help"};
        if (!fallback.empty()) {
            arguments.insert(arguments.end(), {"--value", fallback});
        }
        if (const auto answer = captureHum(arguments)) {
            return answer->empty() ? fallback : *answer;
        }
    }

    std::cout << kBold << label << kReset;
    if (!fallback.empty()) {
        std::cout << " [" << fallback << "]";
    }
    std::cout << ": " << std::flush;

    std::string answer;
    if (!std::getline(std::cin, answer)) {
        throw std::runtime_error("input was closed");
    }
    answer = trim(answer);
    return answer.empty() ? fallback : answer;
}

bool confirm(const std::string& label, bool fallback = true) {
    if (gUseHum) {
        const int status = std::system(humCommand(
            {"confirm", "--default", fallback ? "yes" : "no", "--no-show-help", label}).c_str());
        if (status == 0) {
            return true;
        }
        if (status != -1) {
            return false;
        }
    }

    while (true) {
        const auto answer = prompt(label + (fallback ? " (Y/n)" : " (y/N)"));
        if (answer.empty()) {
            return fallback;
        }
        if (answer == "y" || answer == "Y" || answer == "yes" || answer == "YES") {
            return true;
        }
        if (answer == "n" || answer == "N" || answer == "no" || answer == "NO") {
            return false;
        }
        std::cout << "Please answer y or n.\n";
    }
}

void showStyled(const std::string& text, const std::string& colour, bool bordered = false) {
    if (gUseHum) {
        std::vector<std::string> arguments = {"style", "--bold", "--foreground", colour};
        if (bordered) {
            arguments.insert(arguments.end(), {"--border", "rounded", "--padding", "1 2"});
        }
        arguments.push_back(text);
        if (std::system(humCommand(arguments).c_str()) == 0) {
            return;
        }
    }
    std::cout << (bordered ? kPurple : kAmber) << kBold << text << kReset << "\n";
}

std::string projectSlug(const std::string& projectName) {
    std::string slug;
    bool previousDash = false;
    for (const unsigned char character : projectName) {
        if (std::isalnum(character)) {
            slug.push_back(static_cast<char>(std::tolower(character)));
            previousDash = false;
        } else if (!slug.empty() && !previousDash) {
            slug.push_back('-');
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "project" : slug;
}

std::string selectProjectDirectory(const std::string& projectName) {
    const std::string suggested = "/boot/home/" + projectSlug(projectName) + "/";
    if (gUseHum) {
        const fs::path start = fs::exists("/boot/home") ? fs::path("/boot/home") : fs::current_path();
        if (const auto selection = captureHum({"file", "--no-file", "--directory",
                                               "--header", "Select the project directory",
                                               start.string()})) {
            if (!selection->empty()) {
                return *selection;
            }
        }
    }
    return prompt("Project directory", suggested);
}

const Theme& selectTheme() {
    if (gUseHum) {
        std::vector<std::string> arguments = {
            "choose", "--header", "Choose a colour theme", "--selected", kThemes.front().name,
        };
        for (const auto& theme : kThemes) {
            arguments.push_back(theme.name);
        }
        if (const auto selection = captureHum(arguments)) {
            for (const auto& theme : kThemes) {
                if (*selection == theme.name) {
                    return theme;
                }
            }
        }
    }

    while (true) {
        const auto selection = prompt("Theme (Kizuku, Coast, Sakura)", kThemes.front().name);
        std::string normalised;
        normalised.reserve(selection.size());
        for (const unsigned char character : selection) {
            normalised.push_back(static_cast<char>(std::tolower(character)));
        }
        for (const auto& theme : kThemes) {
            std::string themeName(theme.name);
            for (char& character : themeName) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            if (normalised == themeName) {
                return theme;
            }
        }
        std::cout << "Choose Kizuku, Coast, or Sakura.\n";
    }
}

std::string shellSingleQuoted(const std::string& value) {
    std::string escaped = "'";
    for (const char character : value) {
        if (character == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(character);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

void writeStep(std::ostream& output, const Step& step, std::size_t number) {
    const std::string colour = !step.colour.empty() && step.colour.front() == '#'
        ? shellDoubleQuoted(step.colour) : step.colour;
    output << "hum style --foreground " << colour
           << " --bold " << shellDoubleQuoted("==> Step " + std::to_string(number) + ": " + step.heading)
           << "\n";
    output << "if hum confirm " << shellDoubleQuoted(step.question) << "; then\n";
    for (const auto& command : step.commands) {
        output << "    " << command << "\n";
    }
    output << "fi\n\n";
}

bool writeBuildFile(const BuildFile& buildFile, std::string& error) {
    if (buildFile.output.has_parent_path()) {
        std::error_code directoryError;
        fs::create_directories(buildFile.output.parent_path(), directoryError);
        if (directoryError) {
            error = "cannot create output directory: " + directoryError.message();
            return false;
        }
    }

    std::ofstream output(buildFile.output);
    if (!output) {
        error = "cannot open " + buildFile.output.string() + " for writing";
        return false;
    }

    output << "#!/bin/bash\n\n";
    output << "clear\n";
    output << "cd " << shellSingleQuoted(buildFile.directory) << " || exit 1\n\n";
    output << "if ! command -v hum >/dev/null 2>&1; then\n";
    output << "    echo \"This build file requires hum.\" >&2\n";
    output << "    exit 127\n";
    output << "fi\n\n";
    output << "hum style --border rounded --foreground " << buildFile.titleColour
           << " --padding \"1 2\" "
              "--margin \"1 0\" --bold "
           << shellDoubleQuoted(buildFile.title) << "\n\n";
    output << "if hum confirm \"Continue?\"; then\n\n";
    for (std::size_t index = 0; index < buildFile.steps.size(); ++index) {
        writeStep(output, buildFile.steps[index], index + 1);
    }
    output << "fi\n";
    output.close();

    if (!output) {
        error = "failed while writing " + buildFile.output.string();
        return false;
    }

    if (::chmod(buildFile.output.c_str(), 0755) != 0) {
        error = "file was written, but could not be made executable (errno " +
                std::to_string(errno) + ")";
        return false;
    }
    return true;
}

bool writeGitFile(const GitFile& gitFile, std::string& error) {
    if (gitFile.output.has_parent_path()) {
        std::error_code directoryError;
        fs::create_directories(gitFile.output.parent_path(), directoryError);
        if (directoryError) {
            error = "cannot create Git script directory: " + directoryError.message();
            return false;
        }
    }

    std::ofstream output(gitFile.output);
    if (!output) {
        error = "cannot open " + gitFile.output.string() + " for writing";
        return false;
    }

    output << "#!/bin/sh\n\n";
    output << "set -eu\n\n";
    output << "script_directory=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n\n";
    output << "if ! command -v git >/dev/null 2>&1; then\n";
    output << "    echo \"error: git is not installed or is not in PATH\" >&2\n";
    output << "    exit 1\n";
    output << "fi\n\n";
    output << "if ! command -v hum >/dev/null 2>&1; then\n";
    output << "    echo \"error: hum is not installed or is not in PATH\" >&2\n";
    output << "    echo \"Install Hum, then run this script again.\" >&2\n";
    output << "    exit 1\n";
    output << "fi\n\n";
    output << "if [ \"$(git -C \"$script_directory\" rev-parse --is-inside-work-tree 2>/dev/null)\" != \"true\" ]; then\n";
    output << "    echo \"error: $script_directory is not a Git working tree\" >&2\n";
    output << "    exit 1\n";
    output << "fi\n\n";
    output << "hum style --bold --foreground " << gitFile.headingColour << " "
           << shellDoubleQuoted("Updating " + gitFile.projectName + "'s Git repository") << "\n\n";
    output << "# Stage every addition, modification, and deletion in this repository.\n";
    output << "git -C \"$script_directory\" add -A\n\n";
    output << "if git -C \"$script_directory\" diff --cached --quiet; then\n";
    output << "    hum style --foreground " << gitFile.informationColour
           << " \"There are no changes to commit.\"\n";
    output << "    exit 0\n";
    output << "fi\n\n";
    output << "hum style --bold \"Staged changes\"\n";
    output << "git -C \"$script_directory\" status --short\n";
    output << "printf '\\n'\n\n";
    output << "commit_title=$(hum input \\\n";
    output << "    --prompt \"Commit title: \" \\\n";
    output << "    --placeholder \"Briefly describe the change\")\n\n";
    output << "if [ -z \"$commit_title\" ]; then\n";
    output << "    hum style --foreground " << gitFile.errorColour
           << " \"A commit title is required. The changes remain staged.\"\n";
    output << "    exit 1\n";
    output << "fi\n\n";
    output << "commit_details=$(hum write \\\n";
    output << "    --header \"Commit details (optional; Ctrl+D when finished)\" \\\n";
    output << "    --placeholder \"Explain what changed and why\")\n\n";
    output << "if [ -n \"$commit_details\" ]; then\n";
    output << "    git -C \"$script_directory\" commit -m \"$commit_title\" -m \"$commit_details\"\n";
    output << "else\n";
    output << "    git -C \"$script_directory\" commit -m \"$commit_title\"\n";
    output << "fi\n\n";
    output << "branch=$(git -C \"$script_directory\" branch --show-current)\n";
    output << "if [ -z \"$branch\" ]; then\n";
    output << "    hum style --foreground " << gitFile.informationColour
           << " \"Commit created in detached HEAD state; it was not pushed.\"\n";
    output << "    exit 0\n";
    output << "fi\n\n";
    output << "if ! hum confirm --default no \"Push '$branch' now?\"; then\n";
    output << "    hum style --foreground " << gitFile.informationColour
           << " \"Commit created locally and not pushed.\"\n";
    output << "    exit 0\n";
    output << "fi\n\n";
    output << "if git -C \"$script_directory\" rev-parse --abbrev-ref '@{upstream}' >/dev/null 2>&1; then\n";
    output << "    git -C \"$script_directory\" push\n";
    output << "elif git -C \"$script_directory\" remote get-url origin >/dev/null 2>&1; then\n";
    output << "    git -C \"$script_directory\" push --set-upstream origin \"$branch\"\n";
    output << "else\n";
    output << "    hum style --foreground " << gitFile.errorColour
           << " \"No upstream branch or 'origin' remote is configured.\"\n";
    output << "    exit 1\n";
    output << "fi\n";
    output.close();

    if (!output) {
        error = "failed while writing " + gitFile.output.string();
        return false;
    }
    if (::chmod(gitFile.output.c_str(), 0755) != 0) {
        error = "Git script was written, but could not be made executable (errno " +
                std::to_string(errno) + ")";
        return false;
    }
    return true;
}

WizardResult runWizard() {
    std::cout << "\n";
    showStyled(std::string(kName) + " v" + kVersion + " — Haiku command-line helper",
               randomColour(), true);
    std::cout << "\n";

    const auto projectName = prompt("Project name", "My Project");
    const auto version = prompt("Builder version", "1.0");
    const auto directory = selectProjectDirectory(projectName);
    const Theme& theme = selectTheme();

    std::vector<Step> steps;
    if (confirm("Include an Update Code step (git pull)?")) {
        steps.push_back({"Update Code", "Git Pull?", theme.updateCode, {"git pull"}});
    }
    if (confirm("Include a clean step?")) {
        steps.push_back({"Make Clean", "Make Clean?", theme.clean,
                         {prompt("Clean command", "make clean")}});
    }
    if (confirm("Include a build step?")) {
        steps.push_back({"Make", "Make?", theme.build, {prompt("Build command", "make")}});
    }
    if (confirm("Include a test step?")) {
        steps.push_back({"Make Test", "Make Tests?", theme.test,
                         {prompt("Test command", "make test")}});
    }
    if (confirm("Include a run step?")) {
        const auto command = prompt("Run command", "./my-project");
        steps.push_back({"Run / Test", "Run?", theme.run, {command}});
    }
    const fs::path output = steps.empty() ? fs::path{} : fs::path(prompt("Output build file", "build.sh"));

    std::optional<GitFile> gitFile;
    if (confirm("Create a Git commit-and-push script?", false)) {
        gitFile = GitFile{projectName, prompt("Output Git script", "git.sh"),
                          randomColour(), randomColour(), randomColour()};
    }

    return {{projectName + " Builder v" + version, randomColour(), directory, output, steps}, gitFile};
}

void printUsage(const char* executable) {
    std::cout << kName << " v" << kVersion << "\n"
              << "Haiku command-line helper for generating interactive build and Git scripts.\n\n"
              << "Usage: " << executable << " [OPTION]\n\n"
              << "Options:\n"
              << "  -h, --help       Show this help\n"
              << "  -v, --version    Show the Kizuku version\n\n"
              << "When Hum is available, Kizuku uses it for prompts, styling, confirmation,\n"
              << "and project-directory selection. Plain terminal prompts are used otherwise.\n"
              << "Built-in themes: Kizuku (default), Coast, and Sakura.\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        gUseHum = ::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0 &&
                  executableInPath("hum");
        WizardResult result;
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printUsage(argv[0]);
            return 0;
        } else if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
            std::cout << kName << " v" << kVersion << "\n";
            return 0;
        } else if (argc != 1) {
            printUsage(argv[0]);
            return 2;
        } else {
            result = runWizard();
        }

        if (result.buildFile.steps.empty() && !result.gitFile) {
            std::cerr << "No build steps or Git script selected; nothing was written.\n";
            return 1;
        }

        std::string error;
        if (!result.buildFile.steps.empty() && result.gitFile &&
            fs::absolute(result.buildFile.output) == fs::absolute(result.gitFile->output)) {
            std::cerr << "kizuku: the build file and Git script cannot use the same path\n";
            return 1;
        }

        if (!result.buildFile.steps.empty()) {
            if (!writeBuildFile(result.buildFile, error)) {
                std::cerr << "kizuku: " << error << "\n";
                return 1;
            }
            std::cout << "\n";
            showStyled("Created " + fs::absolute(result.buildFile.output).string(), randomColour());
        }

        if (result.gitFile) {
            if (!writeGitFile(*result.gitFile, error)) {
                std::cerr << "kizuku: " << error << "\n";
                return 1;
            }
            showStyled("Created " + fs::absolute(result.gitFile->output).string(), randomColour());
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "kizuku: " << exception.what() << "\n";
        return 1;
    }
}
