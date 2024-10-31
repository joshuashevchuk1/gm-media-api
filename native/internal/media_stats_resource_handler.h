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

#ifndef NATIVE_INTERNAL_MEDIA_STATS_RESOURCE_HANDLER_H_
#define NATIVE_INTERNAL_MEDIA_STATS_RESOURCE_HANDLER_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native/api/media_stats_resource.h"
#include "native/internal/resource_handler_interface.h"

namespace meet {

class MediaStatsResourceHandler
    : public ResourceHandlerInterface<MediaStatsChannelToClient,
                                      MediaStatsChannelFromClient> {
 public:
  MediaStatsResourceHandler() = default;
  ~MediaStatsResourceHandler() override = default;

  // MediaStatsResourceHandler is neither copyable nor movable.
  MediaStatsResourceHandler(const MediaStatsResourceHandler&) = delete;
  MediaStatsResourceHandler& operator=(const MediaStatsResourceHandler&) =
      delete;

  absl::StatusOr<MediaStatsChannelToClient> ParseUpdate(
      absl::string_view update) override;

  absl::StatusOr<std::string> Stringify(
      const MediaStatsChannelFromClient& client_request) override;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_MEDIA_STATS_RESOURCE_HANDLER_H_
