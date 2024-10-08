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

#ifndef NATIVE_INTERNAL_TESTING_MOCK_MEET_MEDIA_API_SESSION_OBSERVER_H_
#define NATIVE_INTERNAL_TESTING_MOCK_MEET_MEDIA_API_SESSION_OBSERVER_H_

#include "gmock/gmock.h"
#include "native/api/meet_media_api_client_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/rtc_base/ref_counted_object.h"

namespace meet {

class MockMeetMediaApiSessionObserver
    : public rtc::RefCountedObject<MeetMediaApiSessionObserverInterface> {
 public:
  static rtc::scoped_refptr<MockMeetMediaApiSessionObserver> Create() {
    return rtc::scoped_refptr<MockMeetMediaApiSessionObserver>(
        new MockMeetMediaApiSessionObserver());
  }

  MOCK_METHOD(void, OnResourceUpdate, (ResourceUpdate), (override));
  MOCK_METHOD(void, OnResourceRequestFailure, (ResourceRequestError),
              (override));
  MOCK_METHOD(void, OnClientStateUpdate, (MeetMediaApiClientState), (override));
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_TESTING_MOCK_MEET_MEDIA_API_SESSION_OBSERVER_H_
