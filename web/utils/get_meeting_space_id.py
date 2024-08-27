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

"""Gets meeting space id from meeting space code.

Modified from
https://developers.google.com/meet/api/guides/quickstart/python#configure_the_sample
and
https://developers.google.com/meet/api/guides/meeting-spaces#get-meeting-space.
Note that this has the limitation that it has to be run in an environment that
can open a browser.
"""

import os.path
from typing import Sequence

from absl import app
from absl import flags
from google.apps import meet_v2
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow


# If modifying these scopes, delete the file token.json.
SCOPES = 'https://www.googleapis.com/auth/meetings.space.readonly,https://www.googleapis.com/auth/meetings.media.readonly'

_MEETING_CODE = flags.DEFINE_string(
    'meeting_code', None, 'Meeting code of your meeting'
)
_CREDENTIAL_FILE_PATH = flags.DEFINE_string(
    'credential_file_path',
    './credentials.json',
    'Path to client credential file',
)
_SCOPES = flags.DEFINE_string(
    'scopes', SCOPES, 'String of oauth scopes separated by commas'
)


def main(argv: Sequence[str]):
  """Shows basic usage of the Google Meet API."""
  del argv
  creds = None
  # The file token.json stores the user's access and refresh tokens, and is
  # created automatically when the authorization flow completes for the first
  # time.
  scopes = _SCOPES.value.split(',')
  if not _MEETING_CODE.value:
    print('meeting code string is required')
    return

  if os.path.exists('token.json'):
    creds = Credentials.from_authorized_user_file('token.json', scopes)
  # If there are no (valid) credentials available, let the user log in.
  if not creds or not creds.valid:
    if creds and creds.expired and creds.refresh_token:
      creds.refresh(Request())
    else:
      flow = InstalledAppFlow.from_client_secrets_file(
          _CREDENTIAL_FILE_PATH.value, scopes
      )
      creds = flow.run_local_server(port=0)
      # Save the credentials for the next run
    with open('token.json', 'w') as token:
      token.write(creds.to_json())

  print(creds.token)

  try:
    client = meet_v2.SpacesServiceClient(credentials=creds)
    request = meet_v2.GetSpaceRequest(name=f'spaces/{_MEETING_CODE.value}')
    response = client.get_space(request=request)
    print(f'Meeting Space Id: {response.name}')
  # pylint: disable=broad-except
  except Exception as error:
    print(f'An error occurred: {error}')


if __name__ == '__main__':
  app.run(main)
