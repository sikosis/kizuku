#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

#ifndef KIZUKU_VERSION
#define KIZUKU_VERSION "0.3"
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
    std::array<const char*, 5> colours;
    std::size_t colourCount;
};

// Built-in palettes are data-only so more themes can be added without changing
// how build steps are assembled.
constexpr std::array<Theme, 22> kThemes = {{
    {"Kizuku", {"#f2cc60", "#e8b76a", "#d98a4e", "#d98a3e", "#6f3a63"}, 5},
    {"Coast", {"#73d2de", "#52b2cf", "#2e86ab", "#33658a", "#7b6dba"}, 5},
    {"Sakura", {"#ffb3c6", "#ff8fab", "#fb6f92", "#c77dff", "#7b2cbf"}, 5},
    {"Forest", {"#a7c957", "#6a994e", "#386641", "#588157", "#344e41"}, 5},
    {"Sunset", {"#ffb703", "#fb8500", "#f77f00", "#d62828", "#9d4edd"}, 5},
    {"Lavender", {"#e0aaff", "#c77dff", "#9d4edd", "#7b2cbf", "#5a189a"}, 5},
    {"Slate", {"#cad2c5", "#84a98c", "#52796f", "#354f52", "#2f3e46"}, 5},
    {"Desert", {"#e9c46a", "#f4a261", "#e76f51", "#bc6c25", "#6f4e37"}, 5},
    {"Neon", {"#39ff14", "#00f5d4", "#00bbf9", "#9b5de5", "#f15bb5"}, 5},
    {"Rosewood", {"#ffcad4", "#f4acb7", "#9d8189", "#7d4e57", "#5c374c"}, 5},
    {"Arctic", {"#caf0f8", "#90e0ef", "#48cae4", "#0077b6", "#023e8a"}, 5},
    {"Citrus", {"#f9c74f", "#90be6d", "#43aa8b", "#577590", "#f94144"}, 5},
    {"Meadow Sunflower Glow", {"#d4e09b", "#f6f4d2", "#cbdfbd", "#f19c79", nullptr}, 4},
    {"Candy Floss Dreams", {"#ffa69e", "#faf3dd", "#b8f2e6", "#aed9e0", nullptr}, 4},
    {"Fiery Ice Cream Delight", {"#003049", "#d62828", "#f77f00", "#fcbf49", nullptr}, 4},
    {"Golden Autumn Twilight", {"#219ebc", "#023047", "#ffb703", "#fb8500", nullptr}, 4},
    {"Midnight Sun Dance", {"#001524", "#15616d", "#ffecd1", "#ff7d00", nullptr}, 4},
    {"Seaside Serenity", {"#e7ecef", "#274c77", "#6096ba", "#a3cef1", nullptr}, 4},
    {"Cherry Blossom Sky", {"#2b2d42", "#8d99ae", "#edf2f4", "#ef233c", nullptr}, 4},
    {"Cotton Candy Skies", {"#70d6ff", "#ff70a6", "#ff9770", "#ffd670", nullptr}, 4},
    {"Ocean Sunset Vibes", {"#26547c", "#ef476f", "#ffd166", "#06d6a0", nullptr}, 4},
    {"Cherry Blossom Dream", {"#880d1e", "#dd2d4a", "#f26a8d", "#f49cbb", nullptr}, 4},
}};

const char* themeColour(const Theme& theme, std::size_t role) {
    return theme.colours[role % theme.colourCount];
}
//---------------------------------------------------------------------------------------------------------------------------------//

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
//---------------------------------------------------------------------------------------------------------------------------------//


std::string randomColour() {
    static std::mt19937 generator(std::random_device{}());
    static std::uniform_int_distribution<int> distribution(1, 255);
    return std::to_string(distribution(generator));
}
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//


std::string humCommand(const std::vector<std::string>& arguments) {
    std::string command = "hum";
    for (const auto& argument : arguments) {
        command += " " + shellSingleQuoted(argument);
    }
    return command;
}
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//


