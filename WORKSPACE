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

workspace(name = "io_opencensus_cpp")

# We depend on Abseil.
http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-master",
    urls = ["https://github.com/abseil/abseil-cpp/archive/master.zip"],
)

# CCTZ (Time-zone framework). Used by absl.
http_archive(
    name = "com_googlesource_code_cctz",
    strip_prefix = "cctz-master",
    urls = ["https://github.com/google/cctz/archive/master.zip"],
)

# GoogleTest/GoogleMock framework.
http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-master",
    urls = ["https://github.com/google/googletest/archive/master.zip"],
)

# Grpc
http_archive(
    name = "com_github_grpc_grpc",
    urls = ["https://github.com/grpc/grpc/archive/master.zip"],
    strip_prefix = "grpc-master"
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

bind(
    name = "grpc++",
    actual = "@com_github_grpc_grpc//:grpc++",
)

bind(
    name = "grpc++_codegen_proto",
    actual = "@com_github_grpc_grpc//:grpc++_codegen_proto",
)

bind(
    name = "grpc_cpp_plugin",
    actual = "@com_github_grpc_grpc//:grpc_cpp_plugin",
)

# Protobuf
bind(
    name = "protobuf",
    actual = "@com_google_protobuf//:protobuf",
)

bind(
    name = "protobuf_clib",
    actual = "@com_google_protobuf//:protoc_lib",
)

bind(
    name = "protocol_compiler",
    actual = "@com_google_protobuf//:protoc",
)

# Google APIs
#new_http_archive(
#    name = "com_google_apis",
#    strip_prefix = "googleapis-master",
#    urls = ["https://github.com/googleapis/googleapis/archive/master.zip"],
#    build_file_content =
#"""
#load("@com_github_grpc_grpc//bazel:grpc_build_system.bzl", "grpc_proto_library")
#
#proto_library(
#  name = "tracing_proto",
#  srcs = [
#    "google/devtools/cloudtrace/v2/tracing.proto",
#    "google/devtools/cloudtrace/v2/trace.proto",
#    "google/api/annotations.proto",
#    "google/api/http.proto",
#    "google/rpc/status.proto",
#  ],
#  deps = ["@com_google_protobuf//:any_proto",
#          "@com_google_protobuf//:descriptor_proto",
#          "@com_google_protobuf//:timestamp_proto",
#          "@com_google_protobuf//:wrappers_proto",
#          "@com_google_protobuf//:empty_proto",],
#)
#
#grpc_proto_library(
#    name = "http_proto",
#    srcs = ["google/api/http.proto",],
#    use_external = True,
#)
#"""
#)

# Google Benchmark library.
# Adapted from cctz's WORKSPACE.
# Upstream support for bazel is tracked in
#  - https://github.com/google/benchmark/pull/329
#  - https://github.com/google/benchmark/issues/191
new_http_archive(
    name = "com_google_benchmark",
    urls = ["https://github.com/google/benchmark/archive/master.zip"],
    strip_prefix = "benchmark-master",
    build_file_content =
"""
cc_library(
    name = "benchmark",
    srcs = glob([
        "src/*.h",
        "src/*.cc",
    ]),
    hdrs = glob(["include/benchmark/*.h"]),
    copts = ["-DHAVE_POSIX_REGEX"],  # HAVE_STD_REGEX didn't work.
    includes = ["include"],
    visibility = ["//visibility:public"],
)
"""
)
