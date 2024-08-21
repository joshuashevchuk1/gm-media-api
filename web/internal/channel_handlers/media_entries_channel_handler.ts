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
 * @fileoverview Handles Media entries
 */

import {DeletedMediaEntry, MediaEntriesChannelToClient, MediaEntry as MediaEntryResource} from '../../types/datachannels';
import {MediaEntry, MediaLayout, MeetStreamTrack} from '../../types/mediatypes';
import {InternalMediaEntry, InternalMediaLayout, InternalMeetStreamTrack} from '../internal_types';
import {SubscribableDelegate} from '../subscribable_impl';
import {createMediaEntry} from '../utils';

/**
 * Helper class to handle the media entries channel.
 */
export class MediaEntriesChannelHandler {
  constructor(
      private readonly channel: RTCDataChannel,
      private readonly mediaEntriesDelegate: SubscribableDelegate<MediaEntry[]>,
      private readonly idMediaEntryMap: Map<number, MediaEntry>,
      private readonly internalMediaEntryMap =
          new Map<MediaEntry, InternalMediaEntry>(),
      private readonly internalMeetStreamTrackMap =
          new Map<MeetStreamTrack, InternalMeetStreamTrack>(),
      private readonly internalMediaLayoutMap =
          new Map<MediaLayout, InternalMediaLayout>()) {
    this.channel.onmessage = (event) => {
      this.onMediaEntriesMessage(event);
    };
  }

  private onMediaEntriesMessage(message: MessageEvent) {
    const data = JSON.parse(message.data) as MediaEntriesChannelToClient;
    let mediaEntryArray = this.mediaEntriesDelegate.get();

    // Delete media entries.
    data.deletedResources?.forEach((deletedResource: DeletedMediaEntry) => {
      const deletedMediaEntry = this.idMediaEntryMap.get(deletedResource.id);
      if (deletedMediaEntry) {
        mediaEntryArray = mediaEntryArray.filter(
            mediaEntry => mediaEntry !== deletedMediaEntry);
        // If we find the media entry in the id map, it should exist in the
        // internal map.
        const internalMediaEntry =
            this.internalMediaEntryMap.get(deletedMediaEntry);
        // Remove relationship between media entry and media layout.
        const mediaLayout: MediaLayout|undefined =
            internalMediaEntry!.mediaLayout.get();
        if (mediaLayout) {
          const internalMediaLayout =
              this.internalMediaLayoutMap.get(mediaLayout);
          if (internalMediaLayout) {
            internalMediaLayout.mediaEntry.set(undefined);
          }
        }

        // Remove relationship between media entry and meet stream tracks.
        const videoMeetStreamTrack =
            internalMediaEntry!.videoMeetStreamTrack.get();
        if (videoMeetStreamTrack) {
          const internalVideoStreamTrack =
              this.internalMeetStreamTrackMap.get(videoMeetStreamTrack);
          internalVideoStreamTrack!.mediaEntry.set(undefined);
        }

        const audioMeetStreamTrack =
            internalMediaEntry!.audioMeetStreamTrack.get();
        if (audioMeetStreamTrack) {
          const internalAudioStreamTrack =
              this.internalMeetStreamTrackMap.get(audioMeetStreamTrack);
          internalAudioStreamTrack!.mediaEntry.set(undefined);
        }

        // Remove from maps
        this.idMediaEntryMap.delete(deletedResource.id);
        this.internalMediaEntryMap.delete(deletedMediaEntry);
      }
    });

    // Update or add media entries.
    const addedMediaEntries: MediaEntry[] = [];
    data.resources?.forEach((resource: MediaEntryResource) => {
      let internalMediaEntry: InternalMediaEntry|undefined;
      let mediaEntry: MediaEntry|undefined;
      let videoCsrc = 0;
      if (resource.mediaEntry.videoCsrcs.length > 0) {
        // We expect there to only be one video Csrcs. There is possibility
        // for this to be more than value in WebRTC but unlikely in Meet.
        // TODO : Explore making video csrcs field singluar.
        videoCsrc = resource.mediaEntry.videoCsrcs[0];
      }

      if (this.idMediaEntryMap.has(resource.id!)) {
        // Update media entry if it already exists.
        mediaEntry = this.idMediaEntryMap.get(resource.id!);
        internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry!);
        internalMediaEntry!.audioMuted.set(resource.mediaEntry.audioMuted);
        internalMediaEntry!.videoMuted.set(resource.mediaEntry.videoMuted);
        internalMediaEntry!.audioCsrc = resource.mediaEntry.audioCsrc;
        internalMediaEntry!.videoCsrc = videoCsrc;
      } else {
        // Create new media entry if it does not exist.
        const mediaEntryElement = createMediaEntry({
          audioMuted: resource.mediaEntry.audioMuted,
          videoMuted: resource.mediaEntry.videoMuted,
          screenShare: resource.mediaEntry.screenshare,
          isPresenter: resource.mediaEntry.presenter,
          id: resource.id!,
          audioCsrc: resource.mediaEntry.audioCsrc,
          videoCsrc,
        });
        internalMediaEntry = mediaEntryElement.internalMediaEntry;
        mediaEntry = mediaEntryElement.mediaEntry;
        this.internalMediaEntryMap.set(mediaEntry, internalMediaEntry);
        this.idMediaEntryMap.set(internalMediaEntry.id, mediaEntry);
        addedMediaEntries.push(mediaEntry);
      }

      // Assign meet streams to media entry if they are not already assigned
      // correctly.
      if (!(this.isAudioMeetStreamTrackAssignedToMediaEntry(internalMediaEntry!
                                                            ) &&
            this.isVideoMeetStreamTrackAssignedToMediaEntry(internalMediaEntry!
                                                            ))) {
        for (const [meetStreamTrack, internalMeetStreamTrack] of this
                 .internalMeetStreamTrackMap.entries()) {
          const receiver = internalMeetStreamTrack.receiver;
          const contributingSources: RTCRtpContributingSource[] =
              receiver.getContributingSources();
          for (const contributingSource of contributingSources) {
            if (contributingSource.source === internalMediaEntry!.audioCsrc) {
              internalMediaEntry!.audioMeetStreamTrack.set(meetStreamTrack);
              // If Video stream is already assigned correctly, break.
              if (this.isVideoMeetStreamTrackAssignedToMediaEntry(
                      internalMediaEntry!)) {
                break;
              }
            }
            if (contributingSource.source === internalMediaEntry!.videoCsrc) {
              internalMediaEntry!.videoMeetStreamTrack.set(meetStreamTrack);
              // If Audio stream is already assigned correctly, break.
              if (this.isAudioMeetStreamTrackAssignedToMediaEntry(
                      internalMediaEntry!)) {
                break;
              }
            }
          }
        }
      }
    });

