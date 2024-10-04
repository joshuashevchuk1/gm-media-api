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

#include "native/internal/meet_media_streams.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/types/optional.h"
#include "native/api/meet_media_sink_interface.h"
#include "native/internal/testing/mock_meet_media_sink_factory.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/rtp_headers.h"
#include "webrtc/api/rtp_packet_info.h"
#include "webrtc/api/rtp_packet_infos.h"
#include "webrtc/api/rtp_receiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/test/mock_media_stream_interface.h"
#include "webrtc/api/test/mock_rtp_transceiver.h"
#include "webrtc/api/test/mock_rtpreceiver.h"
#include "webrtc/api/test/mock_video_track.h"
#include "webrtc/api/transport/rtp/rtp_source.h"
#include "webrtc/api/units/timestamp.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/rtc_base/ref_counted_object.h"

namespace meet {
namespace {
using ::base_logging::ERROR;
using ::base_logging::WARNING;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::SizeIs;

// TODO: Update these and all Mocks to NiceMocks.
class MockMeetVideoSink : public rtc::RefCountedObject<MeetVideoSinkInterface> {
 public:
  static rtc::scoped_refptr<MockMeetVideoSink> Create() {
    return rtc::scoped_refptr<MockMeetVideoSink>(new MockMeetVideoSink());
  }

  MOCK_METHOD(void, OnFrame, (const MeetVideoFrame&), (override));
  MOCK_METHOD(void, OnFirstFrameReceived, (uint32_t), (override));
};

class MockMeetAudioSink : public rtc::RefCountedObject<MeetAudioSinkInterface> {
 public:
  static rtc::scoped_refptr<MockMeetAudioSink> Create() {
    return rtc::scoped_refptr<MockMeetAudioSink>(new MockMeetAudioSink());
  }

  MOCK_METHOD(void, OnFrame, (const MeetAudioFrame&), (override));
  MOCK_METHOD(void, OnFirstFrameReceived, (uint32_t), (override));
};

class MockMeetVideoStreamTrack : public MeetVideoStreamTrack {
 public:
  MockMeetVideoStreamTrack(
      std::string mid, rtc::scoped_refptr<MeetVideoSinkInterface> external_sink,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      : MeetVideoStreamTrack(std::move(mid), external_sink,
                             std::move(receiver)) {}

  MOCK_METHOD(void, OnFrame, (const webrtc::VideoFrame&), (override));
};

class MockMeetAudioStreamTrack : public MeetAudioStreamTrack {
 public:
  MockMeetAudioStreamTrack(
      std::string mid, rtc::scoped_refptr<MeetAudioSinkInterface> external_sink,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      : MeetAudioStreamTrack(std::move(mid), external_sink,
                             std::move(receiver)) {}

  MOCK_METHOD(void, OnData,
              (const void*, int, int, size_t, size_t, absl::optional<int64_t>),
              (override));
};

class UpdatedMockRtpReceiver : public webrtc::MockRtpReceiver {
 public:
  static rtc::scoped_refptr<UpdatedMockRtpReceiver> Create() {
    return rtc::scoped_refptr<UpdatedMockRtpReceiver>(
        new UpdatedMockRtpReceiver());
  }

