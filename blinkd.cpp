#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <regex>
#include <sys/stat.h> // For checking file existence

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return str.substr(first, last - first + 1);
}

bool file_exists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

class DotAnimator {
public:
    DotAnimator() : active(false) {}

    void start(const std::string& message = "") {
        active = true;
        anim_thread = std::thread([this, message]() {
            int dot_count = 1;
            while (active) {
                std::string dots = std::string(dot_count, '.');
                std::cout << "\r" << message << " " << dots << std::string(4 - dot_count, ' ') << std::flush;
                dot_count++;
                if (dot_count > 4) dot_count = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
            std::cout << "\r" << std::string(message.size() + 5, ' ') << "\r" << std::flush;
            });
    }

    void stop() {
        active = false;
        if (anim_thread.joinable())
            anim_thread.join();
    }

private:
    std::atomic<bool> active;
    std::thread anim_thread;
};

std::string fetch_content(const std::string& url, const std::string& tmpfile) {
    std::string cmd;
    if (file_exists("cookies.txt")) {
        cmd = "curl -s -b cookies.txt -c cookies.txt \"" + url + "\" -o " + tmpfile;
    }
    else {
        cmd = "curl -s \"" + url + "\" -o " + tmpfile;
    }

    int result = system(cmd.c_str());
    if (result != 0) return "";

    std::ifstream infile(tmpfile.c_str(), std::ios::in | std::ios::binary);
    std::ostringstream oss;
    char ch;
    while (infile.get(ch)) oss << ch;
    infile.close();
    return oss.str();
}

std::string extract_meaningful_content(const std::string& html) {
    std::ostringstream content;
    bool in_tag = false, in_script = false, in_style = false;
    std::string tag;

    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];
        if (c == '<') {
            in_tag = true;
            tag.clear();
        }

        if (in_tag) {
            tag += c;
            if (c == '>') {
                in_tag = false;
                std::string lower_tag = tag;
                std::transform(lower_tag.begin(), lower_tag.end(), lower_tag.begin(), ::tolower);
                if (lower_tag.find("<script") == 0) in_script = true;
                if (lower_tag.find("</script") == 0) in_script = false;
                if (lower_tag.find("<style") == 0) in_style = true;
                if (lower_tag.find("</style") == 0) in_style = false;
            }
        }
        else if (!in_script && !in_style) {
            content << c;
        }
    }

    std::string text = content.str();
    text.erase(std::remove_if(text.begin(), text.end(), [](char c) {
        return !isprint(c) && !isspace(c);
        }), text.end());

    std::string normalized;
    bool last_was_space = false;
    for (char c : text) {
        if (isspace(c)) {
            if (!last_was_space) {
                normalized += ' ';
                last_was_space = true;
            }
        }
        else {
            normalized += c;
            last_was_space = false;
        }
    }

    std::regex timestamp_regex(R"(\b\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2})?\b)");
    normalized = std::regex_replace(normalized, timestamp_regex, "");

    return normalized;
}

unsigned long simple_hash(const std::string& s) {
    unsigned long hash = 5381;
    for (char c : s)
        hash = ((hash << 5) + hash) + c;
    return hash;
}

std::string current_datetime_iso() {
    time_t now = time(0);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &t);
    return std::string(buffer);
}

std::string iso_to_human_readable(const std::string& iso) {
    if (iso == "Never checked before") return iso;

    std::tm tm = {};
    std::istringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return iso;

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%b %d, %Y %I:%M %p", &tm);
    return std::string(buffer);
}

std::map<std::string, unsigned long> load_signatures(const std::string& filename) {
    std::map<std::string, unsigned long> sigs;
    std::ifstream in(filename.c_str());
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string url;
        unsigned long hash;
        if (iss >> url >> hash)
            sigs[url] = hash;
    }
    return sigs;
}

void save_signatures(const std::string& filename, const std::map<std::string, unsigned long>& sigs) {
    std::ofstream out(filename.c_str());
    for (const auto& pair : sigs)
        out << pair.first << " " << pair.second << "\n";
}

std::map<std::string, std::string> load_times(const std::string& filename) {
    std::map<std::string, std::string> times;
    std::ifstream in(filename.c_str());
    std::string line;
    while (std::getline(in, line)) {
        size_t space = line.find(' ');
        if (space != std::string::npos) {
            std::string url = line.substr(0, space);
            std::string time = line.substr(space + 1);
            times[url] = time;
        }
    }
    return times;
}

void save_times(const std::string& filename, const std::map<std::string, std::string>& times) {
    std::ofstream out(filename.c_str());
    for (const auto& pair : times)
        out << pair.first << " " << pair.second << "\n";
}

