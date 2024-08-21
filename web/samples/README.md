# Media API Typescript reference client Samples

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

This is meant as a guide of how to use the reference implementation of the
*Media API* from *Google Meet*.

- You will need to install Node.js and yarn.
- This sample project uses webpack to compile the TS client to JS and be serveable.
- Due to CORS requirements, you must deploy this sample before it can hit the *Meet API*.

**We recommend deploying the sample on *Google App Engine*.**

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

    **Note:** There is currently a known bug where meeting codes are not accepted. For now, you must retrieve the `space` resource identifier by calling the *Meet API*. Please see https://developers.google.com/meet/api/guides/meeting-spaces#identify-meeting-space for more details.

11. Select number of video streams and enable/disable audio.

12. Click the *Create Client* button.

13. Click on *Join Meeting* button.

14. Observe video stream and audio elements being added to the page.
