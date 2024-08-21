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
    name = "meet_media_api_client",
    srcs = ["meet_media_api_client.cc"],
    hdrs = [
        "meet_media_api_client.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        ":conference_resource_data_channel",
        ":curl_request",
        ":media_entries_resource_handler",
        ":meet_connect_response",
        ":meet_media_api_audio_device_module",
        ":meet_media_streams",
        ":meet_session_observers",
        ":resource_handler_interface",
        ":session_control_resource_handler",
        ":video_assignment_resource_handler",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleurl//url",
        "@media_api_samples//native/api:conference_resources",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@media_api_samples//native/api:meet_media_sink_interface",
        "@nlohmann_json//:json",
        "@webrtc",
    ],
)

cc_library(
    name = "curl_request",
    srcs = [
        "curl_request.cc",
    ],
    hdrs = [
        "curl_request.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@curl",
    ],
)

cc_library(
    name = "meet_connect_response",
    srcs = [
        "meet_connect_response.cc",
    ],
    hdrs = [
        "meet_connect_response.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "meet_connect_response_test",
    srcs = ["meet_connect_response_test.cc"],
    deps = [
        ":meet_connect_response",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "curl_request_test",
    srcs = ["curl_request_test.cc"],
    deps = [
        ":curl_request",
        ":mock_curl_api_wrapper",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
        "@curl",
    ],
)

cc_library(
    name = "meet_session_observers",
    srcs = [
        "meet_session_observers.cc",
    ],
    hdrs = [
        "meet_session_observers.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "meet_media_streams",
    srcs = [
        "meet_media_streams.cc",
    ],
    hdrs = [
        "meet_media_streams.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@media_api_samples//native/api:meet_media_sink_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "meet_media_api_audio_device_module",
    srcs = [
        "meet_media_api_audio_device_module.cc",
    ],
    hdrs = [
        "meet_media_api_audio_device_module.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/synchronization",
        "@webrtc",
    ],
)

cc_library(
    name = "conference_resource_data_channel",
    srcs = ["conference_resource_data_channel.cc"],
    hdrs = [
        "conference_resource_data_channel.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//native/api:conference_resources",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@webrtc",
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
        "@media_api_samples//native/api:conference_resources",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "media_entries_resource_handler_test",
    srcs = ["media_entries_resource_handler_test.cc"],
    deps = [
        ":media_entries_resource_handler",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//native/api:conference_resources",
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
        "@media_api_samples//native/api:conference_resources",
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
        "@media_api_samples//native/api:conference_resources",
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
        "@media_api_samples//native/api:conference_resources",
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
        "@media_api_samples//native/api:conference_resources",
        "@nlohmann_json//:json",
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
    ],
)

cc_library(
    name = "mock_meet_media_sink_factory",
    testonly = 1,
    hdrs = [
        "testing/mock_meet_media_sink_factory.h",
    ],
    deps = [
        "@com_google_googletest//:gtest",
        "@media_api_samples//native/api:meet_media_sink_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "mock_meet_media_api_session_observer",
    testonly = 1,
    hdrs = [
        "testing/mock_meet_media_api_session_observer.h",
    ],
    deps = [
        "@com_google_googletest//:gtest",
        "@media_api_samples//native/api:conference_resources",
        "@media_api_samples//native/api:meet_media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "mock_curl_api_wrapper",
    testonly = 1,
    hdrs = [
        "testing/mock_curl_api_wrapper.h",
    ],
    deps = [
        ":curl_request",
        "@com_google_googletest//:gtest",
        "@curl",
    ],
)

cc_library(
    name = "mock_curl_request_factory",
    testonly = 1,
    hdrs = [
        "testing/mock_curl_request_factory.h",
    ],
    deps = [
        ":curl_request",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "webrtc_testing",
    testonly = 1,
    hdrs = [
        "testing/sdp_constants.h",
    ],
)
