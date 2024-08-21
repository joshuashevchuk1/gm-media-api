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
 * @fileoverview Video assignment channel handler.
 */

import {MediaApiCanvas, MediaApiResponseStatus, SetVideoAssignmentRequest, SetVideoAssignmentResponse, VideoAssignment, VideoAssignmentChannelFromClient, VideoAssignmentChannelToClient} from '../../types/datachannels';
import {MediaEntry, MediaLayout, MediaLayoutRequest} from '../../types/mediatypes';
import {InternalMediaEntry, InternalMediaLayout} from '../internal_types';
import {SubscribableDelegate} from '../subscribable_impl';
import {createMediaEntry} from '../utils';

// We request the highest possible resolution by default.
const MAX_RESOLUTION = {
  height: 1080,
  width: 1920,
  frameRate: 30,
};

/**
 * Helper class to handle the video assignment channel.
 */
export class VideoAssignmentChannelHandler {
  private requestId = 1;
  private readonly mediaLayoutLabelMap = new Map<MediaLayout, string>();
  private readonly pendingRequestResolveMap =
      new Map<number, ((value: MediaApiResponseStatus) => void)>();

  constructor(
      private readonly channel: RTCDataChannel,
      private readonly idMediaEntryMap: Map<number, MediaEntry>,
      private readonly internalMediaEntryMap =
          new Map<MediaEntry, InternalMediaEntry>(),
      private readonly idMediaLayoutMap = new Map<number, MediaLayout>(),
      private readonly internalMediaLayoutMap =
          new Map<MediaLayout, InternalMediaLayout>(),
      private readonly mediaEntriesDelegate: SubscribableDelegate<MediaEntry[]>,
  ) {
    this.channel.onmessage = (event) => {
      this.onVideoAssignmentMessage(event);
    };
    this.channel.onclose = () => {
      // Resolve all pending requests with an error.
      for (const [, resolve] of this.pendingRequestResolveMap) {
        resolve({code: 400, message: 'Channel closed', details: []});
      }
      this.pendingRequestResolveMap.clear();
    };
  }

  private onVideoAssignmentMessage(message: MessageEvent) {
    const data = JSON.parse(message.data) as VideoAssignmentChannelToClient;
    if (data.response) {
      this.onVideoAssignmentResponse(data.response);
    }
    if (data.resources) {
      this.onVideoAssignmentResources(data.resources);
    }
  }

  private onVideoAssignmentResponse(response: SetVideoAssignmentResponse) {
    // Users should listen on the video assignment channel for actual video
    // assignments. These responses signify that the request was expected.
    this.pendingRequestResolveMap.get(response.requestId)?.(response.status);
  }

  private onVideoAssignmentResources(resources: VideoAssignment[]) {
    resources.forEach((resource) => {
      if (resource.videoAssignment.canvases) {
        this.onVideoAssignment(resource);
      }
    });
  }

  private onVideoAssignment(videoAssignment: VideoAssignment) {
    const canvases = videoAssignment.videoAssignment.canvases;
    canvases.forEach((canvas: {
                       canvasId: number,
                       ssrc?: number, mediaEntryId: number,
                     }) => {
      const mediaLayout = this.idMediaLayoutMap.get(canvas.canvasId);
      // We expect that the media layout is already created.
      if (mediaLayout) {
        let mediaEntry = mediaLayout.mediaEntry.get();
        if (mediaEntry &&
            this.internalMediaEntryMap.get(mediaEntry)!.id !==
                canvas.mediaEntryId) {
          // If the media entry is already associated with a media layout,
          // we need to remove that association.
          const internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry);
          internalMediaEntry!.mediaLayout.set(undefined);
          mediaEntry = undefined;
        }

        if (!mediaEntry) {
          let newMediaEntry;
          if (this.idMediaEntryMap.has(canvas.mediaEntryId)) {
            newMediaEntry = this.idMediaEntryMap.get(canvas.mediaEntryId);
          } else {
            // We don't expect to hit this expression, but since data channels
            // don't guarantee order, we do this to be safe.
            const mediaEntryElement = createMediaEntry({
              id: canvas.mediaEntryId,
              audioMuted: false,
              videoMuted: false,
              screenShare: false,
              isPresenter: false,
              mediaLayout,
              videoSsrc: canvas.ssrc,
            });
            this.internalMediaEntryMap.set(
                mediaEntryElement.mediaEntry,
                mediaEntryElement.internalMediaEntry);
            newMediaEntry = mediaEntryElement.mediaEntry;
            this.idMediaEntryMap.set(canvas.mediaEntryId, newMediaEntry);
            const newMediaEntries =
                [...this.mediaEntriesDelegate.get(), newMediaEntry];
            this.mediaEntriesDelegate.set(newMediaEntries);
          }
          this.internalMediaLayoutMap.get(mediaLayout)
              ?.mediaEntry.set(newMediaEntry);
          // We expect the media entry to be created, without assertion, TS
          // complains it can be undefined.
          // tslint:disable-next-line:no-unnecessary-type-assertion
          this.internalMediaEntryMap.get(newMediaEntry!)
              ?.mediaLayout.set(mediaLayout);
        }
      }
      // TODO: b/341361368 - Handle the case where the media layout is not
      // created in the client but the server sends a video assignment for it.
    });
  }

  sendRequests(mediaLayoutRequests: MediaLayoutRequest[]):
      Promise<MediaApiResponseStatus> {
    const label = Date.now().toString();
    const canvases: MediaApiCanvas[] = [];
    mediaLayoutRequests.forEach((request) => {
      this.mediaLayoutLabelMap.set(request.mediaLayout, label);
      canvases.push({
        id: this.internalMediaLayoutMap.get(request.mediaLayout)!.id,
        dimensions: request.mediaLayout.canvasDimensions,
        relevant: {},
      });
    });
    const request: SetVideoAssignmentRequest = {
      requestId: this.requestId++,
      setAssignment: {
        layoutModel: {
          label,
          canvases,
        },
        maxVideoResolution: MAX_RESOLUTION,
      },
    };
    this.channel.send(JSON.stringify({
      request,
    } as VideoAssignmentChannelFromClient));
    const requestPromise = new Promise<MediaApiResponseStatus>((resolve) => {
      this.pendingRequestResolveMap.set(request.requestId, resolve);
    });
    return requestPromise;
  }
}
