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

#include "opencensus/plugins/channel_filter.h"
#include "opencensus/plugins/client_filter.h"
#include "opencensus/plugins/server_filter.h"

#include "benchmark/benchmark.h"
#include "grpc/include/grpc/byte_buffer.h"
#include "grpc/include/grpc/support/host_port.h"
#include "grpc/src/core/lib/surface/channel_init.h"
#include "grpc/src/core/lib/transport/metadata_batch.h"
#include "grpc/test/core/end2end/cq_verifier.h"
#include "grpc/test/core/end2end/end2end_tests.h"
#include "grpc/test/core/util/port.h"
#include "gtest/gtest.h"

namespace {

struct FullstackFixtureData {
  char *localaddr;
};

static grpc_end2end_test_fixture chttp2_create_fixture_fullstack(
    grpc_channel_args *client_args, grpc_channel_args *server_args) {
  grpc_end2end_test_fixture f;
  int port = grpc_pick_unused_port_or_die();
  FullstackFixtureData *ffd = reinterpret_cast<FullstackFixtureData *>(
      gpr_malloc(sizeof(FullstackFixtureData)));
  memset(&f, 0, sizeof(f));

  gpr_join_host_port(&ffd->localaddr, "localhost", port);

  f.fixture_data = ffd;
  f.cq = grpc_completion_queue_create_for_next(nullptr);
  f.shutdown_cq = grpc_completion_queue_create_for_pluck(nullptr);

  return f;
}

static void chttp2_init_client_fullstack(grpc_end2end_test_fixture *f,
                                         grpc_channel_args *client_args) {
  FullstackFixtureData *ffd =
      reinterpret_cast<FullstackFixtureData *>(f->fixture_data);
  f->client =
      grpc_insecure_channel_create(ffd->localaddr, client_args, nullptr);
  GPR_ASSERT(f->client);
}

static void chttp2_init_server_fullstack(grpc_end2end_test_fixture *f,
                                         grpc_channel_args *server_args) {
  FullstackFixtureData *ffd =
      reinterpret_cast<FullstackFixtureData *>(f->fixture_data);
  if (f->server) {
    grpc_server_destroy(f->server);
  }
  f->server = grpc_server_create(server_args, nullptr);
  grpc_server_register_completion_queue(f->server, f->cq, nullptr);
  GPR_ASSERT(grpc_server_add_insecure_http2_port(f->server, ffd->localaddr));
  grpc_server_start(f->server);
}

static void chttp2_tear_down_fullstack(grpc_end2end_test_fixture *f) {
  FullstackFixtureData *ffd =
      reinterpret_cast<FullstackFixtureData *>(f->fixture_data);
  gpr_free(ffd->localaddr);
  gpr_free(ffd);
}

/******************************************************************************/

class GrpcPluginTest : public ::testing::Test {
 public:
  void *Tag(intptr_t t) const { return reinterpret_cast<void *>(t); }

  grpc_end2end_test_fixture BeginTest(grpc_end2end_test_config config,
                                      const char *test_name) {
    grpc_end2end_test_fixture f;
    gpr_log(GPR_INFO, "Running test: %s/%s", test_name, config.name);
    f = config.create_fixture(nullptr, nullptr);
    config.init_server(&f, nullptr);
    config.init_client(&f, nullptr);
    return f;
  }

  gpr_timespec NSecondsFromNow(int n) {
    return grpc_timeout_seconds_to_deadline(n);
  }

  void DrainCq(grpc_completion_queue *cq) {
    grpc_event ev;
    do {
      ev = grpc_completion_queue_next(cq, NSecondsFromNow(5), nullptr);
    } while (ev.type != GRPC_QUEUE_SHUTDOWN);
  }

  void ShutdownServer(grpc_end2end_test_fixture *f) {
    if (!f->server) return;
    grpc_server_shutdown_and_notify(f->server, f->shutdown_cq, Tag(1000));
    GPR_ASSERT(grpc_completion_queue_pluck(f->shutdown_cq, Tag(1000),
                                           NSecondsFromNow(5), nullptr)
                   .type == GRPC_OP_COMPLETE);
    grpc_server_destroy(f->server);
    f->server = nullptr;
  }

  void ShutdownClient(grpc_end2end_test_fixture *f) {
    if (!f->client) return;
    grpc_channel_destroy(f->client);
    f->client = nullptr;
  }

