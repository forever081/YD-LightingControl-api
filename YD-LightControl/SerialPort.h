#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <atomic>

class SerialPortManager {
public:
    static SerialPortManager& Instance();

    int OpenPort(const std::string& portName, int baudRate);
    void ClosePort(int handleId);

    bool Send(int handleId, const std::vector<uint8_t>& data);
    int  Receive(int handleId, std::vector<uint8_t>& outData);

private:
    struct PortContext {
        HANDLE hComm = INVALID_HANDLE_VALUE;
        std::string portName;
        std::mutex sendMutex;
        std::mutex recvMutex;
        std::atomic<bool> active{ false };
    };

    std::map<int, std::shared_ptr<PortContext>> m_ports;
    std::mutex m_mapMutex;
    std::atomic<int> m_nextId{ 1 };

    SerialPortManager() = default;
    ~SerialPortManager();
};
