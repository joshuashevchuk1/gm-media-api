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
 * @fileoverview Meet Media Api types and interfaces.
 */

import {Subscribable} from './subscribable';

/**
 * Media entry serves as the central relational object between the
 * participant, media canvas and meet stream. This object represents media in
 * a Meet call and holds metadata for the media.
 */
export interface MediaEntry {
  // We expect participant to be set once.
  readonly participant: Subscribable<Participant|undefined>;
  readonly audioMuted: Subscribable<boolean>;
  readonly videoMuted: Subscribable<boolean>;
  readonly screenShare: Subscribable<boolean>;
  readonly isPresenter: Subscribable<boolean>;
  readonly mediaLayout: Subscribable<MediaLayout|undefined>;
  readonly videoMeetStreamTrack: Subscribable<MeetStreamTrack|undefined>;
  readonly audioMeetStreamTrack: Subscribable<MeetStreamTrack|undefined>;
}

/**
 * A representation of a participant in a media api client.
 * This maps to
 * https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants#Participant.
 */
export interface Participant {
  // Resource name of the participant.
  // Format: `conferenceRecords/{conferenceRecord}/participants/{participant}`
  readonly name: string;
  /** Additional metadata about the participant. */
  readonly participantInfo: SignedInUser|AnonymousUser|PhoneUser;
  /**
   * The media entries associated with this participant. These can be
   * transient. There is one participant to many media entries relationship.
   */
  readonly mediaEntries: Subscribable<MediaEntry[]>;
}

// A signed in user in a Meet call.
interface SignedInUser {
  /**
   * Unique ID of the user which is interoperable with Admin SDK API and People
   * API.
   */
  readonly user: string;
  /**
   * Display name of the user. First and last name for a personal device. Admin
   * defined name for a robot account.
   */
  readonly displayName: string;
}

// An anonymous user in a Meet call.
interface AnonymousUser {
  /** User specified display name. */
  readonly displayName: string;
}

// A dial-in user in a Meet call.
interface PhoneUser {
  /** Partially redacted user's phone number. */
  readonly displayMame: string;
}

/**
 * An abstraction of a track in a Meet stream. This is used to represent
 * both audio and video tracks and their relationship to Media Entries.
 */
export interface MeetStreamTrack {
  /** WebRTC media stream track. */
  readonly mediaStreamTrack: MediaStreamTrack;
  /**
   * The media entry associated with this track. This a one to one
   * relationship.
   */
  readonly mediaEntry: Subscribable<MediaEntry|undefined>;
}

/**
 * The dimensions of the canvas.
 */
export interface CanvasDimensions {
  /** Width measured in pixels. This can be changed by the user. */
  width: number;
  /** Height measured in pixels. This can be changed by the user. */
  height: number;
}

/**
 * A Media layout for the Media Api Web client. This must be created by the
 * Media API client to be valid.
 */
export interface MediaLayout {
  /** The dimensions of the layout. */
  readonly canvasDimensions: CanvasDimensions;
  /** The media entry associated with this layout. */
  readonly mediaEntry: Subscribable<MediaEntry|undefined>;
}

/**
 * A request for a MediaLayout. This is required to be able to request a video
 * stream. Can be used to request a presenter, direct or relevant media layout.
 */
export type MediaLayoutRequest =
    // Used to request or change the presenter stream if it exists.
    {presenter: true, mediaLayout: MediaLayout, mediaEntry?: never}|
    // Used to request a direct media layout. This can be any media entry in the
    // meeting.
    {mediaEntry: MediaEntry, mediaLayout: MediaLayout, presenter?: never}|
    // Used to request a relevant media layout. This is a media layout that
    // the backend determines is relevant to the user. This could be a current
    // loudest speaker, presenter, etc.
    {mediaLayout: MediaLayout, mediaEntry?: never, presenter?: never};

/**
 * Enum for the status of the Meet session.
 */
export enum MeetSessionStatus {
  NEW = 0, /* Default value */
  WAITING = 1,
  JOINED = 2,
  DISCONNECTED = 3,
  KICKED = 4,   /* DISCONNECTED with leave request */
  REJECTED = 5, /* Error state */
}

/**
 * Required configuration for the MeetMediaApiClient.
 */
export interface MeetMediaClientRequiredConfiguration {
  meetingSpaceId: string;
  numberOfVideoStreams: number;
  /* Number of audio streams is not configurable. False maps to 0 and True maps
   * to 3.
   */
  enableAudioStreams: boolean;
  accessToken: string;
}