std::string prompt(const std::string& label, const std::string& fallback = {}) {
    if (gUseHum) {
        std::vector<std::string> arguments = {"input", "--prompt", label + ": ", "--no-show-help"};
        if (!fallback.empty()) {
            arguments.insert(arguments.end(), {"--placeholder", fallback});
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
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//


std::string selectProjectDirectory(const std::string& projectName) {
    const std::string suggested = "/boot/home/" + projectSlug(projectName) + "/";
    if (gUseHum) {
        const fs::path start = fs::exists("/boot/home") ? fs::path("/boot/home") : fs::current_path();
        if (const auto selection = captureHum({"file", "--no-file", "--directory",
                                               "--header", "Project directory — Right arrow opens folders; Enter selects",
                                               start.string()})) {
            if (!selection->empty()) {
                return *selection;
            }
        }
    }
    return prompt("Project directory", suggested);
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string selectOption(const std::string& header, const std::vector<std::string>& options,
                         const std::string& fallback);

const Theme& selectTheme() {
    std::vector<std::string> names;
    for (const auto& theme : kThemes) {
        names.push_back(theme.name);
    }
    names.push_back("Random Theme");
    const auto selection = selectOption("Choose a colour theme", names, kThemes.front().name);
    if (selection == "Random Theme") {
        static std::mt19937 generator(std::random_device{}());
        static std::uniform_int_distribution<std::size_t> distribution(0, kThemes.size() - 1);
        const Theme& theme = kThemes[distribution(generator)];
        showStyled("Random theme selected: " + std::string(theme.name), randomColour());
        return theme;
    }
    for (const auto& theme : kThemes) {
        if (selection == theme.name) {
            return theme;
        }
    }
    return kThemes.front();
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::string selectOption(const std::string& header, const std::vector<std::string>& options,
                         const std::string& fallback) {
    if (gUseHum) {
        std::vector<std::string> arguments = {
            "choose", "--header", header, "--selected", fallback,
        };
        arguments.insert(arguments.end(), options.begin(), options.end());
        if (const auto selection = captureHum(arguments)) {
            for (const auto& option : options) {
                if (*selection == option) {
                    return option;
                }
            }
        }
    }

    while (true) {
        std::string choices;
        for (std::size_t index = 0; index < options.size(); ++index) {
            if (index != 0) {
                choices += ", ";
            }
            choices += options[index];
        }
        const auto selection = prompt(header + " (" + choices + ")", fallback);
        for (const auto& option : options) {
            if (selection == option) {
                return option;
            }
        }
        std::cout << "Choose one of: " << choices << ".\n";
    }
}

bool validShellVariable(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) ||
                           value.front() == '_')) {
        return false;
    }
    for (const unsigned char character : value) {
        if (!(std::isalnum(character) || character == '_')) {
            return false;
        }
    }
    return true;
}

std::string promptShellVariable(const std::string& fallback) {
    while (true) {
        const auto value = prompt("Shell variable name", fallback);
        if (validShellVariable(value)) {
            return value;
        }
        std::cout << "Use letters, numbers, and underscores; the first character cannot be a number.\n";
    }
}

std::vector<std::string> commaSeparated(const std::string& value) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(',', start);
        const auto item = trim(value.substr(start, end == std::string::npos ? end : end - start));
        if (!item.empty()) {
            items.push_back(item);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return items;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::vector<std::string> collectStepCommands(const std::vector<std::string>& lines) {
    std::vector<std::string> commands;
    for (std::size_t heading = 0; heading < lines.size(); ++heading) {
        if (lines[heading].find("hum style") == std::string::npos ||
            lines[heading].find("==> Step ") == std::string::npos) {
            continue;
        }

        std::size_t confirmation = heading + 1;
        while (confirmation < lines.size() && trim(lines[confirmation]).empty()) {
            ++confirmation;
        }
        if (confirmation >= lines.size() ||
            trim(lines[confirmation]).rfind("if hum confirm ", 0) != 0) {
            continue;
        }

        std::size_t depth = 1;
        std::size_t end = confirmation + 1;
        for (; end < lines.size(); ++end) {
            const auto statement = trim(lines[end]);
            if (statement.rfind("if ", 0) == 0 && statement.find("; then") != std::string::npos) {
                ++depth;
            } else if (statement == "fi") {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
        }
        if (end >= lines.size()) {
            continue;
        }

        std::size_t indentation = std::string::npos;
        for (std::size_t index = confirmation + 1; index < end; ++index) {
            if (trim(lines[index]).empty()) {
                continue;
            }
            indentation = std::min(indentation, lines[index].find_first_not_of(" \t"));
        }
        if (indentation == std::string::npos) {
            continue;
        }
        if (!commands.empty()) {
            commands.emplace_back();
        }
        for (std::size_t index = confirmation + 1; index < end; ++index) {
            if (lines[index].size() <= indentation) {
                commands.emplace_back();
            } else {
                commands.push_back(lines[index].substr(indentation));
            }
        }
        heading = end;
    }
    while (!commands.empty() && commands.back().empty()) {
        commands.pop_back();
    }
    return commands;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::vector<std::string> buildAllOrNothingSnippet(const std::vector<std::string>& lines) {
    const auto commands = collectStepCommands(lines);
    if (commands.empty()) {
        return {};
    }

    const auto question = prompt("All-or-nothing question", "Run all steps?");
    const auto defaultAnswer = selectOption("Default answer", {"No", "Yes"}, "No");
    std::vector<std::string> snippet = {
        "if hum confirm " + shellDoubleQuoted(question) + " --default=" +
            (defaultAnswer == "Yes" ? "yes" : "no") + "; then",
    };
    for (const auto& command : commands) {
        snippet.push_back(command.empty() ? std::string{} : "    " + command);
    }
    snippet.push_back("fi");
    return snippet;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::vector<std::string> buildHumSnippet(const std::string& type) {
    if (type == "Git Last Commit Show") {
        return {"git show --name-status --oneline"};
    }
    if (type == "Fancy Git Pull") {
        const auto title = prompt("Spinner title", "Updating Git");
        return {"hum spin --title " + shellDoubleQuoted(title) + " -- git pull",
                "git show --name-status --oneline"};
    }
    if (type == "Git Pull Basic") {
        const auto defaultAnswer = selectOption("Default answer", {"No", "Yes"}, "No");
        return {"hum style --foreground \"#d4e09b\" --bold \"==> Step 1: Update Code\"",
                "if hum confirm \"Git Pull?\" --default=" +
                    std::string(defaultAnswer == "Yes" ? "yes" : "no") + "; then",
                "    git pull",
                "fi"};
    }
    if (type == "Hum Confirm") {
        const auto question = prompt("Confirmation question", "Continue?");
        const auto defaultAnswer = selectOption("Default answer", {"No", "Yes"}, "No");
        const auto command = prompt("Command when confirmed", "echo \"Confirmed\"");
        return {"if hum confirm " + shellDoubleQuoted(question) + " --default=" +
                    (defaultAnswer == "Yes" ? "yes" : "no") + "; then",
                "    " + command,
                "fi"};
    }
    if (type == "Hum Choose") {
        const auto variable = promptShellVariable("choice");
        auto items = commaSeparated(prompt("Comma-separated choices", "Red, Green, Blue"));
        if (items.empty()) {
            items = {"Red", "Green", "Blue"};
        }
        std::string line = variable + "=$(hum choose";
        for (const auto& item : items) {
            line += " " + shellSingleQuoted(item);
        }
        line += ")";
        return {line};
    }
    if (type == "Hum Input") {
        const auto variable = promptShellVariable("value");
        const auto placeholder = prompt("Input placeholder", "Type a value");
        return {variable + "=$(hum input --placeholder " + shellDoubleQuoted(placeholder) + ")"};
    }
    if (type == "Hum File Picker") {
        const auto variable = promptShellVariable("selected_path");
        const auto selectionType = selectOption("What can be selected?",
            {"File", "Directory", "File or directory"}, "File");
        std::string options;
        std::string defaultHeader;
        if (selectionType == "Directory") {
            options = "--no-file --directory";
            defaultHeader = "Select a directory — Right arrow opens folders; Enter selects";
        } else if (selectionType == "File or directory") {
            options = "--file --directory";
            defaultHeader = "Select a file or directory — Right arrow opens folders; Enter selects";
        } else {
            options = "--file --no-directory";
            defaultHeader = "Select a file";
        }
        const auto header = prompt("Picker heading", defaultHeader);
        return {"project_directory=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)",
                variable + "=$(hum file " + options + " --header " + shellDoubleQuoted(header) +
                    " \"$project_directory\")"};
    }
    if (type == "Hum Style") {
        const auto text = prompt("Text to display", "Hello from Hum");
        const auto colour = prompt("Foreground colour", randomColour());
        const bool bold = confirm("Use bold text?");
        return {"hum style --foreground " + shellSingleQuoted(colour) +
                (bold ? " --bold " : " ") + shellDoubleQuoted(text)};
    }
    if (type == "Hum Write") {
        const auto variable = promptShellVariable("details");
        const auto header = prompt("Editor heading", "Enter details");
        const auto placeholder = prompt("Editor placeholder", "Type here");
        return {variable + "=$(hum write \\",
                "    --header " + shellDoubleQuoted(header) + " \\",
                "    --placeholder " + shellDoubleQuoted(placeholder) + ")"};
    }

    const auto title = prompt("Spinner title", "Working...");
    const auto command = prompt("Command to run", "make");
    return {"hum spin --title " + shellDoubleQuoted(title) + " -- " + command};
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool parseLineNumber(const std::string& value, std::size_t maximum, std::size_t& result) {
    try {
        std::size_t consumed = 0;
        const auto number = std::stoul(value, &consumed);
        if (consumed != value.size() || number < 1 || number > maximum) {
            return false;
        }
        result = number;
        return true;
    } catch (...) {
        return false;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::string insertionPreview(const std::string& line) {
    std::string preview;
    preview.reserve(line.size());
    for (const unsigned char character : line) {
        if (character < 32 || character == 127) {
            preview.push_back(' ');
        } else {
            preview.push_back(static_cast<char>(character));
        }
    }
    return preview.empty() ? "(blank line)" : preview;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::optional<std::size_t> selectInsertionPoint(const std::vector<std::string>& lines,
                                                const std::string& blockType) {
    if (gUseHum) {
        constexpr const char* delimiter = "\t";
        std::vector<std::string> arguments = {
            "choose", "--header", "Insert " + blockType + " before:",
            "--height", "15", "--label-delimiter", delimiter,
        };
        for (std::size_t index = 0; index < lines.size(); ++index) {
            arguments.push_back("Line " + std::to_string(index + 1) + " | " +
                                insertionPreview(lines[index]) + delimiter +
                                std::to_string(index));
        }
        const std::string endOption = "End of script" + std::string(delimiter) +
                                      std::to_string(lines.size());
        arguments.push_back(endOption);
        arguments.push_back("Cancel" + std::string(delimiter) + "cancel");

        const auto selection = captureHum(arguments);
        if (!selection || *selection == "cancel") {
            return std::nullopt;
        }
        try {
            std::size_t consumed = 0;
            const auto insertion = std::stoul(*selection, &consumed);
            if (consumed == selection->size() && insertion <= lines.size()) {
                return insertion;
            }
        } catch (...) {
        }
        return std::nullopt;
    }

    showStyled("Choose where to insert " + blockType, randomColour());
    std::cout << "0 | Cancel\n";
    for (std::size_t index = 0; index < lines.size(); ++index) {
        std::cout << index + 1 << " | Before: " << insertionPreview(lines[index]) << '\n';
    }
    const std::size_t endChoice = lines.size() + 1;
    std::cout << endChoice << " | End of script\n";

    while (true) {
        const auto selection = prompt("Insertion point", std::to_string(endChoice));
        if (selection == "0") {
            return std::nullopt;
        }
        std::size_t choice = 0;
        if (parseLineNumber(selection, endChoice, choice)) {
            return choice - 1;
        }
        std::cout << "Enter a number from 0 to " << endChoice << ".\n";
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool isStandardGitPullLine(const std::string& line) {
    return trim(line) == "git pull";
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::optional<std::size_t> selectGitPullLine(const std::vector<std::string>& lines) {
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (isStandardGitPullLine(lines[index])) {
            matches.push_back(index);
        }
    }
    if (matches.empty()) {
        return std::nullopt;
    }

    if (gUseHum) {
        constexpr const char* delimiter = "\t";
        std::vector<std::string> arguments = {
            "choose", "--header", "Select the git pull line to replace:",
            "--height", "15", "--label-delimiter", delimiter,
        };
        for (const auto index : matches) {
            arguments.push_back("Line " + std::to_string(index + 1) + " | " +
                                insertionPreview(lines[index]) + delimiter +
                                std::to_string(index));
        }
        arguments.push_back("Cancel" + std::string(delimiter) + "cancel");
        const auto selection = captureHum(arguments);
        if (!selection || *selection == "cancel") {
            return std::nullopt;
        }
        try {
            std::size_t consumed = 0;
            const auto index = std::stoul(*selection, &consumed);
            if (consumed == selection->size() && index < lines.size() &&
                isStandardGitPullLine(lines[index])) {
                return index;
            }
        } catch (...) {
        }
        return std::nullopt;
    }

    showStyled("Standard Git pull lines", randomColour());
    std::cout << "0 | Cancel\n";
    for (std::size_t choice = 0; choice < matches.size(); ++choice) {
        const auto index = matches[choice];
        std::cout << choice + 1 << " | Line " << index + 1 << " | "
                  << insertionPreview(lines[index]) << '\n';
    }
    while (true) {
        const auto selection = prompt("Git pull line to replace");
        if (selection == "0") {
            return std::nullopt;
        }
        std::size_t choice = 0;
        if (parseLineNumber(selection, matches.size(), choice)) {
            return matches[choice - 1];
        }
        std::cout << "Enter a number from 0 to " << matches.size() << ".\n";
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::size_t findUnquotedOption(const std::string& line, const std::string& option,
                               std::size_t start) {
    bool insideSingleQuote = false;
    bool insideDoubleQuote = false;
    bool escaped = false;
    for (std::size_t index = start; index + option.size() <= line.size(); ++index) {
        const char character = line[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && !insideSingleQuote) {
            escaped = true;
            continue;
        }
        if (character == '\'' && !insideDoubleQuote) {
            insideSingleQuote = !insideSingleQuote;
            continue;
        }
        if (character == '"' && !insideSingleQuote) {
            insideDoubleQuote = !insideDoubleQuote;
            continue;
        }
        if (insideSingleQuote || insideDoubleQuote || line.compare(index, option.size(), option) != 0) {
            continue;
        }
        const bool startsToken = index == 0 ||
            std::isspace(static_cast<unsigned char>(line[index - 1]));
        const std::size_t after = index + option.size();
        const bool endsToken = after == line.size() || line[after] == '=' ||
            std::isspace(static_cast<unsigned char>(line[after]));
        if (startsToken && endsToken) {
            return index;
        }
    }
    return std::string::npos;
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool isHumConfirmLine(const std::string& line) {
    const auto firstCharacter = line.find_first_not_of(" \t");
    return firstCharacter != std::string::npos && line[firstCharacter] != '#' &&
           findUnquotedOption(line, "hum confirm", firstCharacter) != std::string::npos;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::string humConfirmDefaultStatus(const std::string& line) {
    const auto command = findUnquotedOption(line, "hum confirm", 0);
    if (command == std::string::npos) {
        return "not set";
    }
    const auto option = findUnquotedOption(line, "--default", command);
    if (option == std::string::npos) {
        return "not set";
    }

    std::size_t valueStart = option + std::string("--default").size();
    if (valueStart < line.size() && line[valueStart] == '=') {
        ++valueStart;
    } else {
        while (valueStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[valueStart]))) {
            ++valueStart;
        }
    }
    std::size_t valueEnd = valueStart;
    while (valueEnd < line.size() &&
           !std::isspace(static_cast<unsigned char>(line[valueEnd])) && line[valueEnd] != ';') {
        ++valueEnd;
    }
    std::string value = line.substr(valueStart, valueEnd - valueStart);
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value.empty() ? "not set" : value;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::optional<std::size_t> selectHumConfirmLine(const std::vector<std::string>& lines) {
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (isHumConfirmLine(lines[index])) {
            matches.push_back(index);
        }
    }
    if (matches.empty()) {
        return std::nullopt;
    }

    if (gUseHum) {
        constexpr const char* delimiter = "\t";
        std::vector<std::string> arguments = {
            "choose", "--header", "Select a Hum confirm to update:",
            "--height", "15", "--label-delimiter", delimiter,
        };
        for (const auto index : matches) {
            arguments.push_back("Line " + std::to_string(index + 1) + " [default: " +
                                humConfirmDefaultStatus(lines[index]) + "] | " +
                                insertionPreview(lines[index]) + delimiter + std::to_string(index));
        }
        arguments.push_back("Cancel" + std::string(delimiter) + "cancel");
        const auto selection = captureHum(arguments);
        if (!selection || *selection == "cancel") {
            return std::nullopt;
        }
        try {
            std::size_t consumed = 0;
            const auto index = std::stoul(*selection, &consumed);
            if (consumed == selection->size() && index < lines.size()) {
                return index;
            }
        } catch (...) {
        }
        return std::nullopt;
    }

    showStyled("Hum confirm status", randomColour());
    std::cout << "0 | Cancel\n";
    for (std::size_t choice = 0; choice < matches.size(); ++choice) {
        const auto index = matches[choice];
        std::cout << choice + 1 << " | Line " << index + 1 << " [default: "
                  << humConfirmDefaultStatus(lines[index]) << "] | "
                  << insertionPreview(lines[index]) << '\n';
    }
    while (true) {
        const auto selection = prompt("Confirmation to update");
        if (selection == "0") {
            return std::nullopt;
        }
        std::size_t choice = 0;
        if (parseLineNumber(selection, matches.size(), choice)) {
            return matches[choice - 1];
        }
        std::cout << "Enter a number from 0 to " << matches.size() << ".\n";
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//


void setHumConfirmDefault(std::string& line, const std::string& answer) {
    const auto command = findUnquotedOption(line, "hum confirm", 0);
    const auto option = findUnquotedOption(line, "--default", command);
    const std::string replacement = "--default=" + answer;
    if (option != std::string::npos) {
        std::size_t valueEnd = option + std::string("--default").size();
        if (valueEnd < line.size() && line[valueEnd] == '=') {
            ++valueEnd;
        } else {
            while (valueEnd < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[valueEnd]))) {
                ++valueEnd;
            }
        }
        while (valueEnd < line.size() &&
               !std::isspace(static_cast<unsigned char>(line[valueEnd])) && line[valueEnd] != ';') {
            ++valueEnd;
        }
        line.replace(option, valueEnd - option, replacement);
        return;
    }

    const auto then = line.find("; then", command);
    if (then != std::string::npos) {
        line.insert(then, " " + replacement);
        return;
    }
    const auto contentEnd = line.find_last_not_of(" \t");
    line.insert(contentEnd == std::string::npos ? 0 : contentEnd + 1, " " + replacement);
}
//---------------------------------------------------------------------------------------------------------------------------------//


struct VersionOccurrence {
    std::size_t line;
    std::size_t column;
    std::size_t length;
    std::string value;
};

std::vector<VersionOccurrence> findVersionOccurrences(const std::vector<std::string>& lines) {
    std::vector<VersionOccurrence> occurrences;
    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const auto& line = lines[lineIndex];
        for (std::size_t start = 0; start < line.size(); ++start) {
            if (line[start] != 'v' ||
                (start > 0 && (std::isalnum(static_cast<unsigned char>(line[start - 1])) ||
                               line[start - 1] == '_'))) {
                continue;
            }
            std::size_t cursor = start + 1;
            const std::size_t majorStart = cursor;
            while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            if (cursor == majorStart || cursor >= line.size() || line[cursor] != '.') {
                continue;
            }
            ++cursor;
            const std::size_t fractionStart = cursor;
            while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            if (cursor == fractionStart ||
                (cursor < line.size() && (line[cursor] == '.' || line[cursor] == '_' ||
                                          std::isalnum(static_cast<unsigned char>(line[cursor]))))) {
                continue;
            }
            occurrences.push_back({lineIndex, start, cursor - start,
                                   line.substr(start, cursor - start)});
            start = cursor - 1;
        }
    }
    return occurrences;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::optional<VersionOccurrence> selectVersionOccurrence(
        const std::vector<std::string>& lines,
        const std::vector<VersionOccurrence>& occurrences) {
    if (occurrences.empty()) {
        return std::nullopt;
    }
    if (gUseHum) {
        constexpr const char* delimiter = "\t";
        std::vector<std::string> arguments = {
            "choose", "--header", "Select a script version to update:",
            "--height", "15", "--label-delimiter", delimiter,
        };
        for (std::size_t choice = 0; choice < occurrences.size(); ++choice) {
            const auto& occurrence = occurrences[choice];
            arguments.push_back("Line " + std::to_string(occurrence.line + 1) +
                                ", column " + std::to_string(occurrence.column + 1) +
                                " [" + occurrence.value + "] | " +
                                insertionPreview(lines[occurrence.line]) + delimiter +
                                std::to_string(choice));
        }
        arguments.push_back("Cancel" + std::string(delimiter) + "cancel");
        const auto selection = captureHum(arguments);
        if (!selection || *selection == "cancel") {
            return std::nullopt;
        }
        try {
            std::size_t consumed = 0;
            const auto choice = std::stoul(*selection, &consumed);
            if (consumed == selection->size() && choice < occurrences.size()) {
                return occurrences[choice];
            }
        } catch (...) {
        }
        return std::nullopt;
    }

    showStyled("Script versions", randomColour());
    std::cout << "0 | Cancel\n";
    for (std::size_t choice = 0; choice < occurrences.size(); ++choice) {
        const auto& occurrence = occurrences[choice];
        std::cout << choice + 1 << " | Line " << occurrence.line + 1
                  << ", column " << occurrence.column + 1 << " [" << occurrence.value
                  << "] | " << insertionPreview(lines[occurrence.line]) << '\n';
    }
    while (true) {
        const auto selection = prompt("Version to update");
        if (selection == "0") {
            return std::nullopt;
        }
        std::size_t choice = 0;
        if (parseLineNumber(selection, occurrences.size(), choice)) {
            return occurrences[choice - 1];
        }
        std::cout << "Enter a number from 0 to " << occurrences.size() << ".\n";
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::optional<std::string> adjustVersion(const std::string& version, bool increment,
                                         std::string& error) {
    const auto point = version.find('.');
    if (version.size() < 4 || version.front() != 'v' || point == std::string::npos) {
        error = "unsupported version format: " + version;
        return std::nullopt;
    }
    const std::string majorText = version.substr(1, point - 1);
    const std::string fractionText = version.substr(point + 1);
    try {
        unsigned long long scale = 1;
        for (std::size_t digit = 0; digit < fractionText.size(); ++digit) {
            if (scale > std::numeric_limits<unsigned long long>::max() / 10) {
                throw std::out_of_range("version precision is too large");
            }
            scale *= 10;
        }
        const unsigned long long major = std::stoull(majorText);
        const unsigned long long fraction = std::stoull(fractionText);
        if (major > (std::numeric_limits<unsigned long long>::max() - fraction) / scale) {
            throw std::out_of_range("version is too large");
        }
        unsigned long long value = major * scale + fraction;
        const unsigned long long step = scale / 10;
        if (increment) {
            if (value > std::numeric_limits<unsigned long long>::max() - step) {
                throw std::out_of_range("version is too large");
            }
            value += step;
        } else {
            if (value < step) {
                error = version + " cannot be decremented below zero";
                return std::nullopt;
            }
            value -= step;
        }

        std::string fractionResult = std::to_string(value % scale);
        if (fractionResult.size() < fractionText.size()) {
            fractionResult.insert(0, fractionText.size() - fractionResult.size(), '0');
        }
        return "v" + std::to_string(value / scale) + "." + fractionResult;
    } catch (const std::exception& exception) {
        error = "cannot update " + version + ": " + exception.what();
        return std::nullopt;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool rewriteScript(const fs::path& path, const std::vector<std::string>& lines, std::string& error) {
    struct stat details {};
    if (::stat(path.c_str(), &details) != 0) {
        error = "cannot read permissions for " + path.string();
        return false;
    }

    fs::path temporary = path;
    temporary += ".kizuku.tmp." + std::to_string(::getpid());
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "cannot create temporary file beside " + path.string();
        return false;
    }
    for (const auto& line : lines) {
        output << line << '\n';
    }
    output.close();
    if (!output) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        error = "failed while writing the updated script";
        return false;
    }
    if (::chmod(temporary.c_str(), details.st_mode & 07777) != 0) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        error = "could not preserve the script permissions";
        return false;
    }

    std::error_code renameError;
    fs::rename(temporary, path, renameError);
    if (renameError) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        error = "could not replace the script: " + renameError.message();
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool editScriptOnce(const fs::path& path, std::string& error) {
    if (!fs::is_regular_file(path)) {
        error = path.string() + " is not a regular file";
        return false;
    }

    std::ifstream input(path);
    if (!input) {
        error = "cannot open " + path.string();
        return false;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (!input.eof()) {
        error = "failed while reading " + path.string();
        return false;
    }

    const auto type = selectOption("Edit action or block to add",
        {"Hum Confirm", "Check Hum Confirm Status", "Update Script Version", "All or Nothing",
         "Git Pull Basic", "Fancy Git Pull", "Git Last Commit Show", "Hum Choose", "Hum Input",
         "Hum File Picker", "Hum Style", "Hum Write", "Hum Spin"},
        "Hum Confirm");
    if (type == "Check Hum Confirm Status") {
        const bool hasHumConfirmation = std::any_of(lines.begin(), lines.end(),
            [](const std::string& scriptLine) {
                return isHumConfirmLine(scriptLine);
            });
        if (!hasHumConfirmation) {
            error = "no Hum confirm lines were found in " + path.string();
            return false;
        }
        const auto selectedLine = selectHumConfirmLine(lines);
        if (!selectedLine) {
            error = "edit cancelled";
            return false;
        }
        const auto defaultAnswer = selectOption("Default answer", {"No", "Yes"}, "No");
        const std::string original = lines[*selectedLine];
        setHumConfirmDefault(lines[*selectedLine], defaultAnswer == "Yes" ? "yes" : "no");
        showStyled("Confirmation preview", randomColour());
        std::cout << "Before: " << original << '\n'
                  << "After:  " << lines[*selectedLine] << "\n\n";
        if (!confirm("Update this Hum confirmation in " + path.string() + "?")) {
            error = "edit cancelled";
            return false;
        }
        return rewriteScript(path, lines, error);
    }
    if (type == "Update Script Version") {
        const auto occurrences = findVersionOccurrences(lines);
        if (occurrences.empty()) {
            error = "no v<number>.<number> versions were found in " + path.string();
            return false;
        }
        const auto occurrence = selectVersionOccurrence(lines, occurrences);
        if (!occurrence) {
            error = "edit cancelled";
            return false;
        }
        const auto operation = selectOption("Version change",
            {"Increment (+0.1)", "Decrement (-0.1)", "Cancel"}, "Increment (+0.1)");
        if (operation == "Cancel") {
            error = "edit cancelled";
            return false;
        }
        const auto updatedVersion = adjustVersion(
            occurrence->value, operation == "Increment (+0.1)", error);
        if (!updatedVersion) {
            return false;
        }
        const std::string original = lines[occurrence->line];
        lines[occurrence->line].replace(occurrence->column, occurrence->length, *updatedVersion);
        showStyled("Version preview", randomColour());
        std::cout << "Before: " << original << '\n'
                  << "After:  " << lines[occurrence->line] << "\n\n";
        if (!confirm("Update this version in " + path.string() + "?")) {
            error = "edit cancelled";
            return false;
        }
        return rewriteScript(path, lines, error);
    }
    if (type == "Fancy Git Pull") {
        const bool hasStandardGitPull = std::any_of(lines.begin(), lines.end(),
            [](const std::string& scriptLine) {
                return isStandardGitPullLine(scriptLine);
            });
        if (!hasStandardGitPull) {
            error = "no standalone git pull lines were found in " + path.string();
            return false;
        }
        const auto selectedLine = selectGitPullLine(lines);
        if (!selectedLine) {
            error = "edit cancelled";
            return false;
        }
        const auto snippet = buildHumSnippet(type);
        const auto contentStart = lines[*selectedLine].find_first_not_of(" \t");
        const std::string indentation = lines[*selectedLine].substr(0, contentStart);
        showStyled("Block preview", randomColour());
        std::cout << "Before: " << lines[*selectedLine] << '\n'
                  << "After:  " << indentation << snippet[0] << '\n'
                  << "        " << indentation << snippet[1] << "\n\n";
        if (!confirm("Replace this git pull in " + path.string() + "?")) {
            error = "edit cancelled";
            return false;
        }
        lines[*selectedLine] = indentation + snippet[0];
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(*selectedLine + 1),
                     indentation + snippet[1]);
        return rewriteScript(path, lines, error);
    }
    const auto snippet = type == "All or Nothing"
        ? buildAllOrNothingSnippet(lines) : buildHumSnippet(type);
    if (snippet.empty()) {
        error = "no commands were found in styled Hum step blocks";
        return false;
    }

    showStyled("Block preview", randomColour());
    for (const auto& snippetLine : snippet) {
        std::cout << snippetLine << '\n';
    }
    std::cout << '\n';

    const auto insertion = selectInsertionPoint(lines, type);
    if (!insertion) {
        error = "edit cancelled";
        return false;
    }

    if (!confirm("Insert this block into " + path.string() + "?")) {
        error = "edit cancelled";
        return false;
    }

    std::vector<std::string> addition;
    if (*insertion > 0 && !lines[*insertion - 1].empty()) {
        addition.emplace_back();
    }
    addition.insert(addition.end(), snippet.begin(), snippet.end());
    if (*insertion < lines.size() && !lines[*insertion].empty()) {
        addition.emplace_back();
    }
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(*insertion),
                 addition.begin(), addition.end());
    return rewriteScript(path, lines, error);
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool editExistingScript(fs::path path, std::string& error) {
    if (path.empty() && gUseHum) {
        const fs::path start = fs::current_path();
        if (const auto selection = captureHum({"file", "--file", "--no-directory",
                                               "--header", "Select a script to edit",
                                               start.string()})) {
            path = *selection;
        }
    }
    if (path.empty()) {
        path = prompt("Script to edit");
    }

    bool changed = false;
    while (true) {
        error.clear();
        if (!editScriptOnce(path, error)) {
            if (changed && error == "edit cancelled") {
                error.clear();
                return true;
            }
            return false;
        }
        changed = true;
        if (!confirm("Make another edit to " + path.string() + "?", false)) {
            return true;
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

const char* themedRoleColour(const std::string& line, const Theme& theme) {
    if (line.find("Update Code") != std::string::npos) {
        return themeColour(theme, 0);
    }
    if (line.find("Make Clean") != std::string::npos) {
        return themeColour(theme, 1);
    }
    if (line.find("Make Test") != std::string::npos) {
        return themeColour(theme, 3);
    }
    if (line.find("Run / Test") != std::string::npos) {
        return themeColour(theme, 4);
    }
    if (line.find("==> Step") != std::string::npos && line.find("Make") != std::string::npos) {
        return themeColour(theme, 2);
    }
    return nullptr;
}

std::size_t replaceThemeColours(std::string& line, const Theme& theme,
                                std::unordered_map<std::string, std::string>& colourMap,
                                std::size_t& nextColour) {
    std::size_t replacements = 0;
    std::size_t searchFrom = 0;
    while (true) {
        const auto option = line.find("--foreground", searchFrom);
        if (option == std::string::npos) {
            break;
        }
        std::size_t tokenStart = option + std::string("--foreground").size();
        while (tokenStart < line.size() && std::isspace(static_cast<unsigned char>(line[tokenStart]))) {
            ++tokenStart;
        }
        if (tokenStart >= line.size()) {
            break;
        }

        const char quote = line[tokenStart] == '\'' || line[tokenStart] == '"' ? line[tokenStart] : '\0';
        const std::size_t colourStart = tokenStart + (quote == '\0' ? 0 : 1);
        if (colourStart + 7 > line.size() || line[colourStart] != '#') {
            searchFrom = colourStart;
            continue;
        }
        bool valid = true;
        for (std::size_t index = colourStart + 1; index < colourStart + 7; ++index) {
            if (!std::isxdigit(static_cast<unsigned char>(line[index]))) {
                valid = false;
                break;
            }
        }
        const std::size_t colourEnd = colourStart + 7;
        if (!valid || (quote != '\0' && (colourEnd >= line.size() || line[colourEnd] != quote))) {
            searchFrom = colourStart + 1;
            continue;
        }

        std::string oldColour = line.substr(colourStart, 7);
        for (char& character : oldColour) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        std::string newColour;
        if (const char* roleColour = themedRoleColour(line, theme)) {
            newColour = roleColour;
        } else {
            const auto existing = colourMap.find(oldColour);
            if (existing != colourMap.end()) {
                newColour = existing->second;
            } else {
                newColour = themeColour(theme, nextColour);
                colourMap.emplace(oldColour, newColour);
                ++nextColour;
            }
        }

        const std::size_t tokenEnd = colourEnd + (quote == '\0' ? 0 : 1);
        const std::string replacement = "\"" + newColour + "\"";
        line.replace(tokenStart, tokenEnd - tokenStart, replacement);
        searchFrom = tokenStart + replacement.size();
        ++replacements;
    }
    return replacements;
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool renumberStepHeading(std::string& line, std::size_t number, bool& changed) {
    constexpr const char* prefix = "==> Step ";
    const auto numberStart = line.find(prefix);
    if (numberStart == std::string::npos) {
        return false;
    }

    const std::size_t digitStart = numberStart + std::string(prefix).size();
    std::size_t digitEnd = digitStart;
    while (digitEnd < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[digitEnd]))) {
        ++digitEnd;
    }
    if (digitEnd == digitStart || digitEnd >= line.size() || line[digitEnd] != ':') {
        return false;
    }

    const std::string replacement = std::to_string(number);
    changed = line.compare(digitStart, digitEnd - digitStart, replacement) != 0;
    if (changed) {
        line.replace(digitStart, digitEnd - digitStart, replacement);
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool restyleExistingScript(std::string& error) {
    fs::path path;
    if (gUseHum) {
        const fs::path start = fs::current_path();
        if (const auto selection = captureHum({"file", "--file", "--no-directory",
                                               "--header", "Select a script to restyle",
                                               start.string()})) {
            path = *selection;
        }
    }
    if (path.empty()) {
        path = prompt("Script to restyle");
    }
    if (!fs::is_regular_file(path)) {
        error = path.string() + " is not a regular file";
        return false;
    }

    std::ifstream input(path);
    if (!input) {
        error = "cannot open " + path.string();
        return false;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (!input.eof()) {
        error = "failed while reading " + path.string();
        return false;
    }

    const Theme& theme = selectTheme();
    std::unordered_map<std::string, std::string> colourMap;
    std::size_t nextColour = 0;
    std::size_t replacementCount = 0;
    std::size_t headingUpdateCount = 0;
    std::size_t nextStepNumber = 1;
    bool insideHumStyle = false;
    for (auto& scriptLine : lines) {
        bool headingChanged = false;
        if (renumberStepHeading(scriptLine, nextStepNumber, headingChanged)) {
            ++nextStepNumber;
            if (headingChanged) {
                ++headingUpdateCount;
            }
        }
        if (scriptLine.find("hum style") != std::string::npos) {
            insideHumStyle = true;
        }
        if (insideHumStyle) {
            replacementCount += replaceThemeColours(scriptLine, theme, colourMap, nextColour);
        }
        const auto lastCharacter = scriptLine.find_last_not_of(" \t");
        const bool continues = lastCharacter != std::string::npos && scriptLine[lastCharacter] == '\\';
        if (insideHumStyle && !continues) {
            insideHumStyle = false;
        }
    }
    if (replacementCount == 0 && headingUpdateCount == 0) {
        error = "no hexadecimal Hum foreground colours or out-of-sequence step headings were found in " +
                path.string();
        return false;
    }

    showStyled("Theme: " + std::string(theme.name), randomColour());
    std::cout << "  Update Code  " << themeColour(theme, 0) << '\n'
              << "  Make Clean   " << themeColour(theme, 1) << '\n'
              << "  Make         " << themeColour(theme, 2) << '\n'
              << "  Make Test    " << themeColour(theme, 3) << '\n'
              << "  Run / Test   " << themeColour(theme, 4) << '\n';
    showStyled(std::to_string(replacementCount) + " colour replacement(s) and " +
               std::to_string(headingUpdateCount) + " step number update(s) found", randomColour());
    if (!confirm("Apply these restyle changes to " + path.string() + "?")) {
        error = "edit cancelled";
        return false;
    }
    return rewriteScript(path, lines, error);
}
//---------------------------------------------------------------------------------------------------------------------------------//


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
//---------------------------------------------------------------------------------------------------------------------------------//

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
//---------------------------------------------------------------------------------------------------------------------------------//

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
//---------------------------------------------------------------------------------------------------------------------------------//

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
//---------------------------------------------------------------------------------------------------------------------------------//


WizardResult runWizard() {
    const auto projectName = prompt("Project name", "My Project");
    const auto version = prompt("Builder version", "1.0");
    const auto directory = selectProjectDirectory(projectName);
    const Theme& theme = selectTheme();

    std::vector<Step> steps;
    if (confirm("Include an Update Code step (git pull)?")) {
        steps.push_back({"Update Code", "Git Pull?", themeColour(theme, 0), {"git pull"}});
    }
    if (confirm("Include a clean step?")) {
        steps.push_back({"Make Clean", "Make Clean?", themeColour(theme, 1),
                         {prompt("Clean command", "make clean")}});
    }
    if (confirm("Include a build step?")) {
        steps.push_back({"Make", "Make?", themeColour(theme, 2), {prompt("Build command", "make")}});
    }
    if (confirm("Include a test step?")) {
        steps.push_back({"Make Test", "Make Tests?", themeColour(theme, 3),
                         {prompt("Test command", "make test")}});
    }
    if (confirm("Include a run step?")) {
        const auto command = prompt("Run command", "./my-project");
        steps.push_back({"Run / Test", "Run?", themeColour(theme, 4), {command}});
    }
    const fs::path output = steps.empty() ? fs::path{} : fs::path(prompt("Output build file", "build.sh"));

    std::optional<GitFile> gitFile;
    if (confirm("Create a Git commit-and-push script?", false)) {
        gitFile = GitFile{projectName, prompt("Output Git script", "git.sh"),
                          randomColour(), randomColour(), randomColour()};
    }

    return {{projectName + " Builder v" + version, randomColour(), directory, output, steps}, gitFile};
}
//---------------------------------------------------------------------------------------------------------------------------------//


void printUsage(const char* executable) {
    std::cout << kName << " v" << kVersion << "\n"
              << "Haiku command-line helper for generating interactive build and Git scripts.\n\n"
              << "Usage: " << executable << " [OPTION | SCRIPT]\n\n"
              << "Options:\n"
              << "  -h, --help       Show this help\n"
              << "  -v, --version    Show the Kizuku version\n\n"
              << "Arguments:\n"
              << "  SCRIPT           Open this file directly in the Edit workflow\n\n"
              << "Interactive workflows:\n"
              << "  Create new scripts       Generate themed build and Git helpers\n"
              << "  Edit an existing script  Insert blocks or update confirms and versions\n"
              << "  Restyle a script         Replace Hum hex colours with a selected theme\n"
              << "  Quit                     Exit without making changes\n\n"
              << "When Hum is available, Kizuku uses it for prompts, styling, confirmation,\n"
              << "and project-directory selection. Plain terminal prompts are used otherwise.\n"
              << "Built-in themes: Kizuku (default), Coast, Sakura, Forest, Sunset, Lavender, Slate,\n"
              << "Desert, Neon, Rosewood, Arctic, Citrus, and ten popular four-colour palettes.\n";
}
//---------------------------------------------------------------------------------------------------------------------------------//


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
        } else if (argc == 2 && std::string(argv[1]).rfind("-", 0) != 0) {
            std::string editError;
            if (!editExistingScript(fs::path(argv[1]), editError)) {
                if (editError != "edit cancelled") {
                    std::cerr << "kizuku: " << editError << "\n";
                    return 1;
                }
                std::cout << "No changes made.\n";
                return 0;
            }
            showStyled("Script updated successfully", randomColour());
            return 0;
        } else if (argc != 1) {
            printUsage(argv[0]);
            return 2;
        } else {
            std::cout << "\n";
            showStyled(std::string(kName) + " v" + kVersion + " — Haiku command-line helper",
                       randomColour(), true);
            std::cout << "\n";
            const auto action = selectOption("What would you like to do?",
                {"Create new scripts", "Edit an existing script", "Restyle an existing script", "Quit"},
                "Create new scripts");
            if (action == "Quit") {
                std::cout << "Sayonara.\n";
                return 0;
            }
            if (action == "Edit an existing script") {
                std::string editError;
                if (!editExistingScript({}, editError)) {
                    if (editError != "edit cancelled") {
                        std::cerr << "kizuku: " << editError << "\n";
                        return 1;
                    }
                    std::cout << "No changes made.\n";
                    return 0;
                }
                showStyled("Script updated successfully", randomColour());
                return 0;
            }
            if (action == "Restyle an existing script") {
                std::string restyleError;
                if (!restyleExistingScript(restyleError)) {
                    if (restyleError != "edit cancelled") {
                        std::cerr << "kizuku: " << restyleError << "\n";
                        return 1;
                    }
                    std::cout << "No changes made.\n";
                    return 0;
                }
                showStyled("Script theme updated successfully", randomColour());
                return 0;
            }
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
//---------------------------------------------------------------------------------------------------------------------------------//
