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
 * @fileoverview Data channel interfaces for the Google Meet Media API.
 */

// COMMON

/**
 * Base interface for all requests.
 */
export declare interface MediaApiRequest {
  /**
   * A unique client-generated identifier for this request. Different requests
   * must never have the same request ID in the scope of one data channel.
   */
  requestId: number;
}

/**
 * Base interface for all responses.
 */
export declare interface MediaApiResponseStatus {
  code: number;
  message: string;
  // tslint:disable-next-line:no-any
  details: any[];
}

/**
 * Base interface for all responses.
 */
export declare interface MediaApiResponse {
  /** ID of the associated request. */
  requestId: number;
  /** Response status for the request. */
  status: MediaApiResponseStatus;
}

/**
 * Base interface for all resource snapshots.
 */
export declare interface ResourceSnapshot {
  /**
   * The resource ID of the resource being updated. For singleton resources,
   * this is unset.
   */
  id?: number;
}

/**
 * Base interface for all deleted resources.
 */
export declare interface DeletedResource {
  /** ID of the deleted resource. */
  id: number;
}

// SESSION CONTROL

/**
 * Data channel for session control.
 */
export declare interface SessionControlChannelFromClient {
  request: LeaveRequest;
}

/**
 * Data channel for session control.
 */
export declare interface SessionControlChannelToClient {
  /** An optional response to an incoming request. */
  response?: LeaveResponse;
  /**
   * List of resource snapshots managed by the server, with no implied order.
   */
  resources?: SessionStatus[];
}

/**
 * Tells the server the client is about to disconnect. After receiving the
 * response, the client should not expect to receive any other messages or media
 * RTP.
 */
export declare interface LeaveRequest extends MediaApiRequest {
  leave: {};
}

/**
 * Response to a leave request.
 */
export declare interface LeaveResponse extends MediaApiResponse {
  leave: {};
}

/** Singleton resource containing the status of the media session. */
export declare interface SessionStatus extends ResourceSnapshot {
  sessionStatus: {
    /**
     * - `STATE_WAITING`: Session is waiting to be admitted into the meeting.
     * The client may never observe this state if it was admitted or rejected
     *   quickly.
     * - `STATE_JOINED`: Session has fully joined the meeting.
     * - `STATE_DISCONNECTED`: Session is not connected to the meeting.
     */
    connectionState: 'STATE_WAITING'|'STATE_JOINED'|'STATE_DISCONNECTED';
  };
}

// PARTICIPANTS

/**
 * Data channel for participants.
 */
export declare interface ParticipantsChannelToClient {
  /**
   * List of resource snapshots managed by the server, with no implied order.
   */
  resources?: LegacyParticipant|Participant[];
  /** List of deleted resources with no implied order. */
  deletedResources?: DeletedParticipant[];
}

/**
 * Resource snapshot for a participant.
 */
export declare interface LegacyParticipant extends ResourceSnapshot {
  participantId: number;
  participantInfo: {
    /** Human readable name of a participant in the meeting */
    displayName: string; avatarUrl: string;
  };
  /**
   * - `PERSON`: The participant is a single person using the Meet
   *   application from a web or mobile device.
   * - `ROOM_DEVICE`: The participant is a rooms device in a meeting
   *   space.
   * - `DIAL_IN`: The participant is a telephony client that has dialed
   *   in.
   */
  identityType: 'PERSON'|'ROOM_DEVICE'|'DIAL_IN';
}

/**
 * Resource snapshot for a participant.
 */
export declare interface Participant extends ResourceSnapshot {
  participantId: number;
  participantInfo: {
    /** Human readable name of a participant in the meeting */
    displayName: string; avatarUrl: string;
  };
  /**
   * - `PERSON`: The participant is a single person using the Meet
   *   application from a web or mobile device.
   * - `ROOM_DEVICE`: The participant is a rooms device in a meeting
   *   space.
   * - `DIAL_IN`: The participant is a telephony client that has dialed
   *   in
   */
  identityType: 'PERSON'|'ROOM_DEVICE'|'DIAL_IN';
}

/**
 * Deleted resource for a participant.
 */
export declare interface DeletedParticipant extends DeletedResource {
  participant: boolean;
}

// MEDIA ENTRIES

/**
 * Data channel for media entries.
 */
export declare interface MediaEntriesChannelToClient {
  /**
   * List of resource snapshots managed by the server, with no implied order.
   */
  resources?: MediaEntry[];
  /** List of deleted resources with no implied order. */
  deletedResources?: DeletedMediaEntry[];
}

/**
 * Resource snapshot for a media entry.
 */
