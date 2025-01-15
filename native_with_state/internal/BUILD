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

cc_library(
    name = "conference_data_channel_interface",
    hdrs = ["conference_data_channel_interface.h"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
    ],
)

cc_library(
    name = "conference_data_channel",
    srcs = ["conference_data_channel.cc"],
    hdrs = ["conference_data_channel.h"],
    deps = [
        ":conference_data_channel_interface",
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "resource_handler_interface",
    hdrs = [
        "resource_handler_interface.h",
    ],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
    ],
)

cc_library(
    name = "media_entries_resource_handler",
    srcs = ["media_entries_resource_handler.cc"],
    hdrs = [
        "media_entries_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:media_entries_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "media_entries_resource_handler_test",
    srcs = ["media_entries_resource_handler_test.cc"],
    deps = [
        ":media_entries_resource_handler",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:media_entries_resource",
    ],
)

cc_library(
    name = "media_stats_resource_handler",
    srcs = ["media_stats_resource_handler.cc"],
    hdrs = [
        "media_stats_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:media_stats_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "media_stats_resource_handler_test",
    srcs = ["media_stats_resource_handler_test.cc"],
    deps = [
        ":media_stats_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:media_stats_resource",
        "@media_api_samples//native_with_state/api:session_control_resource",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "participants_resource_handler",
    srcs = ["participants_resource_handler.cc"],
    hdrs = [
        "participants_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:participants_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "participants_resource_handler_test",
    srcs = ["participants_resource_handler_test.cc"],
    deps = [
        ":participants_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:participants_resource",
    ],
)

cc_library(
    name = "session_control_resource_handler",
    srcs = ["session_control_resource_handler.cc"],
    hdrs = [
        "session_control_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:session_control_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "session_control_resource_handler_test",
    srcs = ["session_control_resource_handler_test.cc"],
    deps = [
        ":session_control_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:media_stats_resource",
        "@media_api_samples//native_with_state/api:session_control_resource",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "video_assignment_resource_handler",
    srcs = ["video_assignment_resource_handler.cc"],
    hdrs = [
        "video_assignment_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:video_assignment_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "video_assignment_resource_handler_test",
    srcs = ["video_assignment_resource_handler_test.cc"],
    deps = [
        ":video_assignment_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//native_with_state/api:media_api_client_interface",
        "@media_api_samples//native_with_state/api:session_control_resource",
        "@media_api_samples//native_with_state/api:video_assignment_resource",
        "@nlohmann_json//:json",
    ],
)
