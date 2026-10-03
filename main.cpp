#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>

namespace fs = std::filesystem;

namespace {

constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kAmber = "\033[38;2;232;183;106m";
constexpr const char* kPurple = "\033[38;2;111;58;99m";

struct Step {
    std::string heading;
    std::string question;
    std::string colour;
    std::vector<std::string> commands;
};

struct BuildFile {
    std::string title;
    std::string directory;
    fs::path output;
    std::vector<Step> steps;
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string prompt(const std::string& label, const std::string& fallback = {}) {
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
    output << "hum style --foreground " << shellDoubleQuoted(step.colour)
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
    output << "hum style --border rounded --foreground 41 --padding \"1 2\" "
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

BuildFile runWizard() {
    std::cout << "\n" << kPurple << kBold << "  Kizuku — Haiku build-file generator  " << kReset << "\n\n";

    const auto projectName = prompt("Project name", "My Project");
    const auto version = prompt("Builder version", "1.0");
    const auto directory = prompt("Project directory", "/boot/home/my-project/");
    const auto output = prompt("Output build file", "build.sh");

    std::vector<Step> steps;
    if (confirm("Include a clean step?")) {
        steps.push_back({"Make Clean", "Make Clean?", "#e8b76a", {prompt("Clean command", "make clean")}});
    }
    if (confirm("Include a build step?")) {
        steps.push_back({"Make", "Make?", "#d98a4e", {prompt("Build command", "make")}});
    }
    if (confirm("Include a test step?")) {
        steps.push_back({"Make Test", "Make Tests?", "#d98a3e", {prompt("Test command", "make test")}});
    }
    if (confirm("Include a run step?")) {
        const auto command = prompt("Run command", "./my-project");
        steps.push_back({"Run / Test", "Run?", "#6f3a63", {command}});
    }

    return {projectName + " Builder v" + version, directory, output, steps};
}

void printUsage(const char* executable) {
    std::cout << "Usage: " << executable << "\n\n"
              << "Kizuku starts an interactive build-file wizard.\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        BuildFile buildFile;
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printUsage(argv[0]);
            return 0;
        } else if (argc != 1) {
            printUsage(argv[0]);
            return 2;
        } else {
            buildFile = runWizard();
        }

        if (buildFile.steps.empty()) {
            std::cerr << "No build steps selected; nothing was written.\n";
            return 1;
        }

        std::string error;
        if (!writeBuildFile(buildFile, error)) {
            std::cerr << "kizuku: " << error << "\n";
            return 1;
        }

        std::cout << "\n" << kAmber << "Created " << fs::absolute(buildFile.output).string()
                  << kReset << "\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "kizuku: " << exception.what() << "\n";
        return 1;
    }
}
