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

#include "native_with_state/internal/media_api_audio_device_module.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/modules/audio_device/include/mock_audio_transport.h"

namespace meet {
namespace {
using ::base_logging::WARNING;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::Lt;
using ::testing::ScopedMockLog;

TEST(MediaApiAudioDeviceModuleTest, SuccessfullyInvokeInitPlayout) {
  auto adm = rtc::make_ref_counted<MediaApiAudioDeviceModule>();
  EXPECT_EQ(adm->InitPlayout(), 0);
  EXPECT_TRUE(adm->Playing());
}

TEST(MediaApiAudioDeviceModuleTest, SuccessfullyInvokeStopPlayout) {
  webrtc::test::MockAudioTransport audio_transport;
  auto adm = rtc::make_ref_counted<MediaApiAudioDeviceModule>();
  adm->InitPlayout();

  EXPECT_EQ(adm->StopPlayout(), 0);
  EXPECT_FALSE(adm->Playing());
}

TEST(MediaApiAudioDeviceModuleTest,
     InitPlayoutAfterStopPlayoutIsSuccessful) {
  webrtc::test::MockAudioTransport audio_transport;
  auto adm = rtc::make_ref_counted<MediaApiAudioDeviceModule>();
  adm->InitPlayout();

  EXPECT_TRUE(adm->Playing());
  EXPECT_EQ(adm->StopPlayout(), 0);
  EXPECT_FALSE(adm->Playing());

  adm->InitPlayout();

  EXPECT_TRUE(adm->Playing());
}

TEST(MediaApiAudioDeviceModuleTest, InitPlayoutTwiceLogsError) {
  auto adm = rtc::make_ref_counted<MediaApiAudioDeviceModule>();
  ScopedMockLog log(kDoNotCaptureLogsYet);

  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([](int, const std::string&, const std::string& msg) {
        EXPECT_THAT(msg, HasSubstr("Init called on an already initialized"));
      });
  log.StartCapturingLogs();

  adm->InitPlayout();
  adm->InitPlayout();
}

TEST(MediaApiAudioDeviceModuleTest,
     InitPlayoutWithCallbackRegisteredInvokesCallback) {
  webrtc::test::MockAudioTransport audio_transport;

  // no need for synchronization since the `NeedMorePlayData` is never called
  // concurrently.
  std::vector<absl::Duration> callback_intervals_aggregate;
  std::optional<absl::Time> last_callback_time;
  absl::Notification test_done;
  int sample_count = 1;
  absl::Duration sample_avg = absl::ZeroDuration();
  EXPECT_CALL(audio_transport,
              NeedMorePlayData(480, sizeof(int16_t), 1, 48000, _, _, _, _))
      .WillRepeatedly([&]() {
        absl::Time callback_time = absl::Now();
        if (last_callback_time.has_value()) {
          sample_avg += ((callback_time - *last_callback_time - sample_avg) /
                         sample_count);
          sample_count++;
        }

        // For performing CLT analysis, after collecting 30 sample intervals and
        // updating the mean, cache the result and compute the mean for the next
        // 30 samples.
        if (sample_count == 30) {
          callback_intervals_aggregate.push_back(sample_avg);
          sample_count = 1;
          sample_avg = absl::ZeroDuration();
        }
        // After collecting 50 aggregate interval means, stop the test.
        if (callback_intervals_aggregate.size() == 50) {
          if (!test_done.HasBeenNotified()) {
            test_done.Notify();
          }
        }

        last_callback_time = callback_time;
        return 0;
      });

  auto adm = rtc::make_ref_counted<MediaApiAudioDeviceModule>();

  EXPECT_EQ(adm->RegisterAudioCallback(&audio_transport), 0);

  adm->InitPlayout();
  test_done.WaitForNotification();
  adm->StopPlayout();
  absl::c_sort(callback_intervals_aggregate);
  auto percentile = [&](int pct) {
    return callback_intervals_aggregate[pct *
                                        callback_intervals_aggregate.size() /
                                        100];
  };

  // With the distribution of callback interval means, check that 90% of the
  // average callback intervals are between 9 and 15 milliseconds. This ensures
  // that the vast majority of the time, the callback is invoked as close to
  // 10 milliseconds as possible. The rest of the time is assumed OS induced
  // leeway.
  EXPECT_THAT(percentile(5), Gt(absl::Milliseconds(9)));
  EXPECT_THAT(percentile(95), Lt(absl::Milliseconds(15)));
  // Ensure that the average callback interval is close to 10 milliseconds. This
  // ensures that the distribution is not bimodal around the percentiles.
  // Otherwise, the mean would be closer to 12ms. This isn't meant to be exact,
  // but a "good enough" indicator.
  EXPECT_THAT(
      absl::c_accumulate(callback_intervals_aggregate, absl::ZeroDuration()) /
          callback_intervals_aggregate.size(),
      AllOf(Gt(absl::Milliseconds(9)), Lt(absl::Milliseconds(11))))
      << testing::PrintToString(callback_intervals_aggregate);
}

}  // namespace
}  // namespace meet
