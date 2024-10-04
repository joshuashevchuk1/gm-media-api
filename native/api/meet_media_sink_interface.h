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

#ifndef NATIVE_API_MEET_MEDIA_SINK_INTERFACE_H_
#define NATIVE_API_MEET_MEDIA_SINK_INTERFACE_H_

// This file contains the interfaces necessary for receiving audio and video
// frames from the Meet Media API client.
//
// Media does not follow the normal WebRTC paradigm for a Meet Media API
// session. For typical WebRTC sessions, one SRTP stream exists for every remote
// participant. Media from a given remote participant is always sent across the
// same SRTP stream.
//
// This is NOT the case for Meet Media API sessions. Instead, up to `number of
// desired video streams` SRTP streams are created for video media. If audio is
// enabled, exactly three SRTP streams are created for audio media. Each of
// these streams will have a unique and stable synchronization source identifier
// (SSRC).
//         csrc: 1                                   csrc: 3
//      ┌ ----^----------------------------------------^----- ┐
//      |     |           Meet Media API Client        |      |
//      └ —---^----------------------------------------^----- ┘
//        |   |   |                                |   |   |
//        |   ^   |ssrc:1234                       |   ^   |ssrc:5678
//        |   |   |                                |   |   |
//        |   ^   |                                |   ^   |
//      ┌ ----|----------------------------------------|----- ┐
//      |     ^              Meet Servers              ^      |
//      └ —---|----------------------------------------|----- ┘
//            ^                                        ^
//       _____|                                   _____|
//       ^                                        ^
//       |                                        |
//  ┌         ┐         ┌         ┐         ┌         ┐         ┌         ┐
//  |  John   |         |  Jack   |         |   Jim   |         |  Jose   |
//  └         ┘         └         ┘         └         ┘         └         ┘
//    csrc: 1             csrc: 2             csrc: 3             csrc: 4
//
// For example, John, Jack, Jim, and Jose are all remote participants in
// the conference. Each of them have a unique CSRC value.
//
// The client has two video SRTP streams. Each SRTP stream has a unique SSRC
// value. The server will determine which two remote participants are
// relevant out the four. It will then transmit those streams across the two
// SRTP connections. Contained within the media frames will be the CSRC value.
//
// If at any point the backend has determined relevancy has changed, it will
// "switch out" the video stream(s) of the least relevant participant(s) for
// the video stream(s) of the most relevant participant(s).
//
//         csrc: 1                                   csrc: 2
//      ┌ ----^----------------------------------------^----- ┐
//      |     |           Meet Media API Client        |      |
//      └ —---^----------------------------------------^----- ┘
//        |   |   |                                |   |   |
//        |   ^   |ssrc:1234                       |   ^   |ssrc:5678
//        |   |   |                                |   |   |
//        |   ^   |                                |   ^   |
//      ┌ ---------------------------------------------|----- ┐
//      |     ^              Meet Servers              ^      |
//      └ —---|----------------------------------------|----- ┘
//            ^                                        ^
//       _____|              __________________________|
//       ^                   ^
//       |                   |
//  ┌         ┐         ┌         ┐         ┌         ┐         ┌         ┐
//  |  John   |         |  Jack   |         |   Jim   |         |  Jose   |
//  └         ┘         └         ┘         └         ┘         └         ┘
//    csrc: 1             csrc: 2             csrc: 3             csrc: 4
//
// The SSRC values will always remain the same. The CSRC values will change
// depending on which remote participant is being sent across the SRTP stream.
// Using the `Media Entries` and `Participants` resource, the client can lookup
// which remote participant the CSRC belongs to.
//
// A remote participant's CSRC value will never change as long as they remain
// in the session. If a participant leaves, but then rejoins, this is treated
// as a new participant. Hence, a new CSRC value may be generated.
//
// This anecdotal example holds true for any number combination of audio and
// video streams.
//
// For M media streams and N remote participants:
// * if M > N, then only N streams will be transmitting media. The remaining
//   (M-N) streams will be idle.
//
// * if M < N, then M streams will be transmitting media. The remaining
//   (N-M) participants will be transmitted only if they become more relevant.
//
// * if M = N, then all streams will be transmitting media from all remote
// participants.

#include <cstdint>
#include <optional>

#include "absl/types/span.h"
#include "webrtc/api/ref_count.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame.h"

