#pragma once

import bitlog;

#define BITLOG_LOG_CALL(format, ...)                                                               \
  do                                                                                               \
  {                                                                                                \
    bitlog::log<__FILE_NAME__, __PRETTY_FUNCTION__, __LINE__, format>(__VA_ARGS__);              \
  } while (0)

#define LOG_INFO(format, ...) BITLOG_LOG_CALL(format, __VA_ARGS__)