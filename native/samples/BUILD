# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "audio_sample",
    srcs = ["audio_sample.cc"],
    deps = [
        ":media_api_impls",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@media_api_samples//native/internal:meet_media_api_client",  # buildcleaner: keep
        "@media_api_samples//native/internal:meet_media_streams",  # buildcleaner: keep
        "@webrtc",
    ],
)

cc_binary(
    name = "video_sample",
    srcs = ["video_sample.cc"],
    deps = [
        ":media_api_impls",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@media_api_samples//native/internal:meet_media_api_client",  # buildcleaner: keep
        "@media_api_samples//native/internal:meet_media_streams",  # buildcleaner: keep
        "@webrtc",
    ],
)

cc_library(
    name = "media_api_impls",
    srcs = ["media_api_impls.cc"],
    hdrs = ["media_api_impls.h"],
    deps = [
        ":resource_parsers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@media_api_samples//native/api:conference_resources",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@media_api_samples//native/api:meet_media_sink_interface",
        "@media_api_samples//native/internal:meet_media_api_client",  # buildcleaner: keep
        "@media_api_samples//native/internal:meet_media_streams",  # buildcleaner: keep
        "@webrtc",
    ],
)

cc_library(
    name = "resource_parsers",
    hdrs = ["resource_parsers.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@media_api_samples//native/api:conference_resources",
        "@media_api_samples//native/internal:meet_media_api_client",  # buildcleaner: keep
        "@nlohmann_json//:json",
    ],
)
