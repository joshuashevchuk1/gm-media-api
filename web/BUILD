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

load("//:build_defs.bzl", "ts_library")

package(default_visibility = [
    "@media_api_samples//web:__subpackages__",
])

ts_library(
    name = "meetmediaapiclient_interface",
    srcs = ["meetmediaapiclient.ts"],
    deps = [
        "@media_api_samples//web/internal/communication_protocols:communication_protocol_interface",
        "@media_api_samples//web/types:data_channels",
        "@media_api_samples//web/types:media_types",
        "@media_api_samples//web/types:subcribable",
    ],
)

ts_library(
    name = "meetmediaapiclient_impl",
    srcs = [
        "meetmediaapiclient_impl.ts",
    ],
    deps = [
        ":meetmediaapiclient_interface",
        "@media_api_samples//web/internal:internal_types",
        "@media_api_samples//web/internal:meet_stream_track_impl",
        "@media_api_samples//web/internal:subscribable_impl",
        "@media_api_samples//web/internal/channel_handlers",
        "@media_api_samples//web/internal/communication_protocols:communication_protocol_interface",
        "@media_api_samples//web/internal/communication_protocols:default_communication_protocol",
        "@media_api_samples//web/types:data_channels",
        "@media_api_samples//web/types:media_types",
        "@media_api_samples//web/types:subcribable",
    ],
)
