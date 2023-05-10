#pragma once

enum LogLevel
{
    LOG_INFO,
    LOG_WARN,
    LOG_ERR,
    LOG_ERRSILENT,
};

void log_force_errsilent();
void dlog(enum LogLevel l, char fmt[], ...);