  MOCK_METHOD(std::vector<std::string>, stream_ids, (), (const, override));
};

TEST(MeetMediaStreamManagerTest, OnRemoteTrackAddedInvokesCreateVideoSink) {
  rtc::scoped_refptr<webrtc::MockVideoTrack> mock_video_track =
      webrtc::MockVideoTrack::Create();

  EXPECT_CALL(*mock_video_track, AddOrUpdateSink).Times(1);

  rtc::scoped_refptr<webrtc::MockRtpReceiver> receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  EXPECT_CALL(*receiver, media_type)
      .WillOnce(Return(cricket::MediaType::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(*receiver, track).WillOnce(Return(mock_video_track));

  rtc::scoped_refptr<webrtc::MockRtpTransceiver> transceiver =
      webrtc::MockRtpTransceiver::Create();

  ON_CALL(*transceiver, receiver).WillByDefault(Return(receiver));
  ON_CALL(*transceiver, mid).WillByDefault(Return("mid"));

  rtc::scoped_refptr<MockMeetMediaSinkFactory> mock_sink_factory =
      MockMeetMediaSinkFactory::Create();

  EXPECT_CALL(*mock_sink_factory, CreateVideoSink).Times(1);

  MeetMediaStreamManager media_stream_manager(mock_sink_factory);
  media_stream_manager.OnRemoteTrackAdded(transceiver);
}

TEST(MeetMediaStreamManagerTest, OnRemoteTrackAddedInvokesCreateAudioSink) {
  rtc::scoped_refptr<webrtc::MockAudioTrack> mock_audio_track =
      webrtc::MockAudioTrack::Create();

  EXPECT_CALL(*mock_audio_track, AddSink).Times(1);

  rtc::scoped_refptr<webrtc::MockRtpReceiver> receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  EXPECT_CALL(*receiver, media_type)
      .WillOnce(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(*receiver, track).WillOnce(Return(mock_audio_track));

  rtc::scoped_refptr<webrtc::MockRtpTransceiver> transceiver =
      webrtc::MockRtpTransceiver::Create();

  ON_CALL(*transceiver, receiver).WillByDefault(Return(receiver));
  ON_CALL(*transceiver, mid).WillByDefault(Return("mid"));

  rtc::scoped_refptr<MockMeetMediaSinkFactory> mock_sink_factory =
      MockMeetMediaSinkFactory::Create();

  EXPECT_CALL(*mock_sink_factory, CreateAudioSink).Times(1);

  MeetMediaStreamManager media_stream_manager(mock_sink_factory);
  media_stream_manager.OnRemoteTrackAdded(transceiver);
}

TEST(MeetMediaStreamManagerTest, OnRemoteTrackAddedLogsUnsupportedMediaType) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  EXPECT_CALL(*receiver, media_type)
      .WillOnce(Return(cricket::MediaType::MEDIA_TYPE_UNSUPPORTED));

  rtc::scoped_refptr<webrtc::MockRtpTransceiver> transceiver =
      webrtc::MockRtpTransceiver::Create();

  EXPECT_CALL(*transceiver, receiver).WillOnce(Return(receiver));

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([](int, const std::string&, const std::string& msg) {
        EXPECT_THAT(
            msg, HasSubstr("Received remote track of unsupported media type:"));
      });
  log.StartCapturingLogs();

  rtc::scoped_refptr<MockMeetMediaSinkFactory> mock_sink_factory =
      MockMeetMediaSinkFactory::Create();

  auto media_stream_manager =
      std::make_unique<MeetMediaStreamManager>(mock_sink_factory);
  media_stream_manager->OnRemoteTrackAdded(transceiver);
}

TEST(MeetVideoStreamTrackTest, OnFrameNotifiesExternalVideoSink) {
  rtc::scoped_refptr<MockMeetVideoSink> mock_video_sink =
      MockMeetVideoSink::Create();

  EXPECT_CALL(*mock_video_sink, OnFirstFrameReceived)
      .WillOnce([](uint32_t ssrc) { EXPECT_EQ(ssrc, 123); });
  EXPECT_CALL(*mock_video_sink, OnFrame)
      .WillOnce([](const MeetVideoSinkInterface::MeetVideoFrame& frame) {
        EXPECT_EQ(frame.csrc, 42);
      });

  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  MeetVideoStreamTrack video_stream_track("mid", std::move(mock_video_sink),
                                          std::move(mock_receiver));

  webrtc::VideoFrame::Builder builder;
  webrtc::RtpPacketInfo packet_info;
  packet_info.set_ssrc(123);
  packet_info.set_csrcs({42});
  webrtc::RtpPacketInfos packet_infos({packet_info});
  builder.set_packet_infos(packet_infos);
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));

  video_stream_track.OnFrame(builder.build());
}

TEST(MeetAudioStreamTrackTest, OnFrameNotifiesExternalAudioSink) {
  int16_t pcm_data[100];
  int bits_per_sample = 16;
  int sample_rate = 48000;
  size_t number_of_channels = 2;
  size_t number_of_frames = 100;

  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  webrtc::RtpSource csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890), 99, webrtc::RtpSourceType::CSRC,
      424242,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890), 123, webrtc::RtpSourceType::SSRC,
      424242,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source), std::move(ssrc_rtp_source)}));

  rtc::scoped_refptr<MockMeetAudioSink> mock_audio_sink =
      MockMeetAudioSink::Create();

  EXPECT_CALL(*mock_audio_sink, OnFirstFrameReceived)
      .WillOnce([](uint32_t ssrc) { EXPECT_EQ(ssrc, 123); });
  EXPECT_CALL(*mock_audio_sink, OnFrame)
      .WillOnce([&](const MeetAudioSinkInterface::MeetAudioFrame& frame) {
        EXPECT_EQ(frame.csrc, 99);
        // Size is 200 because there are 2 channels.
        EXPECT_THAT(frame.audio_data.pcm16, SizeIs(200));
        EXPECT_EQ(frame.audio_data.bits_per_sample, bits_per_sample);
        EXPECT_EQ(frame.audio_data.sample_rate, sample_rate);
        EXPECT_EQ(frame.audio_data.number_of_channels, number_of_channels);
        EXPECT_EQ(frame.audio_data.number_of_frames, number_of_frames);
      });

  MeetAudioStreamTrack audio_stream_track("mid", std::move(mock_audio_sink),
                                          std::move(mock_receiver));

  audio_stream_track.OnData(pcm_data, bits_per_sample, sample_rate,
                            number_of_channels, number_of_frames,
                            absl::nullopt);
}

