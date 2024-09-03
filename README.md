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

This repository contains two reference implementations and samples: TypeScript
and C++. These are stored in the `web` and `native` directories respectively.
The C++ implementation requires [Bazel](https://bazel.build/) in order to build and
fetch many of the dependencies required. The TypeScript implementation can be built
with Bazel or with [Webpack](https://webpack.js.org/).

### Web

The TypeScript implementation can be built by running `bazel build //web/...`.
Bazel will fetch any dependencies needed and build all relevant artifacts and
convert it to JavaScript.

You can also use a webpack configuration as shown in the [samples](#running-typescript-client-samples).

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

> **Note:** Some documentation will encourage you to generate tokens with https://developers.google.com/oauthplayground. However, you will not be able to connect to the API if you retrieve an OAuth token this way because it is not associated with a cloud project.

#### Web app

If you are running a web app, this can be done with the [implicit grant flow](https://developers.google.com/identity/protocols/oauth2/javascript-implicit-flow).

The drawback to this approach is that this requires a browser environment and your server has to
be deployed.

This is how the TypeScript sample server generates OAuth tokens. If you are running the TypeScript sample, you can skip getting OAuth tokens as this is handled in the deployed server. You will still need an OAuth client ID; follow [these instructions](https://developers.google.com/identity/protocols/oauth2/javascript-implicit-flow#prerequisites) to create a client. Once you have your client, skip to [this section](#running-typescript-client-samples).

- If you want to run the native (C++) client in a browser environment, continue to the [next section](#user-accounts).
- If you want to run the native client in a server-based environment, skip to this [section](#server-environment).

#### User accounts

> **Note:** This approach requires the environment can use a browser for login. For a server-based approach, see the [next section](#server-environment).

If you want to avoid running a web app, you can set up a client as a desktop app. The process is described in [here](https://developers.google.com/meet/api/guides/quickstart/python#set-up-environment).

We have created a helper script to get the access token and meeting space ID.
(There is known issue where the API only accepts meeting space IDs, not meeting
codes.)

##### Run browser OAuth2 script

###### Prerequisites

1. Client credentials

    * Follow setup instructions [here](https://developers.google.com/meet/api/guides/quickstart/python#set-up-environment) up to *"Install the Google Client library"*
    * Download client credentials file.

2. Make sure you have Bazel installed

3. [Start a meeting](https://meet.google.com/new) and copy meeting code.

> Commands are written for a Linux environment

```sh
$ bazel run web/utils:get_meeting_space_id -- -meeting_code <your_meeting_code> -credential_file_path <path_to_credential_file>
```

- This script will prompt you to sign in. Make sure you are using the same account you created the meeting with.
- The script will print out the access token and the meeting space ID. This is all you need to start the [native (C++) samples](#running-native-client-samples).
- If you need the access token later, you can retrieve from the token.json file that is generated by the script.

#### Server environment

##### Service accounts

To generate access tokens without a browser, you must use a service account.

###### Prerequisites

> **Note:** For service accounts to get user data in Meet API, the account must impersonate users. In order to this, the admin of the domain must give domain-wide delegation of authority. Please see https://developers.google.com/meet/api/guides/authenticate-authorize#domain-wide-delegation.

1. Service account credentials

    * Follow setup instructions [here](https://developers.google.com/identity/protocols/oauth2/service-account#creatinganaccount)
    * Download service account key file.

2. An account which can be impersonated (admin has given delegation of authority). You will use this account to create meetings.

3. Make sure you have Bazel installed

> Commands are written for a Linux environment

##### Run OAuth scripts

This script will return the access token.

```sh
$ bazel run web/utils:generate_service_account_access_token -- -service_account_email <service_account_email> -private_key_file_path <path_to_credential_file> -delegate_email <email_for_account_to_impersonate>
```

If you need the token later, it will be saved in service_account_token.json.

##### Run meeting space ID scripts

> There is known issue that the API requires meeting space id rather than meeting
codes.

> **Note:** Make sure your service account has the role of "Service Account Token Creator"

1. [Start a meeting](https://meet.google.com/new) with email that you are impersonating and copy meeting code.

To get meeting space id with the service account run this script

```sh
$ bazel run web/utils:get_meeting_space_id_service_account -- -meeting_code <your_meeting_code> -private_key_file_path <path_to_credential_file> -delegate_email <email_for_account_to_impersonate>
```

Once you have the token and meeting space id you are ready to run the [native (C++) samples](#running-native-client-samples).

### Running native client samples

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

> Due to CORS requirements, you must deploy this sample before it can hit the *Meet API*.

1. Install Node.js and yarn.
2. A Cloud project with *Google App Engine* enabled

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

> **Note:** There is currently a known bug where meeting codes are not accepted. For now, you must retrieve the `space` resource identifier by calling the *Meet API*. Please see https://developers.google.com/meet/api/guides/meeting-spaces#identify-meeting-space for more details. You can use the [oauth section](#generating-oauth-tokens) to help getting these meeting space ids.

11. Select number of video streams and enable/disable audio.

12. Click the *Create Client* button.

13. Click on *Join Meeting* button.

14. Observe video stream and audio elements being added to the page.
