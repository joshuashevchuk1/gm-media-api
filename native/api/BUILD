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
    name = "meet_media_api_client_interface",
    hdrs = [
        "conference_resources.h",
        "meet_media_api_client_interface.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        ":meet_media_sink_interface",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@webrtc",
    ],
)

cc_library(
    name = "meet_media_sink_interface",
    hdrs = [
        "conference_resources.h",
        "meet_media_sink_interface.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@webrtc",
    ],
)

cc_library(
    name = "conference_resources",
    hdrs = [
        "conference_resources.h",
    ],
    visibility = ["@media_api_samples//native:__subpackages__"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
    ],
)