TEST(MeetVideoStreamTrackTest, OnFrameNotifiesFirstFrameOnlyOnce) {
  rtc::scoped_refptr<MockMeetVideoSink> mock_video_sink =
      MockMeetVideoSink::Create();

  EXPECT_CALL(*mock_video_sink, OnFirstFrameReceived).Times(1);

  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  MeetVideoStreamTrack video_stream_track("mid", std::move(mock_video_sink),
                                          std::move(mock_receiver));

  webrtc::VideoFrame::Builder builder;
  webrtc::RtpPacketInfo packet_info;
  packet_info.set_ssrc(123);
  packet_info.set_csrcs({42});
  webrtc::RtpPacketInfos packet_infos({packet_info});
  builder.set_packet_infos(packet_infos);
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));

  webrtc::VideoFrame frame = builder.build();
  video_stream_track.OnFrame(frame);
  video_stream_track.OnFrame(frame);
}

TEST(MeetAudioStreamTrackTest, OnFrameNotifiesFirstFrameOnlyOnce) {
  int16_t pcm_data[100];
  int bits_per_sample = 16;
  int sample_rate = 48000;
  size_t number_of_channels = 2;
  size_t number_of_frames = 100;

  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver =
      rtc::scoped_refptr<webrtc::MockRtpReceiver>(
          new webrtc::MockRtpReceiver());

  webrtc::RtpSource csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890), 42, webrtc::RtpSourceType::CSRC,
      424242,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890), 123, webrtc::RtpSourceType::SSRC,
      424242,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  ON_CALL(*mock_receiver, GetSources)
      .WillByDefault(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source), std::move(ssrc_rtp_source)}));

  rtc::scoped_refptr<MockMeetAudioSink> mock_audio_sink =
      MockMeetAudioSink::Create();

  EXPECT_CALL(*mock_audio_sink, OnFirstFrameReceived).Times(1);

  MeetAudioStreamTrack audio_stream_track("mid", std::move(mock_audio_sink),
                                          std::move(mock_receiver));

  audio_stream_track.OnData(pcm_data, bits_per_sample, sample_rate,
                            number_of_channels, number_of_frames,
                            absl::nullopt);
  audio_stream_track.OnData(pcm_data, bits_per_sample, sample_rate,
                            number_of_channels, number_of_frames,
                            absl::nullopt);
}

