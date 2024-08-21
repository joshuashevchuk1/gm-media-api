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

"""Build definitions for Media API sample clients."""

load("@npm//@bazel/typescript:index.bzl", "ts_project")

DEFAULT_TS_DEPS = []

def provided_args(**kwargs):
    return {k: v for k, v in kwargs.items() if v != None}

def ts_library(
        name,
        srcs = [],
        visibility = None,
        deps = [],
        testonly = 0):
    ts_project(**provided_args(
        name = name,
        srcs = srcs,
        visibility = visibility,
        deps = deps + DEFAULT_TS_DEPS,
        testonly = testonly,
        declaration = True,
        tsconfig = "//:tsconfig.json",
    ))
