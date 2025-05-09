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

#ifndef CPP_INTERNAL_SESSION_CONTROL_RESOURCE_HANDLER_H_
#define CPP_INTERNAL_SESSION_CONTROL_RESOURCE_HANDLER_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/internal/resource_handler_interface.h"

namespace meet {

// Parses and dispatches session control resource updates from Meet servers.
class SessionControlResourceHandler : public ResourceHandlerInterface {
 public:
  SessionControlResourceHandler() = default;
  ~SessionControlResourceHandler() override = default;

  absl::StatusOr<ResourceUpdate> ParseUpdate(absl::string_view update) override;

  absl::StatusOr<std::string> StringifyRequest(
      const ResourceRequest& request) override;

  // SessionControlResourceHandler is neither copyable nor movable.
  SessionControlResourceHandler(const SessionControlResourceHandler&) = delete;
  SessionControlResourceHandler& operator=(
      const SessionControlResourceHandler&) = delete;
};

}  // namespace meet

#endif  // CPP_INTERNAL_SESSION_CONTROL_RESOURCE_HANDLER_H_
