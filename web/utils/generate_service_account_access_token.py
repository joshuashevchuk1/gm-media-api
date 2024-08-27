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

"""Generates an access token for a service account.

This script generates an access token for a service account by exchanging a JWT
for an access token. The JWT is signed with the private key of the service
account. The access token can then be used to make authenticated requests to
Google APIs.

For more information:
https://developers.google.com/identity/protocols/oauth2/service-account
"""

import datetime
import json
from typing import Sequence

from absl import app
from absl import flags
import google.auth.crypt
import google.auth.jwt
import requests

SCOPES = "https://www.googleapis.com/auth/meetings.space.readonly,https://www.googleapis.com/auth/meetings.media.readonly,https://www.googleapis.com/auth/meetings.space.created"

_SERVICE_ACCOUNT_EMAIL = flags.DEFINE_string(
    "service_account_email",
    None,
    "Service account email",
)

_DELEGATE_EMAIL = flags.DEFINE_string(
    "delegate_email",
    None,
    "Delegate email that service account impersonates",
)

_PRIVATE_KEY_FILE_PATH = flags.DEFINE_string(
    "private_key_file_path",
    "./credentials.json",
    "Path to private key file",
)

_SCOPES = flags.DEFINE_string(
    "scopes", SCOPES, "String of oauth scopes separated by commas"
)


def generate_jwt_and_access_token(
    service_account_email_arg,
    private_key_file_arg,
    delegate_email_arg,
    scopes_arg,
):
  """Generates a JWT and then an access token.

  Args:
      service_account_email_arg: The email address of the service account.
      private_key_file_arg: The path to the private key file.
      delegate_email_arg: The email to delegate access to.
      scopes_arg: string of oauth scopes separated by commas.

  Returns:
      The access token.

  Raises:
      Exception: If the request to exchange the JWT for an access token fails.
  """

  scopes = scopes_arg.split(",")
  # Load the private key
  with open(private_key_file_arg, "r") as f:
    private_key = f.read()

  file_key = json.loads(private_key)
  # Create the JWT header
  header = {"alg": "RS256", "typ": "JWT"}

  # Create the JWT payload
  now = datetime.datetime.now(datetime.timezone.utc)
  exp = now + datetime.timedelta(hours=1)  # Token expires in 1 hour
  payload = {
      "iss": service_account_email_arg,
      "sub": (
          delegate_email_arg
          if delegate_email_arg
          else service_account_email_arg
      ),
      "aud": "https://oauth2.googleapis.com/token",
      "scope": " ".join(scopes),
      "iat": int(now.timestamp()),
      "exp": int(exp.timestamp()),
  }

  # Sign the JWT with the private key
  signer = google.auth.crypt.RSASigner.from_string(file_key["private_key"])
  jwt = google.auth.jwt.encode(signer, payload, header)

  # Exchange the JWT for an access token
  data = {
      "grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer",
      "assertion": jwt,
  }
  response = requests.post("https://oauth2.googleapis.com/token", data=data)
  # Handle potential errors
  if response.status_code != 200:
    # pylint: disable=broad-exception-raised
    raise Exception(f"Error getting access token: {response.text}")

  with open("service_account_token.json", "w") as token:
    token.write(json.dumps(response.json()))

  # Extract the access token
  oauth2_access_token = response.json()["access_token"]
  return oauth2_access_token


def main(argv: Sequence[str]):
  """Shows basic usage of the Google Meet API."""
  del argv
  service_account_email = _SERVICE_ACCOUNT_EMAIL.value
  delegate_email = _DELEGATE_EMAIL.value
  private_key_file = _PRIVATE_KEY_FILE_PATH.value
  scopes = _SCOPES.value
  if not service_account_email:
    # pylint: disable=broad-exception-raised
    raise Exception("service account is required")
  if not delegate_email:
    # pylint: disable=broad-exception-raised
    raise Exception("delegate email is required")

  try:
    access_token = generate_jwt_and_access_token(
        service_account_email, private_key_file, delegate_email, scopes
    )
    print("Access Token:", access_token)
  # pylint: disable=broad-except
  except Exception as e:
    print(f"An error occurred: {e}")


if __name__ == "__main__":
  app.run(main)
