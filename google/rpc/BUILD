# OpenCensus C++ exporter libraries.
# See ../../README.md for details.
#
# Copyright 2018, OpenCensus Authors
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
    name = "rpc_proto_lib",
    srcs = [
        "status.grpc.pb.cc",
        "status.pb.cc",
    ],
    hdrs = [
        "status.grpc.pb.h",
        "status.pb.h",
    ],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_protobuf//:protobuf",
    ],
)
