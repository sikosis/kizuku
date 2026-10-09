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
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

#ifndef KIZUKU_VERSION
#define KIZUKU_VERSION "0.17"
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
constexpr std::array<Theme, 12> kThemes = {{
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
    {
        "Forest",
        "#a7c957",
        "#6a994e",
        "#386641",
        "#588157",
        "#344e41",
    },
    {
        "Sunset",
        "#ffb703",
        "#fb8500",
        "#f77f00",
        "#d62828",
        "#9d4edd",
    },
    {
        "Lavender",
        "#e0aaff",
        "#c77dff",
        "#9d4edd",
        "#7b2cbf",
        "#5a189a",
    },
    {
        "Slate",
        "#cad2c5",
        "#84a98c",
        "#52796f",
        "#354f52",
        "#2f3e46",
    },
    {
        "Desert",
        "#e9c46a",
        "#f4a261",
        "#e76f51",
        "#bc6c25",
        "#6f4e37",
    },
    {
        "Neon",
        "#39ff14",
        "#00f5d4",
        "#00bbf9",
        "#9b5de5",
        "#f15bb5",
    },
    {
        "Rosewood",
        "#ffcad4",
        "#f4acb7",
        "#9d8189",
        "#7d4e57",
        "#5c374c",
    },
    {
        "Arctic",
        "#caf0f8",
        "#90e0ef",
        "#48cae4",
        "#0077b6",
        "#023e8a",
    },
    {
        "Citrus",
        "#f9c74f",
        "#90be6d",
        "#43aa8b",
        "#577590",
        "#f94144",
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
    const auto selection = selectOption("Choose a colour theme", names, kThemes.front().name);
    for (const auto& theme : kThemes) {
        if (selection == theme.name) {
            return theme;
        }
    }
    return kThemes.front();
}

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

std::vector<std::string> buildHumSnippet(const std::string& type) {
    if (type == "Hum Confirm") {
        const auto question = prompt("Confirmation question", "Continue?");
        const auto command = prompt("Command when confirmed", "echo \"Confirmed\"");
        return {"if hum confirm " + shellDoubleQuoted(question) + "; then",
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

bool editExistingScript(std::string& error) {
    fs::path path;
    if (gUseHum) {
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

    const auto type = selectOption("Block to add",
        {"Hum Confirm", "Hum Choose", "Hum Input", "Hum File Picker", "Hum Style", "Hum Write", "Hum Spin"},
        "Hum Confirm");
    const auto snippet = buildHumSnippet(type);

    showStyled("Block preview", randomColour());
    for (const auto& snippetLine : snippet) {
        std::cout << snippetLine << '\n';
    }
    std::cout << '\n';

    const auto placement = selectOption("Where should it be inserted?",
        {"After shebang", "Before a line", "After a line", "End of script"},
        "End of script");
    std::size_t insertion = lines.size();
    if (placement == "After shebang") {
        insertion = !lines.empty() && lines.front().rfind("#!", 0) == 0 ? 1 : 0;
    } else if (placement == "Before a line" || placement == "After a line") {
        if (lines.empty()) {
            error = "cannot select a line in an empty script";
            return false;
        }
        showStyled("Current script", randomColour());
        for (std::size_t index = 0; index < lines.size(); ++index) {
            std::cout << index + 1 << " | " << lines[index] << '\n';
        }
        std::size_t lineNumber = 0;
        while (!parseLineNumber(prompt("Line number"), lines.size(), lineNumber)) {
            std::cout << "Enter a line number from 1 to " << lines.size() << ".\n";
        }
        insertion = placement == "Before a line" ? lineNumber - 1 : lineNumber;
    }

    if (!confirm("Insert this block into " + path.string() + "?")) {
        error = "edit cancelled";
        return false;
    }

    std::vector<std::string> addition;
    if (insertion > 0 && !lines[insertion - 1].empty()) {
        addition.emplace_back();
    }
    addition.insert(addition.end(), snippet.begin(), snippet.end());
    if (insertion < lines.size() && !lines[insertion].empty()) {
        addition.emplace_back();
    }
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertion),
                 addition.begin(), addition.end());
    return rewriteScript(path, lines, error);
}
//---------------------------------------------------------------------------------------------------------------------------------//

const char* themedRoleColour(const std::string& line, const Theme& theme) {
    if (line.find("Update Code") != std::string::npos) {
        return theme.updateCode;
    }
    if (line.find("Make Clean") != std::string::npos) {
        return theme.clean;
    }
    if (line.find("Make Test") != std::string::npos) {
        return theme.test;
    }
    if (line.find("Run / Test") != std::string::npos) {
        return theme.run;
    }
    if (line.find("==> Step") != std::string::npos && line.find("Make") != std::string::npos) {
        return theme.build;
    }
    return nullptr;
}

std::size_t replaceThemeColours(std::string& line, const Theme& theme,
                                std::unordered_map<std::string, std::string>& colourMap,
                                std::size_t& nextColour) {
    const std::array<const char*, 5> palette = {
        theme.updateCode, theme.clean, theme.build, theme.test, theme.run,
    };
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
                newColour = palette[nextColour % palette.size()];
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
    bool insideHumStyle = false;
    for (auto& scriptLine : lines) {
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
    if (replacementCount == 0) {
        error = "no hexadecimal Hum foreground colours were found in " + path.string();
        return false;
    }

    showStyled("Theme: " + std::string(theme.name), randomColour());
    std::cout << "  Update Code  " << theme.updateCode << '\n'
              << "  Make Clean   " << theme.clean << '\n'
              << "  Make         " << theme.build << '\n'
              << "  Make Test    " << theme.test << '\n'
              << "  Run / Test   " << theme.run << '\n';
    showStyled(std::to_string(replacementCount) + " colour replacement(s) found",
               randomColour());
    if (!confirm("Apply this theme to " + path.string() + "?")) {
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
//---------------------------------------------------------------------------------------------------------------------------------//


void printUsage(const char* executable) {
    std::cout << kName << " v" << kVersion << "\n"
              << "Haiku command-line helper for generating interactive build and Git scripts.\n\n"
              << "Usage: " << executable << " [OPTION]\n\n"
              << "Options:\n"
              << "  -h, --help       Show this help\n"
              << "  -v, --version    Show the Kizuku version\n\n"
              << "Interactive workflows:\n"
              << "  Create new scripts       Generate themed build and Git helpers\n"
              << "  Edit an existing script  Insert configured Hum blocks by location\n"
              << "  Restyle a script         Replace Hum hex colours with a selected theme\n"
              << "  Quit                     Exit without making changes\n\n"
              << "When Hum is available, Kizuku uses it for prompts, styling, confirmation,\n"
              << "and project-directory selection. Plain terminal prompts are used otherwise.\n"
              << "Built-in themes: Kizuku (default), Coast, Sakura, Forest, Sunset, Lavender, Slate,\n"
              << "Desert, Neon, Rosewood, Arctic, and Citrus.\n";
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
                if (!editExistingScript(editError)) {
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