namespace meet {

// Interface for receiving information about meet media streams regardless of
// media type.
class MeetMediaSinkInterface : public webrtc::RefCountInterface {
 public:
  virtual ~MeetMediaSinkInterface() = default;

  // Media Streams are agreed upon with the backend after exchanging offers.
  // However, the SSRCs value aren't exchanged in the offer<->answer flow.
  // Instead, the SSRC value for a given stream is made known once RTP packets
  // have actually started flowing.
  //
  // This method will notify the implementor of the SSRC for the meet media
  // stream the sink is registered with once the first media frame has arrived.
  virtual void OnFirstFrameReceived(uint32_t ssrc) = 0;
};

// Interface that is registered with a Meet media stream to receive video
// frames.
//
// A sink implementation determines which SSRC it is receiving media frames
// from by listening to the `OnFirstFrameReceived` method of the
// MeetMediaSinkInterface. If no frame has been received, this is only an
// indication that the meet media stream the sink is registered with has not yet
// begun receving RTP packets from the backend.
//
// Scenarios where this would occur include:
// - The MeetMediaApi client has not yet been fully admitted into the
// conference.
// - There are fewer remote participants in the conference than the number of
// desired video streams configured.
// - Remote participants video streams are muted at join time.
// - The MeetMediaApi client has not successfully established a persistent DTLS
// connection with the backend.
class MeetVideoSinkInterface : public MeetMediaSinkInterface {
 public:
  struct MeetVideoFrame {
    const webrtc::VideoFrame& frame;
    // Contributing source of the current video frame. This ID is used to
    // identify which participant in the conference generated the frame.
    // Integrators can cross reference this value with values pushed from Meet
    // servers to the client over the `Media Entries` resource data channel.
    std::optional<uint32_t> csrc;
  };

  virtual void OnFrame(const MeetVideoFrame& frame) = 0;
};

// Interface that is registered with a Meet media stream to receive audio
// frames.
//
// A sink implementation determines which SSRC it is receiving media frames
// from by listening to the `OnFirstFrameReceived` method of the
// MeetMediaSinkInterface. If no frame has been received, this is only an
// indication that the meet media stream the sink is registered with has not yet
// begun receving RTP packets from the backend.
//
// Scenarios where this would occur include:
// - The MeetMediaApi client has not yet been fully admitted into the
// conference.
// - There are fewer remote participants in the conference than the number of
// desired audio streams configured.
// - Remote participants audio streams are muted at join time.
// - The MeetMediaApi client has not successfully established a persistent DTLS
// connection with the backend.
class MeetAudioSinkInterface : public MeetMediaSinkInterface {
 public:
  struct MeetAudioFrame {
    struct AudioData {
      absl::Span<const int16_t> pcm16;
      int bits_per_sample;
      int sample_rate;
      int number_of_channels;
      int number_of_frames;
    };

    AudioData audio_data;
    // Contributing source of the current audio frame. This ID is used to
    // identify which participant in the conference generated the frame.
    // Integrators can cross reference this value with values pushed from Meet
    // servers to the client over the `Media Entries` resource data channel.
    std::optional<uint32_t> csrc;
  };

  virtual void OnFrame(const MeetAudioFrame& frame) = 0;
};

// Factory for creating instances of `MeetVideoSinkInterface` and
// `MeetAudioSinkInterface`.
//
// The integrating codebase is to provide an instance of this interface. This is
// used by the Meet Media API client to create media sinks for every incoming
// media stream.
class MeetMediaSinkFactoryInterface : public webrtc::RefCountInterface {
 public:
  // Creates a new video sink for the Media APi session.  Invoked once for every
  // signaled video transceiver. This will match the number of desired video
  // streams provided to the factory method.
  virtual rtc::scoped_refptr<MeetVideoSinkInterface> CreateVideoSink() = 0;
  // Creates a new audio sink for the Media APi session.  Invoked once for every
  // signaled audio transceiver. If audio is enabled for the session, this will
  // always be invoked three times.
  virtual rtc::scoped_refptr<MeetAudioSinkInterface> CreateAudioSink() = 0;

  virtual ~MeetMediaSinkFactoryInterface() override = default;
};

}  // namespace meet

#endif  // NATIVE_API_MEET_MEDIA_SINK_INTERFACE_H_
