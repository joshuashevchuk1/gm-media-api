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

#include "native/internal/conference_resource_data_channel.h"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "native/api/media_entries_resource.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/participants_resource.h"
#include "native/api/session_control_resource.h"
#include "native/api/video_assignment_resource.h"
#include "native/internal/resource_handler_interface.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/rtc_base/copy_on_write_buffer.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {

namespace {
// Resource data channels are expected to be configured as reliable and ordered.
const webrtc::DataChannelInit kDataChannelConfig = {.reliable = true,
                                                    .ordered = true};
using enum webrtc::DataChannelInterface::DataState;

}  // namespace

template <typename ToClientUpdate, typename FromClientRequest>
absl::StatusOr<std::unique_ptr<
    ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>>>
ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>::Create(
    rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
        api_session_observer,
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection,
    absl::string_view data_channel_label,
    std::unique_ptr<ResourceHandlerInterface<ToClientUpdate, FromClientRequest>>
        resource_handler,
    rtc::Thread* worker_thread) {
  if (peer_connection == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to create data channel ", data_channel_label,
                     ": Provided peer connection is null."));
  }

  if (worker_thread == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to create data channel ", data_channel_label,
                     ": Provided worker thread is null."));
  }

  if (api_session_observer == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to create data channel ", data_channel_label,
                     ": Provided api session observer is null."));
  }

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      create_result = peer_connection->CreateDataChannelOrError(
          std::string(data_channel_label), &kDataChannelConfig);

  if (!create_result.ok()) {
    return absl::InternalError(absl::StrCat("Failed to create data channel ",
                                            data_channel_label, ": ",
                                            create_result.error().message()));
  }

  // Must be created on the worker thread to ensure the safety flag
  // member is created on the same thread all work will be processed on. See
  // `alive_flag_` member for more details.
  return worker_thread->BlockingCall([&]() {
    // Using `new` to access a non-public constructor.
    return absl::WrapUnique(
        new ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>(
            std::move(api_session_observer), create_result.MoveValue(),
            data_channel_label, std::move(resource_handler), worker_thread));
  });
}

template <typename ToClientUpdate, typename FromClientRequest>
void ConferenceResourceDataChannel<ToClientUpdate,
                                   FromClientRequest>::OnStateChange() {}

template <typename ToClientUpdate, typename FromClientRequest>
void ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>::
    OnMessage(const webrtc::DataBuffer& buffer) {
  // Offload work from network thread to worker thread
  worker_thread_.PostTask(SafeTask(alive_flag_, [this, buffer]() {
    // Meet servers should always send JSON updates.
    if (buffer.binary) {
      LOG(ERROR) << "Received unexpected binary " << data_channel_label_
                 << " update.";
      return;
    }

    absl::string_view message(buffer.data.cdata<char>(), buffer.size());
    absl::StatusOr<ToClientUpdate> update_parse_status =
        resource_handler_->ParseUpdate(message);
    if (!update_parse_status.ok()) {
      LOG(ERROR) << "Received " << label()
                 << " resource update but it failed to parse: "
                 << update_parse_status.status();
      return;
    }

    absl::StatusOr<MeetMediaApiSessionObserverInterface::ResourceUpdate>
        create_update_status =
            CreateResourceUpdate(*std::move(update_parse_status));
    if (!create_update_status.ok()) {
      LOG(ERROR) << "Received " << label()
                 << " resource update but failed to create ResourceUpdate: "
                 << create_update_status.status();
      return;
    }

    api_session_observer_->OnResourceUpdate(*std::move(create_update_status));
  }));
}

