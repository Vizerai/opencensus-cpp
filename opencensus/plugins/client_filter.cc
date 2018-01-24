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

#include "opencensus/plugins/client_filter.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "src/core/lib/surface/call.h"

namespace opencensus {

namespace {

void FilterTrailingMetadata(grpc_metadata_batch *b, uint64_t *elapsed_time) {
  if (b->idx.named.grpc_server_stats_bin != nullptr) {
    ServerStatsDeserialize(
        reinterpret_cast<const char *>(GRPC_SLICE_START_PTR(
            GRPC_MDVALUE(b->idx.named.grpc_server_stats_bin->md))),
        GRPC_SLICE_LENGTH(GRPC_MDVALUE(b->idx.named.grpc_server_stats_bin->md)),
        elapsed_time);
    grpc_metadata_batch_remove(b, b->idx.named.grpc_server_stats_bin);
  }
}

}  // namespace

void CensusClientCallData::OnDoneRecvTrailingMetadataCB(void *user_data,
                                                        grpc_error *error) {
  grpc_call_element *elem = reinterpret_cast<grpc_call_element *>(user_data);
  CensusClientCallData *calld =
      reinterpret_cast<CensusClientCallData *>(elem->call_data);
  GPR_ASSERT(calld != nullptr);
  if (error == GRPC_ERROR_NONE) {
    GPR_ASSERT(calld->recv_trailing_metadata_ != nullptr);
    FilterTrailingMetadata(calld->recv_trailing_metadata_,
                           &calld->elapsed_time_);
  }
  GRPC_CLOSURE_RUN(calld->initial_on_done_recv_trailing_metadata_,
                   GRPC_ERROR_REF(error));
}

void CensusClientCallData::StartTransportStreamOpBatch(
    grpc_call_element *elem, ::grpc::TransportStreamOpBatch *op) {
  if (op->send_initial_metadata() != nullptr) {
    char tracing_buf[kMaxTracingLen];
    size_t tracing_len =
        context_.TraceContextSerialize(tracing_buf, kMaxTracingLen);
    if (tracing_len > 0) {
      GRPC_LOG_IF_ERROR(
          "census grpc_filter",
          grpc_metadata_batch_add_tail(
              op->send_initial_metadata()->batch(), &tracing_bin_,
              grpc_mdelem_from_slices(
                  GRPC_MDSTR_GRPC_TRACE_BIN,
                  grpc_slice_from_copied_buffer(tracing_buf, tracing_len))));
    }
    char census_buf[kMaxStatsLen];
    size_t census_len =
        context_.StatsContextSerialize(census_buf, kMaxStatsLen);
    if (census_len > 0) {
      GRPC_LOG_IF_ERROR(
          "census grpc_filter",
          grpc_metadata_batch_add_tail(
              op->send_initial_metadata()->batch(), &stats_bin_,
              grpc_mdelem_from_slices(
                  GRPC_MDSTR_GRPC_TAGS_BIN,
                  grpc_slice_from_copied_buffer(census_buf, census_len))));
    }
  }

  if (op->recv_trailing_metadata() != nullptr) {
    GRPC_CLOSURE_INIT(&on_done_recv_trailing_metadata_,
                      OnDoneRecvTrailingMetadataCB, elem,
                      grpc_schedule_on_exec_ctx);
    recv_trailing_metadata_ = op->recv_trailing_metadata()->batch();
    initial_on_done_recv_trailing_metadata_ = op->on_complete();
    op->set_on_complete(&on_done_recv_trailing_metadata_);
  }
  // Call next op.
  grpc_call_next_op(elem, op->op());
}

grpc_error *CensusClientCallData::Init(grpc_call_element *elem,
                                       const grpc_call_element_args *args) {
  method_ = grpc_slice_ref_internal(args->path);
  const char *method_str =
      GPR_SLICE_IS_EMPTY(method_)
          ? ""
          : reinterpret_cast<const char *>(GRPC_SLICE_START_PTR(method_));
  method_size_ = GRPC_SLICE_IS_EMPTY(method_) ? 0 : GRPC_SLICE_LENGTH(method_);
  GenerateClientContext(absl::string_view(method_str, method_size_), &context_);
  return GRPC_ERROR_NONE;
}

void CensusClientCallData::Destroy(grpc_call_element *elem,
                                   const grpc_call_final_info *final_info,
                                   grpc_closure *then_call_closure) {
  // TODO: End span and record stats and tracing data.
  grpc_slice_unref_internal(method_);
}

}  // namespace opencensus
