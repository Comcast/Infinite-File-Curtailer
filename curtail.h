/**
 * Copyright 2018 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CURTAIL_H__
#define __CURTAIL_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
   CRTL_LEVEL_DEBUG = 0,
   CRTL_LEVEL_INFO  = 1,
   CRTL_LEVEL_WARN  = 2,
   CRTL_LEVEL_ERROR = 3,
   CRTL_LEVEL_NONE  = 4
} crtl_log_level_t;

#ifdef __cplusplus
extern "C"
{
#endif

bool crtl_init(const char *filename, uint64_t size_max, crtl_log_level_t level, bool include_stderr);
int  crtl_flush(void);
void crtl_term(void);

#ifdef __cplusplus
}
#endif

#endif
