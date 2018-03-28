// Copyright 2018, OpenCensus Authors
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

#include "opencensus/exporters/trace/zipkin/zipkin_exporter.h"

#include "absl/time/clock.h"
#include "gtest/gtest.h"
#include "opencensus/trace/exporter/annotation.h"
#include "opencensus/trace/internal/local_span_store.h"
#include "opencensus/trace/span.h"

namespace opencensus {
namespace exporters {
namespace trace {

class ZipkinExporterTestPeer : public ::testing::Test {
 public:
  ZipkinExporterTestPeer() {}

  void Export(
      const std::vector<::opencensus::trace::exporter::SpanData> &spans) {
    ZipkinExporter::ExportForTesting(spans);
  }
};

TEST_F(ZipkinExporterTestPeer, ExportTrace) {
  ::opencensus::trace::AlwaysSampler sampler;
  ::opencensus::trace::StartSpanOptions opts = {&sampler};

  auto span1 = ::opencensus::trace::Span::StartSpan("Span1", nullptr, opts);
  absl::SleepFor(absl::Milliseconds(100));
  span1.AddAnnotation("Annotation1", {{"TestBool", true}});
  auto span2 = ::opencensus::trace::Span::StartSpan("Span2", &span1, opts);
  absl::SleepFor(absl::Milliseconds(200));
  span2.AddAnnotation("Annotation2",
                      {{"TestString", "Test"}, {"TestInt", 123}});
  auto span3 = ::opencensus::trace::Span::StartSpan("Span3", &span2, opts);
  span3.AddAttributes({{"key1", "value1"},
                       {"int_key", 123},
                       {"another_key", "another_value"},
                       {"bool_key", true}});
  absl::SleepFor(absl::Milliseconds(300));
  span3.End();
  span2.End();
  span1.End();
  std::vector<::opencensus::trace::exporter::SpanData> spans =
      ::opencensus::trace::exporter::LocalSpanStore::GetSpans();

  Export(spans);
}

}  // namespace trace
}  // namespace exporters
}  // namespace opencensus
