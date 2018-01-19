# OpenCensus C++ exporter libraries.
# See ../../README.md for details.
#
# Copyright 2017, OpenCensus Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

licenses(["notice"])  # Apache License 2.0

package(default_visibility = ["//visibility:public"])

# Libraries
# ========================================================================= #

cc_library(
    name = "stdout_exporter",
    srcs = [
        "stdout_exporter.cc",
    ],
    hdrs = [
        "stdout_exporter.h",
    ],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//opencensus/trace",
    ],
)

cc_library(
    name = "stackdriver_exporter",
    srcs = [
        #"stackdriver_exporter.cc",
        "stdout_exporter.cc",
    ],
    hdrs = [
        #"stackdriver_exporter.h",
        "stdout_exporter.h",
    ],
    deps = [
        "@com_google_apis//:http_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//opencensus/trace",
    ],
)

# Tests
# ========================================================================= #

cc_test(
    name = "stdout_exporter_test",
    srcs = ["stdout_exporter_test.cc"],
    deps = [
        ":stdout_exporter",
        "@com_google_absl//absl/time",
        "//opencensus/trace",
        "@com_google_googletest//:gtest_main",
        "@com_google_benchmark//:benchmark",
    ],
)

#cc_test(
#    name = "stackdriver_exporter_test",
#    srcs = ["stackdriver_exporter_test.cc"],
#    deps = [
#        ":stackdriver_exporter",
#        "@com_github_grpc_grpc//:grpc++",
#        "//opencensus/trace",
#        "@com_google_googletest//:gtest_main",
#    ],
#)