std::vector<std::string> load_urls(const std::string& filename) {
    std::vector<std::string> urls;
    std::ifstream in(filename.c_str());
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (!line.empty())
            urls.push_back(line);
    }
    return urls;
}

int main(int argc, char* argv[]) {
    const std::string url_file = "websites.txt";
    const std::string sig_file = "signatures.txt";
    const std::string time_file = "timestamps.txt";
    const std::string tmp_file = "tmp_web_content.txt";

    if (argc >= 3) {
        std::string command = argv[1];
        std::string target_url = trim(argv[2]);

        std::vector<std::string> urls = load_urls(url_file);

        if (command == "add") {
            if (std::find(urls.begin(), urls.end(), target_url) != urls.end()) {
                std::cout << "URL already exists in " << url_file << "\n";
            }
            else {
                std::ofstream out(url_file, std::ios::app);
                out << target_url << "\n";
                std::cout << "Added URL to " << url_file << "\n";
            }
            return 0;
        }
        else if (command == "delete") {
            auto it = std::remove(urls.begin(), urls.end(), target_url);
            if (it == urls.end()) {
                std::cout << "URL not found in " << url_file << "\n";
            }
            else {
                urls.erase(it, urls.end());
                std::ofstream out(url_file);
                for (const auto& url : urls)
                    out << url << "\n";
                std::cout << "Deleted URL from " << url_file << "\n";
            }
            return 0;
        }
        else {
            std::cerr << "Unknown command: " << command << "\n";
            std::cerr << "Usage:\n";
            std::cerr << "  " << argv[0] << " add <url>\n";
            std::cerr << "  " << argv[0] << " delete <url>\n";
            return 1;
        }
    }

    std::vector<std::string> urls = load_urls(url_file);
    if (urls.empty()) {
        std::cout << "\nNo URLs in websites.txt\n\n";
        return 0;
    }

    std::map<std::string, unsigned long> old_sigs = load_signatures(sig_file);
    std::map<std::string, unsigned long> new_sigs;
    std::map<std::string, std::string> old_times = load_times(time_file);
    std::map<std::string, std::string> new_times;

    struct Result {
        std::string url;
        std::string status;
        std::string timestamp;
        bool timestamp_updated;
    };
    std::vector<Result> results;

    DotAnimator dots_anim;

    for (const auto& url : urls) {
        auto start_time = std::chrono::steady_clock::now();
        dots_anim.start("Checking " + url);
        std::string content = fetch_content(url, tmp_file);
        auto elapsed = std::chrono::steady_clock::now() - start_time;

        auto min_duration = std::chrono::milliseconds(1500);
        if (elapsed < min_duration)
            std::this_thread::sleep_for(min_duration - elapsed);

        dots_anim.stop();

        if (content.empty()) {
            std::string prev_time = old_times.count(url) ? old_times[url] : "Never checked before";
            results.push_back({ url, "Failed to retrieve content", prev_time, false });
            continue;
        }

        std::string cleaned = extract_meaningful_content(content);
        unsigned long hash = simple_hash(cleaned);
        new_sigs[url] = hash;
        std::string now_str = current_datetime_iso();

        bool is_new = old_sigs.find(url) == old_sigs.end();
        bool has_changed = !is_new && old_sigs[url] != hash;

        if (is_new) {
            new_times[url] = now_str;
            results.push_back({ url, "New site added", now_str, true });
        }
        else if (has_changed) {
            new_times[url] = now_str;
            results.push_back({ url, "Content changed", now_str, true });
        }
        else {
            std::string previous_time = old_times.count(url) ? old_times[url] : "Never checked before";
            new_times[url] = previous_time;
            results.push_back({ url, "No change", previous_time, false });
        }
    }

    save_signatures(sig_file, new_sigs);
    save_times(time_file, new_times);
    std::remove(tmp_file.c_str());

    std::sort(results.begin(), results.end(), [](const Result& a, const Result& b) {
        if (a.timestamp == "Never checked before") return false;
        if (b.timestamp == "Never checked before") return true;
        return a.timestamp > b.timestamp;
        });

    std::reverse(results.begin(), results.end());

    std::cout << "\nSites\n-----\n";
    for (const auto& res : results) {
        std::cout << res.url << " - ";
        if (res.status != "Failed to retrieve content") {
            std::string human_date = iso_to_human_readable(res.timestamp);
            if (res.timestamp_updated) {
                std::cout << "\033[1;32m" << human_date << "\033[0m";
            }
            else {
                std::cout << human_date;
            }
        }
        else {
            std::cout << res.status;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
    return 0;
}
