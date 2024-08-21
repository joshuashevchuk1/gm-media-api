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
 * @fileoverview Handles the session control channel.
 */

import {LeaveRequest, SessionControlChannelFromClient, SessionControlChannelToClient} from '../../types/datachannels';
import {MeetSessionStatus} from '../../types/mediatypes';
import {SubscribableDelegate} from '../subscribable_impl';

/**
 * Helper class to handles the session control channel.
 */
export class SessionControlChannelHandler {
  private requestId = 1;
  private leaveSessionPromise: (() => void)|undefined;

  constructor(
      private readonly channel: RTCDataChannel,
      private readonly sessionStatusDelegate:
          SubscribableDelegate<MeetSessionStatus>) {
    this.channel.onmessage = (event) => {
      this.onSessionControlMessage(event);
    };
    this.channel.onopen = () => {
      this.onSessionControlOpened();
    };
    this.channel.onclose = () => {
      this.onSessionControlClosed();
    };
  }

  private onSessionControlOpened() {
    this.sessionStatusDelegate.set(MeetSessionStatus.WAITING);
  }

  private onSessionControlMessage(event: MessageEvent) {
    const message = event.data;
    const json = JSON.parse(message) as SessionControlChannelToClient;
    if (json?.response?.leave && this.leaveSessionPromise) {
      this.leaveSessionPromise();
    }
    if (json?.resources && json.resources.length > 0) {
      const sessionStatus = json.resources[0].sessionStatus;
      if (sessionStatus.connectionState === 'STATE_WAITING') {
        this.sessionStatusDelegate.set(MeetSessionStatus.WAITING);
      } else if (sessionStatus.connectionState === 'STATE_JOINED') {
        this.sessionStatusDelegate.set(MeetSessionStatus.JOINED);
      } else if (sessionStatus.connectionState === 'STATE_DISCONNECTED') {
        this.sessionStatusDelegate.set(MeetSessionStatus.DISCONNECTED);
      }
    }
  }

  private onSessionControlClosed() {
    // If the channel is closed, we should resolve the leave session promise.
    this.leaveSessionPromise?.();
    this.sessionStatusDelegate.set(MeetSessionStatus.DISCONNECTED);
  }

  leaveSession(): Promise<void> {
    this.channel.send(JSON.stringify({
      request: {
        requestId: this.requestId++,
        leave: {},
      } as LeaveRequest,
    } as SessionControlChannelFromClient));
    return new Promise<void>((resolve) => {
      this.leaveSessionPromise = resolve;
    });
  }
}
