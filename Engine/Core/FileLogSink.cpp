#include "Core/FileLogSink.h"

#include "Core/Assert.h"

#include <format>
#include <system_error>
#include <utility>

namespace apex::core {

Result<std::unique_ptr<FileLogSink>> FileLogSink::open(const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return makeError(ErrorCode::IoFailure, std::format("Cannot create log directory '{}': {}",
                                                               path.parent_path().string(), error.message()));
        }
    }

    std::ofstream stream(path, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        return makeError(ErrorCode::IoFailure, std::format("Cannot open log file '{}'", path.string()));
    }
    return std::make_unique<FileLogSink>(std::move(stream));
}

FileLogSink::FileLogSink(std::ofstream stream) : m_stream(std::move(stream)) {
    APEX_ASSERT(m_stream.is_open(), "FileLogSink needs an open stream");
}

void FileLogSink::write(const LogRecord& record) {
    m_stream << formatRecord(record) << '\n';
    m_stream.flush();
}

} // namespace apex::core