template <typename ToClientUpdate, typename FromClientRequest>
absl::Status
ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>::SendRequest(
    const FromClientRequest& request) {
  if (data_channel_ == nullptr) {
    return absl::InternalError(absl::StrCat(
        "Failed to send request on data channel ", data_channel_label_,
        ": Data channel is null. This should never happen yet here we are."));
  }

  if (webrtc::DataChannelInterface::DataState state = data_channel_->state();
      state != kOpen) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Failed to send request on data channel ", data_channel_label_,
        ". Data channel state ",
        webrtc::DataChannelInterface::DataStateString(state), " is not open."));
  }

  absl::StatusOr<std::string> stringify_status =
      resource_handler_->Stringify(request);
  if (!stringify_status.ok()) {
    return stringify_status.status();
  }

  data_channel_->SendAsync(
      webrtc::DataBuffer(*std::move(stringify_status)),
      [&](webrtc::RTCError error) {
        if (!error.ok()) {
          api_session_observer_->OnResourceRequestFailure({
              .hint = StringToResourceHint(label()),
              .request_id = ExtractRequestId(request),
              .status = absl::InternalError(
                  absl::StrCat("Failed to send request on data channel ",
                               data_channel_label_, ": ", error.message())),
          });
        }
      });

  return absl::OkStatus();
}

template <typename ToClientUpdate, typename FromClientRequest>
int64_t ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>::
    ExtractRequestId(const FromClientRequest& request) {
  // Only session control, video assignment, and stats resources accept requests
  // from the client.
  switch (StringToResourceHint(label())) {
    case ResourceHint::kSessionControl:
      if constexpr (std::is_same_v<ToClientUpdate,
                                   SessionControlChannelToClient>) {
        return request.request.request_id;
      }
      return 0;
    case ResourceHint::kVideoAssignment:
      if constexpr (std::is_same_v<ToClientUpdate,
                                   VideoAssignmentChannelToClient>) {
        return request.request.request_id;
      }
      return 0;
    default:
      return 0;
  }
}

template <typename ToClientUpdate, typename FromClientRequest>
absl::StatusOr<MeetMediaApiSessionObserverInterface::ResourceUpdate>
ConferenceResourceDataChannel<ToClientUpdate, FromClientRequest>::
    CreateResourceUpdate(const ToClientUpdate update) {
  switch (StringToResourceHint(label())) {
    case ResourceHint::kSessionControl:
      if constexpr (std::is_same_v<ToClientUpdate,
                                   SessionControlChannelToClient>) {
        return MeetMediaApiSessionObserverInterface::ResourceUpdate{
            .hint = ResourceHint::kSessionControl,
            .session_control_update = std::move(update)};
      }
      return absl::InvalidArgumentError(
          "Incorrect update type for SessionControl");
    case ResourceHint::kVideoAssignment:
      if constexpr (std::is_same_v<ToClientUpdate,
                                   VideoAssignmentChannelToClient>) {
        return MeetMediaApiSessionObserverInterface::ResourceUpdate{
            .hint = ResourceHint::kVideoAssignment,
            .video_assignment_update = std::move(update)};
      }
      return absl::InvalidArgumentError(
          "Incorrect update type for VideoAssignment");
    case ResourceHint::kMediaEntries:
      if constexpr (std::is_same_v<ToClientUpdate,
                                   MediaEntriesChannelToClient>) {
        return MeetMediaApiSessionObserverInterface::ResourceUpdate{
            .hint = ResourceHint::kMediaEntries,
            .media_entries_update = std::move(update)};
      }
      return absl::InvalidArgumentError(
          "Incorrect update type for MediaEntries");
    case ResourceHint::kParticipants:
      if constexpr (std::is_same_v<ToClientUpdate,
                                   ParticipantsChannelToClient>) {
        return MeetMediaApiSessionObserverInterface::ResourceUpdate{
            .hint = ResourceHint::kParticipants,
            .participants_update = std::move(update)};
      }
      return absl::InvalidArgumentError(
          "Incorrect update type for Participants");

    // TDOO: b/347055783 - Add support for Stats resource updates once they've
    // been implemented in the backend.
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown resource hint: ", label()));
  }
}

// Types that are supported.
template class ConferenceResourceDataChannel<MediaEntriesChannelToClient,
                                             NoResourceRequestsFromClient>;
template class ConferenceResourceDataChannel<ParticipantsChannelToClient,
                                             NoResourceRequestsFromClient>;
template class ConferenceResourceDataChannel<VideoAssignmentChannelToClient,
                                             VideoAssignmentChannelFromClient>;
template class ConferenceResourceDataChannel<SessionControlChannelToClient,
                                             SessionControlChannelFromClient>;
template class ConferenceResourceDataChannel<std::string, std::string>;

}  // namespace meet
