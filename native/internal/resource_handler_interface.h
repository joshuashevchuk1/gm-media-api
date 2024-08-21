/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NATIVE_INTERNAL_RESOURCE_HANDLER_INTERFACE_H_
#define NATIVE_INTERNAL_RESOURCE_HANDLER_INTERFACE_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace meet {
// A special type that is used to indicate that a particular resource does
// not expect and/or support requests from the remote client to Meet servers.
struct NoResourceRequestsFromClient {};

// Interface used by ConferenceResourceDataChannel instances to parse JSON
// resource updates and client requests.
//
// It exposes a method to be invoked when an update is received by the
// ConferenceResourceDataChannel that owns it. It is provided an
// absl::string_view of the json update. It is expected to return the parsed
// object or an absl::Status error to be propagated to the client.
//
// It also exposes a method to be invoked when a client request is to be sent to
// Meet servers. It is expected to return the serialized JSON request
// or an absl::Status error to be propagated to the client.
//
// See `ConferenceResourceDataChannel` for more usage details.
template <typename ToClientUpdate, typename FromClientRequest>
class ResourceHandlerInterface {
 public:
  virtual ~ResourceHandlerInterface() = default;

  virtual absl::StatusOr<ToClientUpdate> ParseUpdate(
      absl::string_view update) = 0;

  virtual absl::StatusOr<std::string> Stringify(
      const FromClientRequest& client_request) = 0;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_RESOURCE_HANDLER_INTERFACE_H_
