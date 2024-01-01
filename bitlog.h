#pragma once

import bitlog;

#define BITLOG_LOG_CALL(level, format, ...)                                                        \
  do                                                                                               \
  {                                                                                                \
    bitlog::log<__FILE_NAME__, __PRETTY_FUNCTION__, __LINE__, level, format>(__VA_ARGS__);         \
  } while (0)

#define LOG_INFO(format, ...) BITLOG_LOG_CALL(bitlog::LogLevel::Info, format, __VA_ARGS__)