# OpenCensus C++ Plugin library.

licenses(["notice"])  # Apache License 2.0

package(default_visibility = ["//visibility:public"])

# Libraries
# ========================================================================= #

cc_library(
    name = "grpc_plugin",
    srcs = [
        "channel_filter.cc",
        "client_filter.cc",
        "filter.cc",
        "server_filter.cc",
    ],
    hdrs = [
        "channel_filter.h",
        "client_filter.h",
        "filter.h",
        "rpc-encoding.h",
        "server_filter.h",
    ],
    deps = [
        "@com_google_absl//absl/base:endian",
        "@com_google_absl//absl/strings",
        #"@com_github_grpc_grpc//grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc_base_c",
        "//opencensus/trace",
    ],
)

# Tests
# ========================================================================= #

#cc_test(
#    name = "grpc_plugin_end2end_test",
#    srcs = ["grpc_plugin_end2end_test.cc"],
#    deps = [
#        ":grpc_plugin",
#        "//testing/base/public:gunit",
#        "@com_github_grpc_grpc//grpc:gpr",
#        "@com_github_grpc_grpc//grpc:grpc++",
#        "@com_github_grpc_grpc//grpc:grpc_base_c",
#        "@com_github_grpc_grpc//grpc/test/core/end2end:cq_verifier",
#        "@com_github_grpc_grpc//grpc/test/core/end2end:end2end_tests",
#        "@com_github_grpc_grpc//grpc/test/core/util:grpc_test_util",
#        "@com_github_grpc_grpc//grpc/test/core/util:grpc_test_util_base",
#    ],
#)
