// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef ENABLE_LOGS_PREVIEW
#  include "opentelemetry/exporters/ostream/log_record_exporter.h"
#  include "opentelemetry/exporters/ostream/common_utils.h"
#  include "opentelemetry/sdk/logs/read_write_log_record.h"
#  include "opentelemetry/sdk_config.h"

#  include <iostream>
#  include <mutex>
#  include <type_traits>

namespace nostd     = opentelemetry::nostd;
namespace sdklogs   = opentelemetry::sdk::logs;
namespace sdkcommon = opentelemetry::sdk::common;
OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace logs
{

/*********************** Constructor ***********************/

OStreamLogRecordExporter::OStreamLogRecordExporter(std::ostream &sout) noexcept : sout_(sout) {}

/*********************** Exporter methods ***********************/

std::unique_ptr<sdklogs::Recordable> OStreamLogRecordExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<sdklogs::Recordable>(new sdklogs::ReadWriteLogRecord());
}

sdk::common::ExportResult OStreamLogRecordExporter::Export(
    const nostd::span<std::unique_ptr<sdklogs::Recordable>> &records) noexcept
{
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[Ostream Log Exporter] Exporting "
                            << records.size() << " log(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }

  for (auto &record : records)
  {
    auto log_record = std::unique_ptr<sdklogs::ReadWriteLogRecord>(
        static_cast<sdklogs::ReadWriteLogRecord *>(record.release()));

    if (log_record == nullptr)
    {
      // TODO: Log Internal SDK error "recordable data was lost"
      continue;
    }

    // Convert trace, spanid, traceflags into exportable representation
    constexpr int trace_id_len    = 32;
    constexpr int span_id__len    = 16;
    constexpr int trace_flags_len = 2;

    char trace_id[trace_id_len]       = {0};
    char span_id[span_id__len]        = {0};
    char trace_flags[trace_flags_len] = {0};

    log_record->GetTraceId().ToLowerBase16(trace_id);
    log_record->GetSpanId().ToLowerBase16(span_id);
    log_record->GetTraceFlags().ToLowerBase16(trace_flags);

    // Print out each field of the log record, noting that severity is separated
    // into severity_num and severity_text
    sout_ << "{\n"
          << "  timestamp          : " << log_record->GetTimestamp().time_since_epoch().count()
          << "\n"
          << "  observed_timestamp : "
          << log_record->GetObservedTimestamp().time_since_epoch().count() << "\n"
          << "  severity_num       : " << static_cast<std::uint32_t>(log_record->GetSeverity())
          << "\n"
          << "  severity_text      : ";

    std::uint32_t severity_index = static_cast<std::uint32_t>(log_record->GetSeverity());
    if (severity_index >= std::extent<decltype(opentelemetry::logs::SeverityNumToText)>::value)
    {
      sout_ << "Invalid severity(" << severity_index << ")\n";
    }
    else
    {
      sout_ << opentelemetry::logs::SeverityNumToText[severity_index] << "\n";
    }

    sout_ << "  body               : ";
    opentelemetry::exporter::ostream_common::print_value(log_record->GetBody(), sout_);
    sout_ << "\n  resource           : ";
    printAttributes(log_record->GetResource().GetAttributes(), "\n    ");

    sout_ << "\n  attributes         : ";

    printAttributes(log_record->GetAttributes(), "\n    ");

    sout_ << "\n"
          << "  trace_id           : " << std::string(trace_id, trace_id_len) << "\n"
          << "  span_id            : " << std::string(span_id, span_id__len) << "\n"
          << "  trace_flags        : " << std::string(trace_flags, trace_flags_len) << "\n"
          << "  scope              : \n"
          << "    name             : " << log_record->GetInstrumentationScope().GetName() << "\n"
          << "    version          : " << log_record->GetInstrumentationScope().GetVersion() << "\n"
          << "    schema_url       : " << log_record->GetInstrumentationScope().GetSchemaURL()
          << "\n"
          << "    attributes       : ";

    printAttributes(log_record->GetInstrumentationScope().GetAttributes(), "\n      ");
    sout_ << "\n}\n";
  }

  return sdk::common::ExportResult::kSuccess;
}

bool OStreamLogRecordExporter::ForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  sout_.flush();
  return true;
}

bool OStreamLogRecordExporter::Shutdown(std::chrono::microseconds) noexcept
{
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  is_shutdown_ = true;
  return true;
}

bool OStreamLogRecordExporter::isShutdown() const noexcept
{
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  return is_shutdown_;
}

void OStreamLogRecordExporter::printAttributes(
    const std::unordered_map<std::string, sdkcommon::OwnedAttributeValue> &map,
    const std::string prefix)
{
  for (const auto &kv : map)
  {
    sout_ << prefix << kv.first << ": ";
    opentelemetry::exporter::ostream_common::print_value(kv.second, sout_);
  }
}

void OStreamLogRecordExporter::printAttributes(
    const std::unordered_map<std::string, opentelemetry::common::AttributeValue> &map,
    const std::string prefix)
{
  for (const auto &kv : map)
  {
    sout_ << prefix << kv.first << ": ";
    opentelemetry::exporter::ostream_common::print_value(kv.second, sout_);
  }
}

}  // namespace logs
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
#endif
