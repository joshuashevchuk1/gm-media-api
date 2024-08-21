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
Bazel will fetch any dependencies needed and build all relevant artifacts and
convert it to JavaScript.

You can also use a webpack configuration as shown in the samples.

### Native

The C++ implementation requires a bit more setup due to the use of the native
WebRTC library which does not have a working Bazel build.

#### Build libwebrtc

Below is an abbreviated version of what’s explained in the
[webrtc docs](https://webrtc.github.io/webrtc-org/native-code/development/):

```sh
$ cd ~
$ mkdir src
$ cd src
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ export PATH=~/src/depot_tools:$PATH
$ mkdir webrtc-checkout
$ cd webrtc-checkout
$ fetch --nohooks webrtc
$ gclient sync
$ mv src webrtc
$ cd webrtc
$ ./build/install-build-deps.sh
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

```sh
$ bazel build //native/...
```

You can also run the tests:

```sh
$ bazel test //native/...
```

## Running the samples

### Generating OAuth tokens

If you are running a web app, this can be done with https://developers.google.com/identity/protocols/oauth2/javascript-implicit-flow.

This is similar to how the TypeScript sample generates OAuth tokens.

For the native client, we can use https://developers.google.com/identity/protocols/oauth2/service-account.

> **Note:** You must give the service account the role of "Service Account Token Creator"

> **Note:** For service accounts to get user data, the account must impersonate users. In order to this, the admin of the domain must give domain-wide delegation of authority. Please see https://developers.google.com/meet/api/guides/authenticate-authorize#domain-wide-delegation.

> **Note:** Some documentation will encourage you to generate tokens with https://developers.google.com/oauthplayground. However, you will not be able to connect to the API if you retrieve an OAuth token this way because it is not associated with a cloud project.

### Running native C++ client samples

#### Receive audio

```sh
$ bazel run native/samples:audio_sample -- --meeting_space_id <MEETING_SPACE_ID> --oauth_token <INPUT_YOUR_TOKEN_HERE>
```

By default the audio is saved into a pcm file with the prefix /tmp/audio_sink_ssrc_.

#### Receive video

```sh
$ bazel run native/samples:video_sample -- --meeting_space_id <MEETING_SPACE_ID> --oauth_token <INPUT_YOUR_TOKEN_HERE>
```

By default the audio is saved into a pcm file with the prefix `/tmp/video_sink_ssrc_`.
The video metadata (csrc and dimensions per frame) are saved in a text file with the same prefix.

These are additional options that can specified for the audio or video samples.

| Option | Description |
|---|---|
| `--output_file_prefix <file_prefix>` | Specify file prefix for audio or video metadata |
| `--collection_duration <duration>` | Specify duration of audio or video collection |


> **Note:** There is currently a known bug where meeting codes are not accepted. For now, you must retrieve the `space` resource identifier by calling the *Meet API*. Please see https://developers.google.com/meet/api/guides/meeting-spaces#identify-meeting-space for more details.


### Running TypeScript client samples

#### Prerequisites

- You will need to install Node.js and yarn.
- This sample project uses webpack to compile the TS client to JS and be serveable.
- Due to CORS requirements, you must deploy this sample before it can hit the *Meet API*.

**We recommend deploying the sample on *Google App Engine*.**

#### Steps to run

1. To start, clone the repo

    *\* All instructions assume you are in the `web/samples` directory*

2. Run yarn install

    ```sh
    yarn install
    ```

3. Run webpack

    ```sh
    webpack
    ```

4. Deploy your server to *Google App Engine*.

    *If you run into issues, see https://cloud.google.com/appengine.*

    ```sh
    gcloud app deploy app.yaml
    ```

5. Navigate to your endpoint. One way to do so would be to use the `gcloud` command or the default URL provided in the console.

    ```sh
    gcloud app browse
    ```

6. Create *OAuth 2* credentials following https://developers.google.com/workspace/guides/create-credentials#oauth-client-id. You will need the following scopes:

    - https://www.googleapis.com/auth/meetings.media.readonly
    - https://www.googleapis.com/auth/meetings.space.readonly

    Add your *Google App Engine* URL to the *Authorized JavaScript Origins* and *Authorized Redirect URIs*.

7. Once created, copy the client ID and paste it into the loaded web page.

8. Hit the login button and proceed to the prompts. If you run into an error, note that it can take a few minutes for the redirect URIs to propagate.

9. Go to https://meet.google.com/new.

10. Copy the meeting code and paste it into the meeting code input with the format `spaces/{meeting_code}`.

> **Note:** There is currently a known bug where meeting codes are not accepted. For now, you must retrieve the `space` resource identifier by calling the *Meet API*. Please see https://developers.google.com/meet/api/guides/meeting-spaces#identify-meeting-space for more details.

11. Select number of video streams and enable/disable audio.

12. Click the *Create Client* button.

13. Click on *Join Meeting* button.

14. Observe video stream and audio elements being added to the page.
