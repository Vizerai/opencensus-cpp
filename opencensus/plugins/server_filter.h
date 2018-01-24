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

#ifndef OPENCENSUS_PLUGINS_SERVER_FILTER_H_
#define OPENCENSUS_PLUGINS_SERVER_FILTER_H_

#include "absl/strings/string_view.h"
#include "opencensus/plugins/channel_filter.h"
#include "opencensus/plugins/filter.h"
#include "include/grpc/grpc_security.h"

namespace opencensus {

// A CallData class will be created for every grpc call within a channel. It is
// used to store data and methods specific to that call. CensusServerCallData is
// thread-compatible, however typically only 1 thread should be interacting with
// a call at a time.
class CensusServerCallData : public ::grpc::CallData {
 public:
  CensusServerCallData()
      : method_size_(0),
        gc_(nullptr),
        auth_context_(nullptr),
        recv_initial_metadata_(nullptr),
        initial_on_done_recv_initial_metadata_(nullptr),
        latency_(0),
        elapsed_time_(0) {
    memset(&census_bin_, 0, sizeof(grpc_linked_mdelem));
    memset(&method_, 0, sizeof(grpc_slice));
    memset(&on_done_recv_initial_metadata_, 0, sizeof(grpc_closure));
  }

  grpc_error *Init(grpc_call_element *elem,
                   const grpc_call_element_args *args) override;

  void Destroy(grpc_call_element *elem, const grpc_call_final_info *final_info,
               grpc_closure *then_call_closure) override;

  void StartTransportStreamOpBatch(grpc_call_element *elem,
                                   ::grpc::TransportStreamOpBatch *op) override;

  static void OnDoneRecvInitialMetadataCB(void *user_data, grpc_error *error);

 private:
  CensusContext context_;
  // server method
  grpc_slice method_;
  // server method size
  size_t method_size_;
  // Pointer to the grpc_call element
  grpc_call *gc_;
  // Authorization context for the call.
  grpc_auth_context *auth_context_;
  // Metadata element for census stats.
  grpc_linked_mdelem census_bin_;
  // recv callback
  grpc_metadata_batch *recv_initial_metadata_;
  grpc_closure *initial_on_done_recv_initial_metadata_;
  grpc_closure on_done_recv_initial_metadata_;
  // rpc latency and elapsed time in nanoseconds.
  uint64_t latency_;
  uint64_t elapsed_time_;
};

}  // namespace opencensus

#endif  // OPENCENSUS_PLUGINS_SERVER_FILTER_H_
