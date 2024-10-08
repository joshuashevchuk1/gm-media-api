#!/bin/bash
#
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

mkdir webrtc-checkout
pushd webrtc-checkout
fetch --no-history --nohooks webrtc
sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS
gclient sync
mv src webrtc
pushd webrtc
gn gen out/Default --args='is_debug=false use_custom_libcxx=false rtc_include_tests=false rtc_build_examples=false dcheck_always_on=true rtc_use_x11=false use_rtti=true'
ninja -C out/Default
popd
popd
