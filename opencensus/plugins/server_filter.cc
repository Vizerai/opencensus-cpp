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

#include "opencensus/plugins/server_filter.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "src/core/lib/surface/call.h"

namespace opencensus {

namespace {

// server metadata elements
struct ServerMetadataElements {
  grpc_slice path;
  grpc_slice tracing_slice;
  grpc_slice census_proto;
};

void FilterInitialMetadata(grpc_metadata_batch *b,
                           ServerMetadataElements *sml) {
  if (b->idx.named.path != nullptr) {
    sml->path = grpc_slice_ref_internal(GRPC_MDVALUE(b->idx.named.path->md));
  }
  if (b->idx.named.grpc_trace_bin != nullptr) {
    sml->tracing_slice =
        grpc_slice_ref_internal(GRPC_MDVALUE(b->idx.named.grpc_trace_bin->md));
    grpc_metadata_batch_remove(b, b->idx.named.grpc_trace_bin);
  }
  if (b->idx.named.grpc_tags_bin != nullptr) {
    sml->census_proto =
        grpc_slice_ref_internal(GRPC_MDVALUE(b->idx.named.grpc_tags_bin->md));
    grpc_metadata_batch_remove(b, b->idx.named.grpc_tags_bin);
  }
}

}  // namespace

void CensusServerCallData::OnDoneRecvInitialMetadataCB(void *user_data,
                                                       grpc_error *error) {
  grpc_call_element *elem = reinterpret_cast<grpc_call_element *>(user_data);
  CensusServerCallData *calld =
      reinterpret_cast<CensusServerCallData *>(elem->call_data);
  GPR_ASSERT(calld != nullptr);
  if (error == GRPC_ERROR_NONE) {
    grpc_metadata_batch *initial_metadata = calld->recv_initial_metadata_;
    GPR_ASSERT(initial_metadata != nullptr);
    ServerMetadataElements sml;
    sml.path = grpc_empty_slice();
    sml.tracing_slice = grpc_empty_slice();
    sml.census_proto = grpc_empty_slice();
    FilterInitialMetadata(initial_metadata, &sml);
    calld->method_ = grpc_slice_ref_internal(sml.path);
    const char *method_str = GRPC_SLICE_IS_EMPTY(calld->method_)
                                 ? ""
                                 : reinterpret_cast<const char *>(
                                       GRPC_SLICE_START_PTR(calld->method_));
    calld->method_size_ =
        GRPC_SLICE_IS_EMPTY(sml.path) ? 0 : GRPC_SLICE_LENGTH(sml.path) - 1;
    const char *tracing_str =
        GRPC_SLICE_IS_EMPTY(sml.tracing_slice)
            ? ""
            : reinterpret_cast<const char *>(
                  GRPC_SLICE_START_PTR(sml.tracing_slice));
    size_t tracing_str_len = GRPC_SLICE_IS_EMPTY(sml.tracing_slice)
                                 ? 0
                                 : GRPC_SLICE_LENGTH(sml.tracing_slice);
    const char *census_str = GRPC_SLICE_IS_EMPTY(sml.census_proto)
                                 ? ""
                                 : reinterpret_cast<const char *>(
                                       GRPC_SLICE_START_PTR(sml.census_proto));
    size_t census_str_len = GRPC_SLICE_IS_EMPTY(sml.census_proto)
                                ? 0
                                : GRPC_SLICE_LENGTH(sml.census_proto);

    // TODO: Setup GenerateServerContext().
    GenerateServerContext(
        absl::string_view(tracing_str, tracing_str_len),
        absl::string_view(census_str, census_str_len), absl::string_view("", 0),
        absl::string_view(method_str, calld->method_size_), &calld->context_);

    grpc_slice_unref_internal(sml.tracing_slice);
    grpc_slice_unref_internal(sml.census_proto);
    grpc_slice_unref_internal(sml.path);
    grpc_census_call_set_context(
        calld->gc_, reinterpret_cast<census_context *>(&calld->context_));
  }
  GRPC_CLOSURE_RUN(calld->initial_on_done_recv_initial_metadata_,
                   GRPC_ERROR_REF(error));
}

void CensusServerCallData::StartTransportStreamOpBatch(
    grpc_call_element *elem, ::grpc::TransportStreamOpBatch *op) {
  if (op->recv_initial_metadata() != nullptr) {
    // substitute our callback for the op callback
    recv_initial_metadata_ = op->recv_initial_metadata()->batch();
    initial_on_done_recv_initial_metadata_ = op->recv_initial_metadata_ready();
    op->set_recv_initial_metadata_ready(&on_done_recv_initial_metadata_);
  }

  // We need to record the time when the trailing metadata was sent to mark the
  // completeness of the request.
  if (op->send_trailing_metadata() != nullptr) {
    // TODO: Setup ServerStatsSerialize().
    char buf[kMaxServerStatsLen];
    size_t len = ServerStatsSerialize(0, buf, kMaxServerStatsLen);
    if (len > 0) {
      GRPC_LOG_IF_ERROR("census grpc_filter",
                        grpc_metadata_batch_add_tail(
                            op->send_trailing_metadata()->batch(), &census_bin_,
                            grpc_mdelem_from_slices(
                                GRPC_MDSTR_GRPC_SERVER_STATS_BIN,
                                grpc_slice_from_copied_buffer(buf, len))));
    }
  }
  // Call next op.
  grpc_call_next_op(elem, op->op());
}

grpc_error *CensusServerCallData::Init(grpc_call_element *elem,
                                       const grpc_call_element_args *args) {
  // TODO: Call initialization (start time, etc.).
  gc_ =
      grpc_call_from_top_element(grpc_call_stack_element(args->call_stack, 0));
  GRPC_CLOSURE_INIT(&on_done_recv_initial_metadata_,
                    OnDoneRecvInitialMetadataCB, elem,
                    grpc_schedule_on_exec_ctx);
  auth_context_ = grpc_call_auth_context(gc_);
  return GRPC_ERROR_NONE;
}

void CensusServerCallData::Destroy(grpc_call_element *elem,
                                   const grpc_call_final_info *final_info,
                                   grpc_closure *then_call_closure) {
  // TODO: End span and record stats and tracing data.
  grpc_auth_context_release(auth_context_);
  grpc_slice_unref_internal(method_);
}

}  // namespace opencensus
