#pragma once

enum LogLevel
{
    LOG_INFO,
    LOG_WARN,
    LOG_ERR,
    LOG_ERRSILENT,
};

void dlog(enum LogLevel l, char fmt[], ...);
