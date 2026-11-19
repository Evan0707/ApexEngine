#include "Core/ConsoleLogSink.h"

#include <cstdio>
#include <string>

namespace apex::core {

void ConsoleLogSink::write(const LogRecord& record) {
    std::FILE* stream = record.level >= LogLevel::Warning ? stderr : stdout;
    std::string line = formatRecord(record);
    line.push_back('\n');
    std::fputs(line.c_str(), stream);
}

} // namespace apex::core
