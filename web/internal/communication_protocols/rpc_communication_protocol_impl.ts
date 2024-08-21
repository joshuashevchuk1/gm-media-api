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
 * @fileoverview An RPC communication protocol for testing. Implements the
 * Abstract CommunicationProtocol interface.
 */

import ConnectActiveConferenceRpcId from 'goog:proto.google.apps.meet.v2main.spacesService.ConnectActiveConferenceRpcId';
import {ConnectActiveConferenceRequest, ImmutableConnectActiveConferenceResponse,} from 'google3/google/apps/meet/v2main/service.proto';
import {ImmutableGenericDataInterface} from 'google3/javascript/frameworks/client/data/immutablegenericdatainterface';

import {MeetMediaClientRequiredConfiguration} from '../../types/mediatypes';

import {MediaApiCommunicationProtocol, MediaApiCommunicationResponse} from './communication_protocol';

/**
 * The RPC communication protocol for communication with Meet API. Used for
 * testing.
 */
export class RpcCommunicationProtocolImpl implements
    MediaApiCommunicationProtocol {
  constructor(
      private readonly requiredConfiguration:
          MeetMediaClientRequiredConfiguration,
      private readonly immutableGenericDataService:
          ImmutableGenericDataInterface,
  ) {}

  async connectActiveConference(sdpOffer: string):
      Promise<MediaApiCommunicationResponse> {
    const response: ImmutableConnectActiveConferenceResponse =
        await this.immutableGenericDataService.mutate(
            ConnectActiveConferenceRpcId.getInstance(
                new ConnectActiveConferenceRequest()
                    .setName(
                        'spaces/' + this.requiredConfiguration.meetingSpaceId)
                    .setOffer(sdpOffer),
                ),
        );
    return Promise.resolve(
        {answer: response.getAnswer()} as MediaApiCommunicationResponse);
  }
}
