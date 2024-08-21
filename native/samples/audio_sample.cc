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


#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/samples/media_api_impls.h"
#include "webrtc/api/make_ref_counted.h"

ABSL_FLAG(std::string, output_file_prefix, "/tmp/audio_sink_ssrc_",
          "Path prefix where files will be written. The file name will be "
          "prefix_<ssrc>_audio.pcm.");

ABSL_FLAG(std::string, meet_api_url,
          "https://meet.googleapis.com/v2beta/",
          "The base URL to use for the Meet API.");

ABSL_FLAG(std::string, meeting_space_id, "",
          "The ID of the meeting space to connect to.");

ABSL_FLAG(std::string, oauth_token, "",
          "The OAuth token to use for the Meet API.");

ABSL_FLAG(absl::Duration, collection_duration, absl::Seconds(30),
          "The duration of the collection.");

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);

  std::string output_file_prefix = absl::GetFlag(FLAGS_output_file_prefix);
  if (output_file_prefix.empty()) {
    LOG(ERROR) << "Output file prefix is empty";
    return EXIT_FAILURE;
  }
  std::string meet_api_url = absl::GetFlag(FLAGS_meet_api_url);
  if (meet_api_url.empty()) {
    LOG(ERROR) << "Meet API URL is empty";
    return EXIT_FAILURE;
  }
  std::string meeting_space_id = absl::GetFlag(FLAGS_meeting_space_id);
  if (meeting_space_id.empty()) {
    LOG(ERROR) << "Meeting space ID is empty";
    return EXIT_FAILURE;
  }
  std::string oauth_token = absl::GetFlag(FLAGS_oauth_token);
  if (oauth_token.empty()) {
    LOG(ERROR) << "OAuth token is empty";
    return EXIT_FAILURE;
  }
  absl::Duration collection_duration = absl::GetFlag(FLAGS_collection_duration);

  // Configure a session with no video streams and only audio.
  meet::MeetMediaApiClientConfiguration api_config = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = true,
      .enable_media_entries_resource = true,
      .enable_video_assignment_resource = false,
  };

  auto client_create_status = meet::MeetMediaApiClientInterface::Create(
      api_config, rtc::make_ref_counted<media_api_impls::SessionObserver>(),
      rtc::make_ref_counted<media_api_impls::SinkFactory>(output_file_prefix));

  if (!client_create_status.ok()) {
    LOG(ERROR) << "Failed to create client: " << client_create_status.status()
               << std::endl;
    return EXIT_FAILURE;
  }
  auto meet_client = *std::move(client_create_status);

  absl::Status join_status = meet_client->ConnectActiveConference(
      meet_api_url, meeting_space_id, oauth_token);
  if (!join_status.ok()) {
    LOG(ERROR) << "Failed to join conference: " << join_status << std::endl;
    return EXIT_FAILURE;
  }
  // TODO: b/359387259 - Send a leave request to end the session once it's been
  // implemented in the client.
  absl::SleepFor(collection_duration);
  return EXIT_SUCCESS;
}
