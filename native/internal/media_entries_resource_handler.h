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

#ifndef NATIVE_INTERNAL_MEDIA_ENTRIES_RESOURCE_HANDLER_H_
#define NATIVE_INTERNAL_MEDIA_ENTRIES_RESOURCE_HANDLER_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native/api/conference_resources.h"
#include "native/internal/resource_handler_interface.h"

namespace meet {

class MediaEntriesResourceHandler
    : public ResourceHandlerInterface<MediaEntriesChannelToClient,
                                      NoResourceRequestsFromClient> {
 public:
  MediaEntriesResourceHandler() = default;
  ~MediaEntriesResourceHandler() = default;

  absl::StatusOr<MediaEntriesChannelToClient> ParseUpdate(
      absl::string_view update) override;

  absl::StatusOr<std::string> Stringify(
      const NoResourceRequestsFromClient& client_request) override {
    return absl::UnimplementedError(
        "Media entries resourceHandler does not support requests from the "
        "client.");
  }

  // MediaEntriesResourceHandler is neither copyable nor movable.
  MediaEntriesResourceHandler(const MediaEntriesResourceHandler&) = delete;
  MediaEntriesResourceHandler& operator=(const MediaEntriesResourceHandler&) =
      delete;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_MEDIA_ENTRIES_RESOURCE_HANDLER_H_
