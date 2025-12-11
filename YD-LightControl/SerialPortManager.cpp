#include "SerialPort.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include "LoggerMacro.h"

#define MODULE_NAME "SerialPort"

static std::string ToHexString(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (auto b : data) {
        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << (int)b << " ";
    }
    return oss.str();
}

SerialPortManager& SerialPortManager::Instance() {
    static SerialPortManager ins;
    return ins;
}

int SerialPortManager::OpenPort(const std::string& portName, int baudRate) {
    std::string fullName = "\\\\.\\" + portName;

    HANDLE h = CreateFileA(
        fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        LOG_ERROR(MODULE_NAME,
            "Open fail " + portName +
            " err=" + Logger::Instance().GetLastErrorString());
        return -1;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(h, &dcb);
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(h, &dcb);

    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = 10;
    to.ReadTotalTimeoutConstant = 20;
    SetCommTimeouts(h, &to);

    auto ctx = std::make_shared<PortContext>();
    ctx->hComm = h;
    ctx->portName = portName;
    ctx->active = true;

    int handleId = m_nextId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(m_mapMutex);
        m_ports[handleId] = ctx;
    }

    LOG_INFO(MODULE_NAME,
        "Open ok " + portName +
        " baud=" + std::to_string(baudRate));
    return handleId;
}

void SerialPortManager::ClosePort(int handleId) {
    std::shared_ptr<PortContext> ctx;

    {
        std::lock_guard<std::mutex> lk(m_mapMutex);
        auto it = m_ports.find(handleId);
        if (it == m_ports.end()) return;
        ctx = it->second;
        m_ports.erase(it);
    }

    ctx->active = false;
    CloseHandle(ctx->hComm);

    LOG_INFO(MODULE_NAME,
        "Close " + ctx->portName);
}

bool SerialPortManager::Send(int handleId,
    const std::vector<uint8_t>& data) {
    std::shared_ptr<PortContext> ctx;
    {
        std::lock_guard<std::mutex> lk(m_mapMutex);
        auto it = m_ports.find(handleId);
        if (it == m_ports.end()) return false;
        ctx = it->second;
    }

    std::lock_guard<std::mutex> slk(ctx->sendMutex);

    if (!ctx->active) return false;

    DWORD wrote = 0;
    BOOL ok = WriteFile(ctx->hComm,
        data.data(),
        (DWORD)data.size(),
        &wrote, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (!ok || wrote != data.size()) {
        LOG_ERROR(MODULE_NAME,
            "Send fail " + ctx->portName +
            " HEX=" + ToHexString(data) +
            " err=" + Logger::Instance().GetLastErrorString());
        return false;
    }

    LOG_HEX(MODULE_NAME, "TX", data);
    return true;
}

int SerialPortManager::Receive(int handleId,
    std::vector<uint8_t>& outData) {
    std::shared_ptr<PortContext> ctx;
    {
        std::lock_guard<std::mutex> lk(m_mapMutex);
        auto it = m_ports.find(handleId);
        if (it == m_ports.end()) return -1;
        ctx = it->second;
    }

    std::lock_guard<std::mutex> rlk(ctx->recvMutex);

    if (!ctx->active) return -1;

    outData.clear();
    uint8_t b = 0;
    DWORD r = 0;
    DWORD start = GetTickCount();

    while (ctx->active) {
        BOOL ok = ReadFile(ctx->hComm, &b, 1, &r, nullptr);
        if (ok && r == 1) {
            outData.push_back(b);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (GetTickCount() - start > 20) break;
    }

    if (!outData.empty()) {
        LOG_HEX(MODULE_NAME, "RX", outData);
    }
    else {
        LOG_WARN(MODULE_NAME, "RX timeout " + ctx->portName);
    }

    return (int)outData.size();
}

SerialPortManager::~SerialPortManager() {
    std::lock_guard<std::mutex> lk(m_mapMutex);
    for (auto& kv : m_ports) {
        CloseHandle(kv.second->hComm);
    }
}
