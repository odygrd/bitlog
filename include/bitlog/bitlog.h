#pragma once

#include "frontend.h"

#define LOG_CONDITIONAL(logger, level, likelihood, log_format, ...)                                  \
  do                                                                                                 \
  {                                                                                                  \
    if (logger->should_log(level))                                                                   \
      likelihood                                                                                     \
      {                                                                                              \
        logger->template log<__FILE_NAME__, __FUNCTION__, __LINE__, level, log_format>(__VA_ARGS__); \
      }                                                                                              \
  } while (0)

#define LOG_TRACE_L3(logger, log_format, ...)                                                      \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::TraceL3, [[unlikely]], log_format, __VA_ARGS__)

#define LOG_TRACE_L2(logger, log_format, ...)                                                      \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::TraceL2, [[unlikely]], log_format, __VA_ARGS__)

#define LOG_TRACE_L1(logger, log_format, ...)                                                      \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::TraceL1, [[unlikely]], log_format, __VA_ARGS__)

#define LOG_DEBUG(logger, log_format, ...)                                                         \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::Debug, [[unlikely]], log_format, __VA_ARGS__)

#define LOG_INFO(logger, log_format, ...)                                                          \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::Info, [[likely]], log_format, __VA_ARGS__)

#define LOG_WARNING(logger, log_format, ...)                                                       \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::Warning, [[likely]], log_format, __VA_ARGS__)

#define LOG_ERROR(logger, log_format, ...)                                                         \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::Error, [[likely]], log_format, __VA_ARGS__)

#define LOG_CRITICAL(logger, log_format, ...)                                                      \
  LOG_CONDITIONAL(logger, bitlog::LogLevel::CRITICAL, [[likely]], log_format, __VA_ARGS__)
