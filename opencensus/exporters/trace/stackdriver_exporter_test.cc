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

#include "net/grpc/public/include/grpc++/grpc++.h"
#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
// #include "net/grpc/internal/test/core/util/test_init_google.h"
#include "opencensus/trace/exporter/local_span_store.h"
#include "opencensus/trace/span.h"

namespace opencensus {
namespace exporters {
namespace trace {

class StackdriverExporterTestPeer : public ::testing::Test {
 public:
  StackdriverExporterTestPeer() : handler_("e2e-debugging") {}

  void ExportSpans(
      const std::vector<::opencensus::trace::exporter::SpanData>& spans) {
    // TODO: Fix this test to work with mock handlers.  It cannot rely
    // on actually talking to stackdriver.
    // handler_.ExportForTesting("e2e-debugging", spans);
  }

 protected:
  StackdriverExporter handler_;
};

TEST_F(StackdriverExporterTestPeer, ExportTrace) {
  ::opencensus::trace::AlwaysSampler sampler;
  ::opencensus::trace::StartSpanOptions opts = {&sampler};

  auto span1 = ::opencensus::trace::Span::StartSpan("Span1", nullptr, opts);
  span1.AddAnnotation(
      "Root span",
      {{"str_attr", "hello"}, {"int_attr", 123}, {"bool_attr", true}});
  span1.AddAttribute("Number", 123);
  span1.AddReceivedMessageEvent(3, 4, 5);
  absl::SleepFor(absl::Milliseconds(100));
  auto span2 = ::opencensus::trace::Span::StartSpan("Span2", &span1, opts);
  span2.AddAnnotation("First child span");
  span2.AddSentMessageEvent(0, 4, 5);
  absl::SleepFor(absl::Milliseconds(100));
  auto span3 = ::opencensus::trace::Span::StartSpan("Span3", &span2, opts);
  span3.AddAnnotation("Second child span");
  absl::SleepFor(absl::Milliseconds(100));
  span3.End();
  absl::SleepFor(absl::Milliseconds(100));
  span2.End();
  absl::SleepFor(absl::Milliseconds(100));
  span1.End();
  std::vector<::opencensus::trace::exporter::SpanData> spans =
      ::opencensus::trace::exporter::LocalSpanStore::GetSpans();

  ExportSpans(spans);
}

}  // namespace trace
}  // namespace exporters
}  // namespace opencensus
