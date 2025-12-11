#include "Logger.h"
#include <sstream>
#include <iomanip>

Logger& Logger::Instance() {
    static Logger ins;
    return ins;
}

Logger::Logger() {}

void Logger::CreateDirIfNotExist(const std::string& path) {
    CreateDirectoryA(path.c_str(), NULL);
}

void Logger::Init(const std::string& logDir,
    size_t maxFileSizeBytes,
    int retainDays,
    LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_logDir = logDir;
    m_maxSize = maxFileSizeBytes;
    m_retainDays = retainDays;
    m_level = level;

    CreateDirIfNotExist(m_logDir);

    m_currentDate = GetDateString();
    m_fileIndex = 0;

    CleanupOldLogs();
    RotateIfNeeded();
}

std::string Logger::GetDateString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[16];
    sprintf_s(buf, "%04d-%02d-%02d",
        st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::string Logger::BuildLogFileName() {
    std::ostringstream oss;
    oss << m_logDir << "\\"
        << m_currentDate << "_"
        << m_fileIndex << ".log";
    return oss.str();
}

void Logger::RotateIfNeeded() {
    if (!m_ofs.is_open()) {
        m_ofs.open(BuildLogFileName(), std::ios::app);
        return;
    }

    if ((size_t)m_ofs.tellp() >= m_maxSize) {
        m_ofs.close();
        ++m_fileIndex;
        m_ofs.open(BuildLogFileName(), std::ios::app);
    }
}

void Logger::CleanupOldLogs() {
    if (m_retainDays <= 0) return;

    std::string pattern = m_logDir + "\\*.log";

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);

    ULARGE_INTEGER now;
    now.LowPart = nowFt.dwLowDateTime;
    now.HighPart = nowFt.dwHighDateTime;

    const ULONGLONG maxAge = (ULONGLONG)m_retainDays
        * 24ULL * 60ULL * 60ULL * 10000000ULL;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        ULARGE_INTEGER ft;
        ft.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
        ft.HighPart = ffd.ftLastWriteTime.dwHighDateTime;

        ULONGLONG age = now.QuadPart - ft.QuadPart;

        if (age > maxAge) {
            std::string full = m_logDir + "\\" + ffd.cFileName;
            DeleteFileA(full.c_str());
        }

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
}

void Logger::Log(LogLevel level,
    const std::string& module,
    const std::string& msg) {
    if (level < m_level) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string today = GetDateString();
    if (today != m_currentDate) {
        if (m_ofs.is_open()) m_ofs.close();
        m_currentDate = today;
        m_fileIndex = 0;
    }

    RotateIfNeeded();

    SYSTEMTIME st;
    GetLocalTime(&st);
    char timebuf[64];

    sprintf_s(timebuf,
        "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    const char* levelStr =
        (level == LogLevel::DEBUG) ? "DEBUG" :
        (level == LogLevel::INFO) ? "INFO" :
        (level == LogLevel::WARN) ? "WARN" :
        "ERROR";

    m_ofs << "[" << timebuf << "] "
        << "[" << levelStr << "] "
        << "[" << module << "] "
        << msg << std::endl;
}

void Logger::LogHex(LogLevel level,
    const std::string& module,
    const std::string& prefix,
    const std::vector<uint8_t>& data) {
    if (level < m_level) return;

    std::ostringstream oss;
    oss << prefix << " LEN=" << data.size() << " HEX=";

    for (auto b : data) {
        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << (int)b << " ";
    }

    Log(level, module, oss.str());
}

std::string Logger::GetLastErrorString() {
    DWORD err = GetLastError();
    if (err == 0) return "No error";

    LPSTR buf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0,
        (LPSTR)&buf, 0, nullptr);

    std::string msg = buf ? buf : "Unknown error";
    if (buf) LocalFree(buf);
    return msg;
}

Logger::~Logger() {
    if (m_ofs.is_open()) {
        m_ofs.close();
    }
}
