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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "curtail.h"
#include "crtl_private.h"

// Perform open while ignoring signals
int crtl_open(const char *pathname, int flags, mode_t mode) {
   int rc;
   do {
      errno = 0;
      rc    = open(pathname, flags, mode);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}

// Perform close while ignoring signals
int crtl_close(int fd) {
   int rc;
   do {
      errno = 0;
      rc    = close(fd);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}

// Perform fstat while ignoring signals
int crtl_fstat(int fd, struct stat *buf) {
   int rc;
   do {
      errno = 0;
      rc    = fstat(fd, buf);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}

// Perform read while ignoring signals
int crtl_read(int fd, void *buf, size_t count) {
   int rc;
   do {
      errno = 0;
      rc    = read(fd, buf, count);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}

// Perform write while ignoring signals
int crtl_write(int fd, const void *buf, size_t count) {
   int rc;
   do {
      errno = 0;
      rc    = write(fd, buf, count);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}

// Perform fallocate while ignoring signals
int crtl_fallocate(int fd, int mode, off_t offset, off_t len) {
   int rc;
   do {
      errno = 0;
      rc    = fallocate(fd, mode, offset, len);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}

// Perform seek while ignoring signals
int crtl_seek(int fd, off_t offset, int whence) {
   int rc;
   do {
      errno = 0;
      rc    = lseek(fd, offset, whence);
   } while(rc < 0 && errno == EINTR);
   return(rc);
}