    // Update media entry collection.
    if ((data.resources && data.resources.length > 0) ||
        (data.deletedResources && data.deletedResources.length > 0)) {
      const newMediaEntryArray = [...mediaEntryArray, ...addedMediaEntries];
      this.mediaEntriesDelegate.set(newMediaEntryArray);
    }
  }

  private isAudioMeetStreamTrackAssignedToMediaEntry(
      internalMediaEntry: InternalMediaEntry): boolean {
    if (!internalMediaEntry.audioCsrc) return false;
    const audioStreamTrack = internalMediaEntry.audioMeetStreamTrack.get();
    if (!audioStreamTrack) return false;
    const internalAudioMeetStreamTrack =
        this.internalMeetStreamTrackMap.get(audioStreamTrack);
    // This is not expected. Map should be comprehensive of all meet stream
    // tracks.
    if (!internalAudioMeetStreamTrack) return false;
    const contributingSources: RTCRtpContributingSource[] =
        internalAudioMeetStreamTrack.receiver.getContributingSources();

    let audioCsrcFound = false;
    for (const contributingSource of contributingSources) {
      if (contributingSource.source === internalMediaEntry.audioCsrc) {
        audioCsrcFound = true;
        break;
      }
    }
    return audioCsrcFound;
  }

  private isVideoMeetStreamTrackAssignedToMediaEntry(
      internalMediaEntry: InternalMediaEntry): boolean {
    if (!internalMediaEntry.videoSsrc) return false;
    const videoStreamTrack = internalMediaEntry.videoMeetStreamTrack.get();
    if (!videoStreamTrack) return false;
    const internalVideoMeetStreamTrack =
        this.internalMeetStreamTrackMap.get(videoStreamTrack);
    // This is not expected. Map should be comprehensive of all meet stream
    // tracks.
    if (!internalVideoMeetStreamTrack) return false;
    const contributingSources: RTCRtpContributingSource[] =
        internalVideoMeetStreamTrack.receiver.getContributingSources();

    let videoCsrcFound = false;
    for (const contributingSource of contributingSources) {
      if (contributingSource.source === internalMediaEntry.videoCsrc) {
        videoCsrcFound = true;
        break;
      }
    }
    return videoCsrcFound;
  }
}
