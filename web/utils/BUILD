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

load("@rules_python//python:py_binary.bzl", "py_binary")
load("@util_deps//:requirements.bzl", "requirement")

py_binary(
    name = "get_meeting_space_id",
    srcs = ["get_meeting_space_id.py"],
    python_version = "PY3",
    deps = [
        requirement("google-apps-meet"),
        requirement("google-auth"),
        requirement("google-auth-oauthlib"),
        requirement("grpcio"),
        "@com_google_absl_py//absl:app",
        "@com_google_absl_py//absl/flags",
    ],
)

py_binary(
    name = "get_meeting_space_id_service_account",
    srcs = ["get_meeting_space_id_service_account.py"],
    python_version = "PY3",
    deps = [
        requirement("google-apps-meet"),
        requirement("google-auth"),
        requirement("google-auth-oauthlib"),
        requirement("grpcio"),
        "@com_google_absl_py//absl:app",
        "@com_google_absl_py//absl/flags",
    ],
)

py_binary(
    name = "generate_service_account_access_token",
    srcs = ["generate_service_account_access_token.py"],
    python_version = "PY3",
    deps = [
        requirement("google-auth"),
        requirement("requests"),
        "@com_google_absl_py//absl:app",
        "@com_google_absl_py//absl/flags",
    ],
)

filegroup(
    name = "requirements_file",
    srcs = ["requirements.txt"],
)
