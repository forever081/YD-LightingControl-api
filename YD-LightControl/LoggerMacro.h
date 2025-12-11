#pragma once
#include "Logger.h"

#define LOG_DEBUG(module, msg) \
    Logger::Instance().Log(LogLevel::DEBUG, module, msg)

#define LOG_INFO(module, msg) \
    Logger::Instance().Log(LogLevel::INFO, module, msg)

#define LOG_WARN(module, msg) \
    Logger::Instance().Log(LogLevel::WARN, module, msg)

#define LOG_ERROR(module, msg) \
    Logger::Instance().Log(LogLevel::ERR, module, msg)

#define LOG_HEX(module, prefix, data) \
    Logger::Instance().LogHex(LogLevel::DEBUG, module, prefix, data)