  void EndTest(grpc_end2end_test_fixture *f) {
    ShutdownServer(f);
    ShutdownClient(f);

    grpc_completion_queue_shutdown(f->cq);
    DrainCq(f->cq);
    grpc_completion_queue_destroy(f->cq);
    grpc_completion_queue_destroy(f->shutdown_cq);
  }
};

TEST_F(GrpcPluginTest, BasicClientServerTest) {
  grpc_end2end_test_config config = {"chttp2/fullstack",
                                     FEATURE_MASK_SUPPORTS_DELAYED_CONNECTION |
                                         FEATURE_MASK_SUPPORTS_CLIENT_CHANNEL |
                                         FEATURE_MASK_SUPPORTS_AUTHORITY_HEADER,
                                     chttp2_create_fixture_fullstack,
                                     chttp2_init_client_fullstack,
                                     chttp2_init_server_fullstack,
                                     chttp2_tear_down_fullstack};
  grpc_call *c;
  grpc_call *s;
  grpc_slice request_payload_slice =
      grpc_slice_from_copied_string("hello world");
  grpc_byte_buffer *request_payload =
      grpc_raw_byte_buffer_create(&request_payload_slice, 1);
  grpc_slice response_payload_slice = grpc_slice_from_copied_string("goodbye");
  grpc_byte_buffer *response_payload =
      grpc_raw_byte_buffer_create(&response_payload_slice, 1);
  grpc_byte_buffer *request_payload_recv = nullptr;
  grpc_byte_buffer *response_payload_recv = nullptr;

  grpc_end2end_test_fixture f = BeginTest(config, "grpc_plugin_test");
  cq_verifier *cqv = cq_verifier_create(f.cq);
  grpc_op ops[8];
  grpc_op *op;
  grpc_metadata_array initial_metadata_recv;
  grpc_metadata_array trailing_metadata_recv;
  grpc_metadata_array request_metadata_recv;
  grpc_call_details call_details;
  grpc_status_code status;
  grpc_call_error error;
  grpc_slice details;
  int was_cancelled = 2;

  gpr_timespec deadline = NSecondsFromNow(5);
  c = grpc_channel_create_call(
      f.client, nullptr, GRPC_PROPAGATE_DEFAULTS, f.cq,
      grpc_slice_from_static_string("/foo"),
      get_host_override_slice("foo.test.opencensus.fr", config), deadline,
      nullptr);
  GPR_ASSERT(c);

  grpc_metadata_array_init(&initial_metadata_recv);
  grpc_metadata_array_init(&trailing_metadata_recv);
  grpc_metadata_array_init(&request_metadata_recv);
  grpc_call_details_init(&call_details);

  memset(ops, 0, sizeof(ops));
  op = ops;
  op->op = GRPC_OP_SEND_INITIAL_METADATA;
  op->data.send_initial_metadata.count = 0;
  op->data.send_initial_metadata.metadata = nullptr;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_SEND_MESSAGE;
  op->data.send_message.send_message = request_payload;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_RECV_INITIAL_METADATA;
  op->data.recv_initial_metadata.recv_initial_metadata = &initial_metadata_recv;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_RECV_STATUS_ON_CLIENT;
  op->data.recv_status_on_client.trailing_metadata = &trailing_metadata_recv;
  op->data.recv_status_on_client.status = &status;
  op->data.recv_status_on_client.status_details = &details;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_RECV_MESSAGE;
  op->data.recv_message.recv_message = &response_payload_recv;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  error = grpc_call_start_batch(c, ops, static_cast<size_t>(op - ops), Tag(1),
                                nullptr);
  GPR_ASSERT(GRPC_CALL_OK == error);

  error =
      grpc_server_request_call(f.server, &s, &call_details,
                               &request_metadata_recv, f.cq, f.cq, Tag(101));
  GPR_ASSERT(GRPC_CALL_OK == error);

  CQ_EXPECT_COMPLETION(cqv, Tag(101), 1);
  cq_verify(cqv);

  memset(ops, 0, sizeof(ops));
  op = ops;
  op->op = GRPC_OP_RECV_MESSAGE;
  op->data.recv_message.recv_message = &request_payload_recv;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_SEND_INITIAL_METADATA;
  op->data.send_initial_metadata.count = 0;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_SEND_MESSAGE;
  op->data.send_message.send_message = response_payload;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_SEND_STATUS_FROM_SERVER;
  op->data.send_status_from_server.trailing_metadata_count = 0;
  op->data.send_status_from_server.status = GRPC_STATUS_UNIMPLEMENTED;
  grpc_slice status_string = grpc_slice_from_static_string("xyz");
  op->data.send_status_from_server.status_details = &status_string;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  op->op = GRPC_OP_RECV_CLOSE_ON_SERVER;
  op->data.recv_close_on_server.cancelled = &was_cancelled;
  op->flags = 0;
  op->reserved = nullptr;
  op++;
  error = grpc_call_start_batch(s, ops, static_cast<size_t>(op - ops), Tag(102),
                                nullptr);
  GPR_ASSERT(GRPC_CALL_OK == error);

  CQ_EXPECT_COMPLETION(cqv, Tag(102), 1);
  CQ_EXPECT_COMPLETION(cqv, Tag(1), 1);
  cq_verify(cqv);
  GPR_ASSERT(0 == grpc_slice_str_cmp(details, "xyz"));

  // TODO: check results...

  grpc_slice_unref(details);
  grpc_metadata_array_destroy(&initial_metadata_recv);
  grpc_metadata_array_destroy(&trailing_metadata_recv);
  grpc_metadata_array_destroy(&request_metadata_recv);
  grpc_call_details_destroy(&call_details);

  grpc_call_unref(s);
  grpc_call_unref(c);

  cq_verifier_destroy(cqv);

  grpc_byte_buffer_destroy(request_payload);
  grpc_byte_buffer_destroy(response_payload);
  grpc_byte_buffer_destroy(request_payload_recv);
  grpc_byte_buffer_destroy(response_payload_recv);

  EndTest(&f);
  config.tear_down_data(&f);
}

void RegisterFilters() {
  grpc::RegisterChannelFilter<opencensus::CensusChannelData,
                              opencensus::CensusClientCallData>(
      "opencensus_client", GRPC_CLIENT_CHANNEL, INT_MAX, nullptr);
  grpc::RegisterChannelFilter<opencensus::CensusChannelData,
                              opencensus::CensusServerCallData>(
      "opencensus_server", GRPC_SERVER_CHANNEL, INT_MAX, nullptr);
}

}  // namespace

int main(int argc, char **argv) {
  // oc-begin
  InitGoogle(argv[0], &argc, &argv, true);
  // oc-end

  RegisterFilters();
  grpc_test_init(argc, argv);
  grpc_init();

  int status = RUN_ALL_TESTS();

  grpc_shutdown();
  return status;
}
