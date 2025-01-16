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

#ifndef NATIVE_WITH_STATE_INTERNAL_CURL_CONNECTOR_H_
#define NATIVE_WITH_STATE_INTERNAL_CURL_CONNECTOR_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native_with_state/internal/curl_request.h"
#include "native_with_state/internal/http_connector_interface.h"

namespace meet {

// Implementation of `HttpConnectorInterface` that uses `CurlRequest` to make
// HTTP requests.
class CurlConnector : public HttpConnectorInterface {
 public:
  explicit CurlConnector(std::unique_ptr<CurlApiWrapper> curl_api_wrapper)
      : curl_api_wrapper_(std::move(curl_api_wrapper)) {};

  absl::StatusOr<std::string> ConnectActiveConference(
      absl::string_view join_endpoint, absl::string_view conference_id,
      absl::string_view access_token, absl::string_view sdp_offer) override;

 private:
  std::unique_ptr<CurlApiWrapper> curl_api_wrapper_;
};

}  // namespace meet

#endif  // NATIVE_WITH_STATE_INTERNAL_CURL_CONNECTOR_H_
