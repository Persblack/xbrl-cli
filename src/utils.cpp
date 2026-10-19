#include "utils.h"

#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

void secureZero(std::string& s) {
    volatile char* p = const_cast<volatile char*>(s.data());
    for (size_t i = 0; i < s.size(); ++i) {
        p[i] = 0;
    }
    s.clear();
}

std::string readPassword() {
    std::cout << "Zertifikat-Passwort: " << std::flush;

#ifdef _WIN32
    std::string password;
    char ch;
    while ((ch = _getch()) != '\r') {
        if (ch == '\b' && !password.empty()) {
            password.pop_back();
        } else if (ch != '\b') {
            password.push_back(ch);
        }
    }
    std::cout << "\n";
#else
    std::string password;
    termios oldt{};
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &oldt) != 0) {
        // Piped/redirected stdin: nothing to hide, plain read
        std::getline(std::cin, password);
        return password;
    }

    // RAII: echo is restored on every exit path (return, exception).
    // ponytail: a SIGINT still kills the process before the destructor runs;
    // no signal handler installed — the shell's `stty sane` is the fallback.
    struct EchoOff {
        termios saved;
        explicit EchoOff(const termios& t) : saved(t) {
            termios n = t;
            n.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &n);
        }
        ~EchoOff() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved); }
    } echoOff(oldt);

    std::getline(std::cin, password);
    std::cout << "\n";
#endif

    return password;
}

std::string parseDate(const std::string& germanDate) {
    // Expected input: "DD.MM.YYYY" or "D.M.YYYY"
    // Output: "YYYY-MM-DD"
    if (germanDate.empty()) {
        return "";
    }

    int day = 0, month = 0, year = 0;
    char dot1 = 0, dot2 = 0;

    std::istringstream iss(germanDate);
    iss >> day >> dot1 >> month >> dot2 >> year;

    if (dot1 != '.' || dot2 != '.' || year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
        // Maybe it's already ISO format — digits only, no "abcd-ef-gh" passthrough
        static const std::regex isoDate(R"(^\d{4}-\d{2}-\d{2}$)");
        if (std::regex_match(germanDate, isoDate)) {
            return germanDate;
        }
        throw std::runtime_error("Ungültiges Datumsformat: " + germanDate + " (erwartet: TT.MM.JJJJ)");
    }

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << year << "-"
        << std::setw(2) << month << "-"
        << std::setw(2) << day;
    return oss.str();
}

std::string formatCurrency(double value) {
    // Format as German currency: 1.234,56
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string str = oss.str();

    // Find decimal point position
    auto dotPos = str.find('.');
    std::string intPart = str.substr(0, dotPos);
    std::string decPart = str.substr(dotPos + 1);

    // Handle negative sign
    bool negative = false;
    if (!intPart.empty() && intPart[0] == '-') {
        negative = true;
        intPart = intPart.substr(1);
    }

    // Add thousand separators (German: .)
    std::string formatted;
    int count = 0;
    for (int i = static_cast<int>(intPart.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) {
            formatted = "." + formatted;
        }
        formatted = intPart[static_cast<size_t>(i)] + formatted;
        ++count;
    }

    if (negative) {
        formatted = "-" + formatted;
    }

    // German: comma as decimal separator
    return formatted + "," + decPart;
}

int monthsBetween(const std::string& dateFrom, const std::string& dateTo) {
    // Both dates in ISO format: YYYY-MM-DD
    if (dateFrom.size() < 7 || dateTo.size() < 7) {
        return 0;
    }

    int yearFrom = std::stoi(dateFrom.substr(0, 4));
    int monthFrom = std::stoi(dateFrom.substr(5, 2));
    int yearTo = std::stoi(dateTo.substr(0, 4));
    int monthTo = std::stoi(dateTo.substr(5, 2));

    return (yearTo - yearFrom) * 12 + (monthTo - monthFrom);
}

std::string readFileContent(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Datei nicht gefunden: " + path);
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

std::string extractYear(const std::string& isoDate) {
    if (isoDate.size() >= 4) {
        return isoDate.substr(0, 4);
    }
    return "";
}

std::string userHomeDir() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? home : ".";
}

bool testmerkerMatchesMode(const std::string& xml, bool testMode) {
    bool hasTestmerker = xml.find("<Testmerker>") != std::string::npos;
    return hasTestmerker == testMode;
}