export declare interface MediaEntry extends ResourceSnapshot {
  mediaEntry: {
    /**
     * ID associated with the participant producing the stream. Use this to
     * correlate with other media entries produced by the same participant.
     * For example, a participant with multiple devices active in the same
     * meeting.
     */
    participantId: number;
    /** CSRC for any audio stream contributed by this participant. */
    audioCsrc?: number;
    /** CSRCs for any video streams contributed by this participant. */
    videoCsrcs: number[];
    /** Whether the current entry is presentating. */
    presenter: boolean;
    /** Whether the current entry is a screenshare. */
    screenshare: boolean;
    /** Whether this participant muted their audio stream. */
    audioMuted: boolean;
    /** Whether this participant muted their video stream. */
    videoMuted: boolean;
  };
}

/**
 * Deleted resource for a media entry.
 */
export declare interface DeletedMediaEntry extends DeletedResource {
  mediaEntry: boolean;
}

// VIDEO ASSIGNMENT

/**
 * Data channel for video assignment.
 */
export declare interface VideoAssignmentChannelToClient {
  /** An optional response to an incoming request. */
  response?: SetVideoAssignmentResponse;
  /**
   * Resource snapshots managed by the server.
   */
  resources?: VideoAssignment[];
}

/**
 * Data channel for video assignment.
 */
export declare interface VideoAssignmentChannelFromClient {
  request: SetVideoAssignmentRequest;
}

/**
 * Dimensions of a canvas.
 */
export declare interface CanvasDimensions {
  /**
   * Height in square pixels. For cameras that can change orientation,
   * height refers to the measurement on the vertical axis.
   */
  height: number;
  /**
   * Width in square pixels. For cameras that can change orientation,
   * width refers to the measurement on the horizontal axis.
   */
  width: number;
}

/**
 * Video canvas for video assignment.
 */
export declare interface MediaApiCanvas {
  /**
   * ID for the video canvas. This is required and must be unique within
   * the containing layout model. Clients should prudently reuse these
   * IDs, as this allows the backend to keep assigning video streams to
   * the same canvas as much as possible.
   */
  id: number;
  dimensions: CanvasDimensions;
  relevant: {};
}

/**
 * Request to set video assignment.
 */
export declare interface SetVideoAssignmentRequest extends MediaApiRequest {
  setAssignment: {
    /** New video layout to use, replacing any previous layout. */
    layoutModel: {
      label: string;
      /**
       * Canvases to assign videos to from virtual SSRCs. Providing more
       * canvases than exists virtual streams will result in an error status.
       */
      canvases: MediaApiCanvas[];
    };
    /**
     * Maximum video resolution the client wants to receive for any video
     * feeds.
     */
    maxVideoResolution: {
      /**
       * Height in square pixels. For cameras that can change orientation,
       * height refers to the measurement on the vertical axis.
       */
      height: number;
      /**
       * Width in square pixels. For cameras that can change orientation,
       * width refers to the measurement on the horizontal axis.
       */
      width: number;
      /** Frames per second. */
      frameRate: number;
    };
  };
}

/**
 * Response to a set video assignment request.
 */
export declare interface SetVideoAssignmentResponse extends MediaApiResponse {
  setAssignment: {};
}

/**
 * Singleton resource describing how video streams are assigned to video
 * canvases specified in the client's video layout model.
 */
export declare interface VideoAssignment extends ResourceSnapshot {
  videoAssignment: {
    /** Label of the layout model. */
    label: string;
    /** Canvas assignments, with no implied order. */
    canvases: Array<{
      /** The video canvas the video should be shown in. */
      canvasId: number;
      /**
       * The virtual video SSRC that the video will be sent over, or
       * unset if no video from the participant.
       */
      ssrc?: number;
      /** ID of the media entry associated with the video stream. */
      mediaEntryId: number;
    }>;
  };
}

// MEDIA STATS

/**
 * Data channel for media stats.
 */
export declare interface MediaStatsChannelToClient {
  /** An optional response to an incoming request. */
  response?: UploadMediaStatsResponse;
  resources?: MediaStatsResource[];
}

/**
 * Resource snapshot for media stats. Managed by the server.
 */
export declare interface MediaStatsResource extends ResourceSnapshot {
  configuration: MediaStatsConfiguration;
}

/**
 * Configuration for media stats.
 */
export declare interface MediaStatsConfiguration {
  /**
   * The interval between each upload of media stats. If this is zero, the
   * client should not upload any media stats.
   */
  uploadIntervalSeconds: number;
  /**
   * A map of allowlisted sections. The key is the section type, and the value
   * is the keys that are allowlisted for that section.
   */
  allowlist: Map<string, {key: string[]}>;
}

/**
 * Data channel for media stats.
 */
export declare interface MediaStatsChannelFromClient {
  request: UploadMediaStatsRequest;
}

