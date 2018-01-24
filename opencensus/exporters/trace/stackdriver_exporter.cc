// Copyright 2017, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "opencensus/exporters/trace/stackdriver_exporter.h"

#include <cstdint>
#include <iostream>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/time/clock.h"

using grpc::ClientContext;
using grpc::Status;

namespace opencensus {
namespace exporters {
namespace trace {

static constexpr size_t kAttributeStringLen = 256;
static constexpr size_t kAnnotationStringLen = 256;
static constexpr size_t kDisplayNameStringLen = 128;
static constexpr char kGoogleStackDriverTraceAddress[] =
    "cloudtrace.googleapis.com";

namespace {

gpr_timespec ConvertToTimespec(absl::Time time) {
  gpr_timespec g_time;
  int64_t secs = absl::ToUnixSeconds(time);
  g_time.tv_sec = secs;
  g_time.tv_nsec = (time - absl::FromUnixSeconds(secs)) / absl::Nanoseconds(1);
  return g_time;
}

bool Validate(const ::google::protobuf::Timestamp& t) {
  const auto sec = t.seconds();
  const auto ns = t.nanos();
  // sec must be [0001-01-01T00:00:00Z, 9999-12-31T23:59:59.999999999Z]
  if (sec < -62135596800 || sec > 253402300799) {
    return false;
  }
  if (ns < 0 || ns > 999999999) {
    return false;
  }
  return true;
}

bool EncodeTimestampProto(absl::Time t, ::google::protobuf::Timestamp* proto) {
  const int64_t s = absl::ToUnixSeconds(t);
  proto->set_seconds(s);
  proto->set_nanos((t - absl::FromUnixSeconds(s)) / absl::Nanoseconds(1));
  return Validate(*proto);
}

void SetTruncatableString(
    absl::string_view str, size_t max_len,
    ::google::devtools::cloudtrace::v2::TruncatableString* t_str) {
  if (str.size() > max_len) {
    auto truncated_str = str.substr(0, max_len);
    t_str->set_value(truncated_str.data(), truncated_str.length());
    t_str->set_truncated_byte_count(str.size() - max_len);
  } else {
    t_str->set_value(str.data(), str.length());
    t_str->set_truncated_byte_count(0);
  }
}

::google::devtools::cloudtrace::v2::Span_Link_Type ConvertLinkType(
    ::opencensus::trace::exporter::Link::Type type) {
  switch (type) {
    case ::opencensus::trace::exporter::Link::Type::kChildLinkedSpan:
      return ::google::devtools::cloudtrace::v2::
          Span_Link_Type_CHILD_LINKED_SPAN;
    case ::opencensus::trace::exporter::Link::Type::kParentLinkedSpan:
      return ::google::devtools::cloudtrace::v2::
          Span_Link_Type_PARENT_LINKED_SPAN;
  }
  return ::google::devtools::cloudtrace::v2::Span_Link_Type_TYPE_UNSPECIFIED;
}

::google::devtools::cloudtrace::v2::Span_TimeEvent_MessageEvent_Type
ConvertMessageType(::opencensus::trace::exporter::MessageEvent::Type type) {
  using Type = ::opencensus::trace::exporter::MessageEvent::Type;
  switch (type) {
    case Type::SENT:
      return ::google::devtools::cloudtrace::v2::
          Span_TimeEvent_MessageEvent_Type_SENT;
    case Type::RECEIVED:
      return ::google::devtools::cloudtrace::v2::
          Span_TimeEvent_MessageEvent_Type_RECEIVED;
  }
  return ::google::devtools::cloudtrace::v2::
      Span_TimeEvent_MessageEvent_Type_TYPE_UNSPECIFIED;
}

// oc:google-replace-begin(std::string vs string problem)
// TODO: Fix this to work in OSS where we need std::string.
using AttributeMap =
    ::google::protobuf::Map<std::string, ::google::devtools::cloudtrace::v2::AttributeValue>;
/* oc:oss-replace with
using AttributeMap = ::google::protobuf::Map<std::string,
    ::google::devtools::cloudtrace::v2::AttributeValue>;
oc:oss-replace-end */
void PopulateAttributes(
    const std::unordered_map<
        std::string, ::opencensus::trace::exporter::AttributeValue>& attributes,
    AttributeMap* attribute_map) {
  for (const auto& attr : attributes) {
    using Type = ::opencensus::trace::exporter::AttributeValue::Type;
    switch (attr.second.type()) {
      case Type::kString:
        SetTruncatableString(
            attr.second.string_value(), kAttributeStringLen,
            (*attribute_map)[attr.first].mutable_string_value());
        break;
      case Type::kBool:
        (*attribute_map)[attr.first].set_bool_value(attr.second.bool_value());
        break;
      case Type::kInt:
        (*attribute_map)[attr.first].set_int_value(attr.second.int_value());
        break;
    }
  }
}

void ConvertAttributes(const ::opencensus::trace::exporter::SpanData& span,
                       ::google::devtools::cloudtrace::v2::Span* proto_span) {
  ::google::devtools::cloudtrace::v2::Span::Attributes attributes;
  PopulateAttributes(span.attributes(),
                     proto_span->mutable_attributes()->mutable_attribute_map());
  proto_span->mutable_attributes()->set_dropped_attributes_count(
      span.num_attributes_dropped());
}

void ConvertTimeEvents(const ::opencensus::trace::exporter::SpanData& span,
                       ::google::devtools::cloudtrace::v2::Span* proto_span) {
  for (const auto& annotation : span.annotations().events()) {
    auto event = proto_span->mutable_time_events()->add_time_event();

    // Encode Timestamp
    EncodeTimestampProto(annotation.timestamp(), event->mutable_time());

    // Populate annotation.
    SetTruncatableString(annotation.event().description(), kAnnotationStringLen,
                         event->mutable_annotation()->mutable_description());
    PopulateAttributes(annotation.event().attributes(),
                       event->mutable_annotation()
                           ->mutable_attributes()
                           ->mutable_attribute_map());
  }

  for (const auto& message : span.message_events().events()) {
    auto event = proto_span->mutable_time_events()->add_time_event();

    // Encode Timestamp
    EncodeTimestampProto(message.timestamp(), event->mutable_time());

    // Populate message event.
    event->mutable_message_event()->set_type(
        ConvertMessageType(message.event().type()));
    event->mutable_message_event()->set_id(message.event().id());
    event->mutable_message_event()->set_uncompressed_size_bytes(
        message.event().uncompressed_size());
    event->mutable_message_event()->set_compressed_size_bytes(
        message.event().compressed_size());
  }

  proto_span->mutable_time_events()->set_dropped_annotations_count(
      span.annotations().dropped_events_count());
  proto_span->mutable_time_events()->set_dropped_message_events_count(
      span.message_events().dropped_events_count());
}

void ConvertLinks(const ::opencensus::trace::exporter::SpanData& span,
                  ::google::devtools::cloudtrace::v2::Span* proto_span) {
  proto_span->mutable_links()->set_dropped_links_count(
      span.num_links_dropped());
  for (const auto& span_link : span.links()) {
    auto link = proto_span->mutable_links()->add_link();
    link->set_trace_id(span_link.trace_id().ToHex());
    link->set_span_id(span_link.span_id().ToHex());
    link->set_type(ConvertLinkType(span_link.type()));
    PopulateAttributes(
        span_link.attributes(),
        proto_span->mutable_attributes()->mutable_attribute_map());
  }
}

void ConvertSpans(
    const std::vector<::opencensus::trace::exporter::SpanData>& spans,
    absl::string_view project_id,
    ::google::devtools::cloudtrace::v2::BatchWriteSpansRequest* request) {
  for (const auto& from_span : spans) {
    auto to_span = request->add_spans();
    SetTruncatableString(from_span.name(), kDisplayNameStringLen,
                         to_span->mutable_display_name());
    to_span->set_name(absl::StrCat("projects/", project_id, "/traces/",
                                   from_span.context().trace_id().ToHex(),
                                   "/spans/",
                                   from_span.context().span_id().ToHex()));
    to_span->set_span_id(from_span.context().span_id().ToHex());
    to_span->set_parent_span_id(from_span.parent_span_id().ToHex());

    // The start time of the span.
    EncodeTimestampProto(from_span.start_time(), to_span->mutable_start_time());

    // The end time of the span.
    EncodeTimestampProto(from_span.end_time(), to_span->mutable_end_time());

    // Export Attributes
    ConvertAttributes(from_span, to_span);

    // Export Time Events.
    ConvertTimeEvents(from_span, to_span);

    // Export Links.
    ConvertLinks(from_span, to_span);

    // True if the parent is on a different process.
    to_span->mutable_same_process_as_parent_span()->set_value(
        !from_span.has_remote_parent());

    // The status of the span.
    to_span->mutable_status()->set_code(
        static_cast<int32_t>(from_span.status().CanonicalCode()));
    to_span->mutable_status()->set_message(from_span.status().error_message());
  }
}

}  // namespace

Status StackdriverExporter::TraceClient::BatchWriteSpans(
    const ::google::devtools::cloudtrace::v2::BatchWriteSpansRequest& request) {
  ::google::protobuf::Empty response;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors. Deadline set for 3000
  // milliseconds.
  ClientContext context;
  context.set_deadline(
      ConvertToTimespec(absl::Now() + absl::Milliseconds(3000)));

  // The actual RPC that sends the span information to Stackdriver.
  return stub_->BatchWriteSpans(&context, request, &response);
}

void StackdriverExporter::Register(absl::string_view project_id) {
  StackdriverExporter* exporter = new StackdriverExporter(project_id);
  auto creds = grpc::GoogleDefaultCredentials();
  auto channel = ::grpc::CreateChannel(kGoogleStackDriverTraceAddress, creds);
  exporter->trace_client_ = absl::make_unique<TraceClient>(channel);
  ::opencensus::trace::exporter::SpanExporter::RegisterHandler(
      absl::WrapUnique<::opencensus::trace::exporter::SpanExporter::Handler>(
          exporter));
}

void StackdriverExporter::Export(
    const std::vector<::opencensus::trace::exporter::SpanData>& spans) {
  ::google::devtools::cloudtrace::v2::BatchWriteSpansRequest request;
  request.set_name(absl::StrCat("projects/", project_id_));
  ConvertSpans(spans, project_id_, &request);

  Status status = trace_client_->BatchWriteSpans(request);
  // Act upon its status.
  if (!status.ok()) {
    // TODO: log error.
  }
}

void StackdriverExporter::ExportForTesting(
    absl::string_view project_id,
    const std::vector<::opencensus::trace::exporter::SpanData>& spans) {
  auto creds = grpc::GoogleDefaultCredentials();
  auto channel = ::grpc::CreateChannel(kGoogleStackDriverTraceAddress, creds);
  std::unique_ptr<StackdriverExporter::TraceClient> trace_client =
      absl::make_unique<TraceClient>(channel);

  ::google::devtools::cloudtrace::v2::BatchWriteSpansRequest request;
  request.set_name(absl::StrCat("projects/", project_id));
  ConvertSpans(spans, project_id, &request);

  Status status = trace_client->BatchWriteSpans(request);
  // Act upon its status.
  if (!status.ok()) {
    std::cerr << "BatchWriteSpans failed with code " << status.error_code()
              << ": " << status.error_message() << "\n";
  }
}

}  // namespace trace
}  // namespace exporters
}  // namespace opencensus
