#pragma once

#include "Core/Error.h"
#include "Core/LogSink.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace apex::core {

// One file per run: an existing file is truncated. Each line is flushed so a crash loses nothing.
class FileLogSink final : public LogSink {
public:
    // Creates the missing parent directories.
    [[nodiscard]] static Result<std::unique_ptr<FileLogSink>> open(const std::filesystem::path& path);

    explicit FileLogSink(std::ofstream stream);

    void write(const LogRecord& record) override;

private:
    std::ofstream m_stream;
};

} // namespace apex::core
