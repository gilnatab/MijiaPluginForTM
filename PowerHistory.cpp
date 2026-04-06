// PowerHistory.cpp - 功率历史记录实现
#include "pch.h"
#include "PowerHistory.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <fstream>

double PowerHistory::Now() {
    auto tp = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>(tp).count();
}

PowerHistory::PowerHistory() = default;

std::wstring PowerHistory::FormatLocalTimestamp(double timestamp) {
    std::time_t timeValue = static_cast<std::time_t>(timestamp);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);

    wchar_t buffer[32]{};
    wcsftime(buffer, _countof(buffer), L"%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

bool PowerHistory::TryParseLocalTimestamp(const std::wstring& text, double& timestamp) {
    std::tm localTime{};
    std::wistringstream iss(text);
    iss >> std::get_time(&localTime, L"%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return false;
    }

    std::time_t timeValue = std::mktime(&localTime);
    if (timeValue == static_cast<std::time_t>(-1)) {
        return false;
    }

    timestamp = static_cast<double>(timeValue);
    return true;
}

void PowerHistory::AddSample(double watts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    double ts = Now();

    PowerSample s{ ts, watts };
    m_realtime.push_back(s);
    while (m_realtime.size() > MAX_REALTIME)
        m_realtime.pop_front();

    // 按分钟聚合长期记录
    double curMin = std::floor(ts / 60.0);
    if (curMin > m_lastMinTs) {
        m_lastMinTs = curMin;
        m_longterm.push_back(s);
        m_pendingPersist.push_back(s);
        while (m_longterm.size() > MAX_LONG)
            m_longterm.pop_front();
    }
}

double PowerHistory::GetCurrentWatts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_realtime.empty()) return 0.0;
    return m_realtime.back().watts;
}

bool PowerHistory::HasData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_realtime.empty();
}

std::vector<PowerSample> PowerHistory::GetRecentSamples(int seconds) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double cutoff = Now() - seconds;
    std::vector<PowerSample> result;
    for (const auto& s : m_realtime)
        if (s.timestamp >= cutoff) result.push_back(s);
    return result;
}

std::vector<PowerSample> PowerHistory::GetLongSamples(int hours) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double cutoff = Now() - hours * 3600.0;
    std::vector<PowerSample> result;
    for (const auto& s : m_longterm)
        if (s.timestamp >= cutoff) result.push_back(s);
    return result;
}

PowerStats PowerHistory::CalcStats(const std::vector<PowerSample>& pts) const {
    PowerStats st;
    if (pts.empty()) return st;
    st.valid = true;
    st.count = pts.size();
    st.maxW  = pts[0].watts;
    st.minW  = pts[0].watts;
    double sum = 0;
    for (const auto& p : pts) {
        if (p.watts > st.maxW) st.maxW = p.watts;
        if (p.watts < st.minW) st.minW = p.watts;
        sum += p.watts;
    }
    st.avgW  = sum / pts.size();
    st.lastW = pts.back().watts;
    return st;
}

PowerStats PowerHistory::GetStats(int seconds) const {
    return CalcStats(GetRecentSamples(seconds));
}

PowerStats PowerHistory::GetLongStats(int hours) const {
    return CalcStats(GetLongSamples(hours));
}

// ─── 持久化（追加 CSV，无第三方库）────
void PowerHistory::SaveToFile(const std::wstring& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (filePath.empty()) return;
    if (m_pendingPersist.empty()) return;

    std::filesystem::path path(filePath);
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::error_code ec;
    bool writeHeader = !std::filesystem::exists(path, ec) || std::filesystem::file_size(path, ec) == 0;

    std::wofstream f(filePath, std::ios::app);
    if (!f.is_open()) return;

    if (writeHeader) {
        f << L"本地时间,功率(W)\r\n";
    }

    for (const auto& s : m_pendingPersist) {
        f << FormatLocalTimestamp(s.timestamp)
          << L"," << std::fixed << std::setprecision(2) << s.watts
          << L"\r\n";
    }

    if (!f.fail()) {
        m_pendingPersist.clear();
    }
}

void PowerHistory::LoadFromFile(const std::wstring& filePath) {
    if (filePath.empty()) {
        return;
    }

    std::wifstream f(filePath);
    if (!f.is_open()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_realtime.clear();
        m_longterm.clear();
        m_pendingPersist.clear();
        m_lastMinTs = 0.0;
        return;
    }

    const double now = Now();
    const double realtimeCutoff = now - 600.0;          // tooltip 最近10分钟
    const double longCutoff = now - 7 * 24 * 3600.0;    // 保留最近7天分钟级样本

    std::deque<PowerSample> loadedRealtime;
    std::deque<PowerSample> loadedLongterm;
    std::wstring line;
    bool firstLine = true;
    double lastMinTs = 0.0;

    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line == L"本地时间,功率(W)" || line == L"LocalTime,Watts") {
                continue;
            }
        }

        size_t commaPos = line.find(L',');
        if (commaPos == std::wstring::npos) {
            continue;
        }

        double timestamp = 0.0;
        if (!TryParseLocalTimestamp(line.substr(0, commaPos), timestamp)) {
            continue;
        }

        double watts = 0.0;
        try {
            watts = std::stod(line.substr(commaPos + 1));
        } catch (...) {
            continue;
        }

        PowerSample sample{ timestamp, watts };

        if (sample.timestamp >= realtimeCutoff) {
            loadedRealtime.push_back(sample);
            while (loadedRealtime.size() > MAX_REALTIME) {
                loadedRealtime.pop_front();
            }
        }

        if (sample.timestamp >= longCutoff) {
            loadedLongterm.push_back(sample);
            while (loadedLongterm.size() > MAX_LONG) {
                loadedLongterm.pop_front();
            }
            lastMinTs = std::floor(sample.timestamp / 60.0);
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_realtime = std::move(loadedRealtime);
    m_longterm = std::move(loadedLongterm);
    m_pendingPersist.clear();
    m_lastMinTs = lastMinTs;
}
