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

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return str.substr(first, last - first + 1);
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
    std::string cmd = "curl -s \"" + url + "\" -o " + tmpfile;
    int result = system(cmd.c_str());
    if (result != 0) return "";

    std::ifstream infile(tmpfile.c_str(), std::ios::in | std::ios::binary);
    std::ostringstream oss;
    char ch;
    while (infile.get(ch)) oss << ch;
    infile.close();
    return oss.str();
}

unsigned long simple_hash(const std::string& s) {
    unsigned long hash = 5381;
    for (size_t i = 0; i < s.size(); ++i)
        hash = ((hash << 5) + hash) + s[i];
    return hash;
}

std::string current_datetime() {
    time_t now = time(0);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    std::ostringstream oss;
    oss << std::put_time(&t, "%b %d, %Y %I:%M %p");
    return oss.str();
}

time_t parse_datetime(const std::string& datetime_str) {
    struct tm t = {};
    std::istringstream ss(datetime_str);
    ss >> std::get_time(&t, "%b %d, %Y %I:%M %p");
    if (ss.fail()) return 0;
    return mktime(&t);
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

void save_urls(const std::string& filename, const std::vector<std::string>& urls) {
    std::ofstream out(filename.c_str());
    for (const auto& url : urls)
        out << url << "\n";
}

std::string normalize_url(const std::string& url) {
    if (url.find("http://") == 0 || url.find("https://") == 0)
        return url;
    return "https://" + url;
}

int main(int argc, char* argv[]) {
    const std::string url_file = "websites.txt";
    const std::string sig_file = "signatures.txt";
    const std::string time_file = "timestamps.txt";
    const std::string tmp_file = "tmp_web_content.txt";

    if (argc > 1) {
        std::string cmd = argv[1];

        if ((cmd == "add" || cmd == "del") && argc == 3) {
            std::string raw_url = argv[2];
            std::string url = normalize_url(raw_url);
            std::vector<std::string> urls = load_urls(url_file);

            if (cmd == "add") {
                if (std::find(urls.begin(), urls.end(), url) == urls.end()) {
                    urls.push_back(url);
                    save_urls(url_file, urls);
                    std::cout << "\nAdded: " << url << std::endl << std::endl;
                } else {
                    std::cout << "\nURL already exists: " << url << std::endl << std::endl;
                }
            } else if (cmd == "del") {
                auto it = std::remove(urls.begin(), urls.end(), url);
                if (it != urls.end()) {
                    urls.erase(it, urls.end());
                    save_urls(url_file, urls);

                    std::map<std::string, unsigned long> sigs = load_signatures(sig_file);
                    sigs.erase(url);
                    save_signatures(sig_file, sigs);

                    std::map<std::string, std::string> times = load_times(time_file);
                    times.erase(url);
                    save_times(time_file, times);

                    std::cout << "\nDeleted: " << url << std::endl << std::endl;
                } else {
                    std::cout << "\nURL not found: " << url << std::endl << std::endl;
                }
            }
            return 0;
        }
    }

    std::vector<std::string> urls = load_urls(url_file);
    if (urls.empty()) {
        std::cout << "\nNo groups in website.txt" << std::endl << std::endl;
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
        if (elapsed < min_duration) {
            std::this_thread::sleep_for(min_duration - elapsed);
        }
        dots_anim.stop();

        if (content.empty()) {
            std::string prev_time = old_times.count(url) ? old_times[url] : "Never checked before";
            results.push_back({ url, "Failed to retrieve content", prev_time, false });
            continue;
        }

        unsigned long hash = simple_hash(content);
        new_sigs[url] = hash;
        std::string now_str = current_datetime();

        bool is_new = old_sigs.find(url) == old_sigs.end();
        bool has_changed = !is_new && old_sigs[url] != hash;

        if (is_new) {
            new_times[url] = now_str;
            results.push_back({ url, "New site added", now_str, true });
        } else if (has_changed) {
            new_times[url] = now_str;
            results.push_back({ url, "Content changed", now_str, true });
        } else {
            std::string previous_time = old_times.count(url) ? old_times[url] : "Never checked before";
            new_times[url] = previous_time;
            results.push_back({ url, "No change", previous_time, false });
        }
    }

    save_signatures(sig_file, new_sigs);
    save_times(time_file, new_times);
    std::remove(tmp_file.c_str());

    std::sort(results.begin(), results.end(), [](const Result& a, const Result& b) {
        if (a.timestamp == "Never checked before") return true;
        if (b.timestamp == "Never checked before") return false;
        return parse_datetime(a.timestamp) < parse_datetime(b.timestamp);
    });

    std::cout << std::endl << "Sites" << std::endl << "-----" << std::endl;

    for (const auto& res : results) {
        std::cout << res.url << " - ";
        if (res.status != "Failed to retrieve content") {
            if (res.timestamp_updated) {
                std::cout << "\033[1;32m" << res.timestamp << "\033[0m";
            } else {
                std::cout << res.timestamp;
            }
        } else {
            std::cout << res.status;
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
    return 0;
}
