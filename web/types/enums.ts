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
 * @fileoverview Enums for the Media API Web Client. Since other files are
 * using the .d.ts file, we need to keep the enums in this file.
 */

/**
 * Log level for each data channel.
 */
export enum LogLevel {
  UNKNOWN = 0,
  ERRORS = 1,
  RESOURCES = 2,
  MESSAGES = 3,
}

/**
 * Enum for the status of the Meet session.
 */
export enum MeetSessionStatus {
  NEW = 0 /* Default value */,
  WAITING = 1,
  JOINED = 2,
  DISCONNECTED = 3,
  KICKED = 4 /* DISCONNECTED with leave request */,
  REJECTED = 5 /* Error state */,
}
