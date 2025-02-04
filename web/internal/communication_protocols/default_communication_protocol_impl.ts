/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview The default communication protocol for the Media API client
 * with Meet API.
 */

import {MeetMediaClientRequiredConfiguration} from '../../types/mediatypes';

import {
  MediaApiCommunicationProtocol,
  MediaApiCommunicationResponse,
} from '../../types/communication_protocol';

const MEET_API_URL = 'https://meet.googleapis.com/v2beta/';

/**
 * The HTTP communication protocol for communication with Meet API.
 */
export class DefaultCommunicationProtocolImpl
  implements MediaApiCommunicationProtocol
{
  constructor(
    private readonly requiredConfiguration: MeetMediaClientRequiredConfiguration,
    private readonly meetApiUrl: string = MEET_API_URL,
  ) {}

  async connectActiveConference(
    sdpOffer: string,
  ): Promise<MediaApiCommunicationResponse> {
    // Call to Meet API
    const connectUrl = `${this.meetApiUrl}${this.requiredConfiguration.meetingSpaceId}:connectActiveConference`;
    const response = await fetch(connectUrl, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${this.requiredConfiguration.accessToken}`,
      },
      body: JSON.stringify({
        'offer': sdpOffer,
      }),
    });
    if (!response.ok) {
      throw new Error(
        `Failed to join meeting, status: ${response.status}, message: ${response.statusText}`,
      );
    }
    const body = await response.json();
    return {answer: body['answer']} as MediaApiCommunicationResponse;
  }
}
