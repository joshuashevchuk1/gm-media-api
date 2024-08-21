# Google Meet Media API - Samples

## Overview

The *Google Meet Media API* allows services to connect to ongoing *Google Meet*
conferences and receive media. This repository provides sample clients
demonstrating use of the API.

For more information, see https://developers.google.com/meet.

## License

```
Copyright 2024 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## Building

This repository contains two reference implementations and samples, TypeScript
and C++. These are stored in the `web` and `native` directories respectively.
Both implementation require [Bazel](https://bazel.build/) in order to build and
fetch many of the dependencies required.

### Web

The TypeScript implementation can be built by running `bazel build //web/...`.
Bazel will fetch any dependencies needed and build all relevant artifacts.

### Native

The C++ implementation requires a bit more setup due to the use of the native
WebRTC library which does not have a working Bazel build.

#### Build libwebrtc

Below is an abbreviated version of what’s explained in the
[webrtc docs](https://webrtc.github.io/webrtc-org/native-code/development/):

```
$ cd ~
$ mkdir src
$ cd src
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ export PATH=~/src/depot_tools:$PATH
$ mkdir webrtc-checkout
$ cd webrtc-checkout
$ fetch --nohooks webrtc
$ gclient sync
$ ./webrtc-checkout/webrtc/build/install-build-deps.sh
$ cd webrtc-checkout
$ mv src webrtc
$ cd webrtc
$ gn gen out/Default --args='is_debug=false use_custom_libcxx=false rtc_include_tests=false rtc_build_examples=false dcheck_always_on=true rtc_use_x11=false use_rtti=true'
$ ninja -C out/Default
```

This set of commands works today, but the above link should be referred to in
case the underlying tooling changes. If you’re building for a non-x64
Debian/Ubuntu linux variant, your prerequisite setup might be different.

Once libwebrtc is successfully built, you must update your `WORKSPACE` file to
point to your `webrtc-checkout` directory. You'll update the `webrtc_base` path
near the top of that file:

```
webrtc_path = "/usr/local/google/home/pareynolds/src/webrtc-checkout/"
```

#### Building the native client

Once libwebrtc is built and the `WORKSPACE` file is updated, you'll run your
Bazel build in much the same manner as for the web implementation:

```
$ bazel build //native/...
```

You can also run the tests:

```
$ bazel test //native/...
```

## Running the Samples

TODO: Add more details to this README.
