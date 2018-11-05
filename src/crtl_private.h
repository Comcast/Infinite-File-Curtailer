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

#ifndef __CRTL_PRIVATE_H__
#define __CRTL_PRIVATE_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifndef DEFAULT_SECTOR_SIZE
#define DEFAULT_SECTOR_SIZE (4096)
#endif

#define LOGR_LOG_SIZE_MAX_DEFAULT (4 * DEFAULT_SECTOR_SIZE)

#ifdef __cplusplus
extern "C"
{
#endif

bool        crtl_log_enabled(crtl_log_level_t level);
const char *crtl_log_level_str(crtl_log_level_t level);

#define LOG_DEBUG(FORMAT, ...); do {if(crtl_log_enabled(CRTL_LEVEL_DEBUG)) { fprintf(stderr, "%s: " FORMAT "\n", __FUNCTION__, ##__VA_ARGS__);}} while(0)
#define LOG_INFO(FORMAT, ...);  do {if(crtl_log_enabled(CRTL_LEVEL_INFO))  { fprintf(stderr, "%s: " FORMAT "\n", __FUNCTION__, ##__VA_ARGS__);}} while(0)
#define LOG_WARN(FORMAT, ...);  do {if(crtl_log_enabled(CRTL_LEVEL_WARN))  { fprintf(stderr, "%s: WARN  :" FORMAT "\n", __FUNCTION__, ##__VA_ARGS__);}} while(0)
#define LOG_ERROR(FORMAT, ...); do {if(crtl_log_enabled(CRTL_LEVEL_ERROR)) { fprintf(stderr, "%s: ERROR :" FORMAT "\n", __FUNCTION__, ##__VA_ARGS__);}} while(0)

int crtl_open(const char *pathname, int flags, mode_t mode);
int crtl_close(int fd);
int crtl_fstat(int fd, struct stat *buf);
int crtl_read(int fd, void *buf, size_t count);
int crtl_fallocate(int fd, int mode, off_t offset, off_t len);
int crtl_seek(int fd, off_t offset, int whence);
int crtl_write(int fd, const void *buf, size_t count);

bool crtl_file_open(const char *filename, int *fd, uint32_t *block_size, uint64_t *file_size);
void crtl_file_close(int *fd);
int  crtl_process_input(int fd, uint64_t *file_size_cur, uint64_t file_size_max, uint32_t logical_block_size, const char *buffer, uint32_t data_size);

#ifdef __cplusplus
}
#endif

#endif