/**
 * Uploads media stats from the client to the server. The stats are
 * retrieved from WebRTC by calling RTCPeerConnection::getStats. The
 * returned RTCStatsReport can be easily mapped to the sections below.
 */
export declare interface UploadMediaStatsRequest extends MediaApiRequest {
  uploadMediaStats: {
    /**
     * Represents the entries in
     * https://w3c.github.io/webrtc-pc/#dom-rtcstatsreport.
     */
    sections: StatsSection[];
  };
}

/**
 * A section of media stats.
 */
export declare interface StatsSection {
  id: string;
}

// STATS SECTION TYPES
// Mapped Types are being used instead of interfaces to allow for objects to be
// indexed by string.

/**
 * Stats section types.
 */
export declare interface StatTypes {
  // Need to use underscore naming to conform to expected structure for data
  // channel.
  // tslint:disable: enforce-name-casing
  candidate_pair: CandidatePairSection;
  codec: CodecSection;
  inbound_rtp: InboundRtpSection;
  media_playout: MediaPlayoutSection;
  transport: TransportSection;
  local_candidate: IceCandidateSection;
  remote_candidate: IceCandidateSection;
  // tslint:enable: enforce-name-casing
}

/**
 * A section of media stats.
 */
export declare type StatsSectionData = StatsSection & {
  [key in keyof StatTypes]?: StatTypes[key];
};

type CodeSectionStringFields = 'mime_type';
type CodecSectionNumberFields = 'payload_type';

/**
 * Codec fields.
 */
export declare type CodecSectionFields =
    CodeSectionStringFields | CodecSectionNumberFields;

/** https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-codec */
export declare type CodecSection = {
  [key in CodecSectionFields]?: key extends CodeSectionStringFields ? string :
                                                                      number;
};

type InboundRtpSectionStringFields = 'codec_id'|'kind';

type InboundRtpSectionNumberFields = 'ssrc'|'jitter'|'packets_lost'|
    'packets_received'|'bytes_received'|'jitter_buffer_delay'|
    'jitter_buffer_emitted_count'|'jitter_buffer_minimum_delay'|
    'jitter_buffer_target_delay'|'total_audio_energy'|'fir_count'|
    'frame_height'|'frame_width'|'frames_decoded'|'frames_dropped'|
    'frames_per_second'|'frames_received'|'freeze_count'|'key_frames_decoded'|
    'nack_count'|'pli_count'|'retransmitted_packets_received'|
    'total_freezes_duration'|'total_pauses_duration'|'audio_level'|
    'concealed_samples'|'total_samples_received'|'total_samples_duration';

/** Inbound Rtp fields. */
export type InboundRtpSectionFields =
    InboundRtpSectionStringFields|InboundRtpSectionNumberFields;

/** https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-inbound-rtp */
export declare type InboundRtpSection = {
  [key in InboundRtpSectionFields]?:
      key extends InboundRtpSectionStringFields ? string : number;
};

/** Candidate pair fields. */
export type CandidatePairSectionFields = 'available_outgoing_bitrate'|
    'bytes_received'|'bytes_sent'|'current_round_trip_time'|'packets_received'|
    'packets_sent'|'total_round_trip_time';

/** https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-candidate-pair */
export declare type CandidatePairSection = {
  [key in CandidatePairSectionFields]: number;
};

/**
 * Media playout fields.
 * https://www.w3.org/TR/webrtc-stats/#dom-rtcaudioplayoutstats
 */
export type MediaPlayoutSectionFields =
    'synthesized_samples_duration'|'synthesized_samples_events'|
    'total_samples_duration'|'total_playout_delay'|'total_samples_count';

/**
 * https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-media-playout
 */
export declare type MediaPlayoutSection = {
  [key in MediaPlayoutSectionFields]?: number;
};

/**
 * Transport fields.
 * https://www.w3.org/TR/webrtc-stats/#transportstats-dict
 */
export type TransportSectionFields = 'selected_candidate_pair_id';

/** https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-transport. */
export declare type TransportSection = {
  [key in TransportSectionFields]?: string;
};


type IceCandidateSectionStringFields =
    'address'|'candidate_type'|'protocol'|'network_type';
type IceCandidateSectionNumberFields = 'port';

/**
 * Ice candidate fields.
 *  https://www.w3.org/TR/webrtc-stats/#dom-rtcicecandidatestats
 */
export declare type IceCandidateSectionFields =
    IceCandidateSectionStringFields | IceCandidateSectionNumberFields;

/**
 * https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-local-candidate or
 * https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-remote-candidate.
 */
export declare type IceCandidateSection = {
  [key in IceCandidateSectionFields]?:
      key extends IceCandidateSectionStringFields ? string : number;
};

/**
 * Response to a media stats upload request.
 */
export declare interface UploadMediaStatsResponse extends MediaApiResponse {
  uploadMediaStats: {};
}
