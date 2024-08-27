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
"""

from typing import Sequence
from absl import app
from absl import flags
from google.apps import meet_v2
from google.oauth2 import service_account


# If modifying these scopes, delete the file token.json.
SCOPES = 'https://www.googleapis.com/auth/meetings.space.readonly'

_SCOPES = flags.DEFINE_string(
    'scopes', SCOPES, 'String of oauth scopes separated by commas'
)

_MEETING_CODE = flags.DEFINE_string(
    'meeting_code', None, 'Meeting code of your meeting'
)

_PRIVATE_KEY_FILE_PATH = flags.DEFINE_string(
    'private_key_file_path',
    './credentials.json',
    'Path to private key file',
)

_DELEGATE_EMAIL = flags.DEFINE_string(
    'delegate_email',
    None,
    'Delegate email that service account impersonates',
)


def get_meeting_space_id(
    meeting_code_arg,
    private_key_file_arg,
    delegate_email_arg,
    scopes_arg,
):
  """Get meeting space with service account.

  Args:
      meeting_code_arg: The meeting space code for target meeting
      private_key_file_arg: The path to the private key file.
      delegate_email_arg: The email to delegate access to.
      scopes_arg: string of oauth scopes separated by commas.

  Returns:
      The meeting space id

  Raises:
      Exception: If the request for meeting id fails.
  """
  scopes = scopes_arg.split(',')

  creds = service_account.Credentials.from_service_account_file(
      private_key_file_arg, scopes=scopes, subject=delegate_email_arg
  )

  try:
    client = meet_v2.SpacesServiceClient(credentials=creds)
    request = meet_v2.GetSpaceRequest(name=f'spaces/{meeting_code_arg}')
    response = client.get_space(request=request)
    print(f'Meeting Space Id: {response.name}')
  # pylint: disable=broad-except
  except Exception as error:
    print(f'An error occurred: {error}')


def main(argv: Sequence[str]):
  """Shows basic usage of the Google Meet API."""
  del argv
  meeting_code = _MEETING_CODE.value
  private_key_file = _PRIVATE_KEY_FILE_PATH.value
  scopes = _SCOPES.value
  delegate_email = _DELEGATE_EMAIL.value
  if not meeting_code:
    # pylint: disable=broad-exception-raised
    raise Exception('meeting code is required')
  if not delegate_email:
    # pylint: disable=broad-exception-raised
    raise Exception('delegate email is required')
  get_meeting_space_id(meeting_code, private_key_file, delegate_email, scopes)


if __name__ == '__main__':
  app.run(main)