TEST(MeetAudioStreamTrackTest,
     OnFrameWithSsrcValueOfZeroDoesNotNotifyFirstFrame) {
  int16_t pcm_data[100];
  int bits_per_sample = 16;
  int sample_rate = 48000;
  size_t number_of_channels = 2;
  size_t number_of_frames = 100;

  rtc::scoped_refptr<UpdatedMockRtpReceiver> mock_receiver(
      new UpdatedMockRtpReceiver());

  webrtc::RtpSource zero_ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890), 0, webrtc::RtpSourceType::SSRC,
      424242,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  ON_CALL(*mock_receiver, GetSources)
      .WillByDefault(Return(
          std::vector<webrtc::RtpSource>{std::move(zero_ssrc_rtp_source)}));
  ON_CALL(*mock_receiver, stream_ids)
      .WillByDefault(Return(std::vector<std::string>{"msid"}));

  rtc::scoped_refptr<MockMeetAudioSink> mock_audio_sink =
      MockMeetAudioSink::Create();

  EXPECT_CALL(*mock_audio_sink, OnFirstFrameReceived).Times(0);

  MeetAudioStreamTrack audio_stream_track("mid", std::move(mock_audio_sink),
                                          std::move(mock_receiver));

  audio_stream_track.OnData(pcm_data, bits_per_sample, sample_rate,
                            number_of_channels, number_of_frames,
                            absl::nullopt);
}

TEST(MeetVideoStreamTrackTest, OnFrameMissingSsrcLogsWarning) {
  rtc::scoped_refptr<MockMeetVideoSink> mock_video_sink =
      MockMeetVideoSink::Create();

  EXPECT_CALL(*mock_video_sink, OnFirstFrameReceived).Times(0);

  rtc::scoped_refptr<UpdatedMockRtpReceiver> mock_receiver =
      UpdatedMockRtpReceiver::Create();

  ON_CALL(*mock_receiver, stream_ids)
      .WillByDefault(Return(std::vector<std::string>{"msid"}));

  MeetVideoStreamTrack video_stream_track("mid", std::move(mock_video_sink),
                                          std::move(mock_receiver));

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(WARNING, _,
                       HasSubstr("First frame received for stream with MID")));

  log.StartCapturingLogs();

  webrtc::VideoFrame::Builder builder;
  webrtc::RtpPacketInfo packet_info;
  packet_info.set_csrcs({42});
  webrtc::RtpPacketInfos packet_infos({packet_info});
  builder.set_packet_infos(packet_infos);
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));

  video_stream_track.OnFrame(builder.build());
}

TEST(MeetAudioStreamTrackTest, OnFrameMissingSsrcLogsWarning) {
  rtc::scoped_refptr<UpdatedMockRtpReceiver> mock_receiver =
      UpdatedMockRtpReceiver::Create();

  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(Return(std::vector<webrtc::RtpSource>()));
  ON_CALL(*mock_receiver, stream_ids)
      .WillByDefault(Return(std::vector<std::string>{"msid"}));

  rtc::scoped_refptr<MockMeetAudioSink> mock_audio_sink =
      MockMeetAudioSink::Create();

  EXPECT_CALL(*mock_audio_sink, OnFirstFrameReceived).Times(0);

  MeetAudioStreamTrack audio_stream_track("mid", std::move(mock_audio_sink),
                                          std::move(mock_receiver));

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(WARNING, _,
                       HasSubstr("First frame received for stream with MID")));

  log.StartCapturingLogs();

  int16_t pcm_data[100];
  audio_stream_track.OnData(pcm_data, 16, 48000, 2, 100, absl::nullopt);
}

TEST(MeetAudioStreamTrackTest, OnDataWithAudioNot16BitsPerSampleLogsError) {
  rtc::scoped_refptr<UpdatedMockRtpReceiver> mock_receiver =
      UpdatedMockRtpReceiver::Create();

  EXPECT_CALL(*mock_receiver, GetSources).Times(0);
  EXPECT_CALL(*mock_receiver, stream_ids).Times(0);

  rtc::scoped_refptr<MockMeetAudioSink> mock_audio_sink =
      MockMeetAudioSink::Create();

  EXPECT_CALL(*mock_audio_sink, OnFirstFrameReceived).Times(0);

  MeetAudioStreamTrack audio_stream_track("mid", std::move(mock_audio_sink),
                                          std::move(mock_receiver));

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _, HasSubstr("Unsupported bits per sample")));

  log.StartCapturingLogs();

  int16_t pcm_data[100];
  audio_stream_track.OnData(pcm_data, 100, 48000, 2, 100, absl::nullopt);
}

}  // namespace
}  // namespace meet
