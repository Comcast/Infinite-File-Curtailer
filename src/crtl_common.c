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

#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include "curtail.h"
#include "crtl_private.h"

const char *crtl_log_level_str(crtl_log_level_t level) {
   switch(level) {
      case CRTL_LEVEL_DEBUG: return("DEBUG");
      case CRTL_LEVEL_INFO:  return("INFO");
      case CRTL_LEVEL_WARN:  return("WARN");
      case CRTL_LEVEL_ERROR: return("ERROR");
      case CRTL_LEVEL_NONE:  return("NONE");
   }
   return("INVALID");
}

bool crtl_file_open(const char *filename, int *fd, uint32_t *block_size, uint64_t *file_size) {
   if(filename == NULL || fd == NULL || block_size == NULL || file_size == NULL) {
      LOG_ERROR("Invalid parameters filename %p fd %p block_size %p file_size %p", filename, fd, block_size, file_size);
      return(false);
   }
   int fd_file = crtl_open(filename, O_RDWR | O_CREAT, 0644);
   
   if(fd_file < 0) {
      int errsv = errno;
      LOG_ERROR("unable to open output file <%s> <%s>", filename, strerror(errsv));
      return(false);
   }
   struct stat statbuf;
   
   int rc = crtl_fstat(fd_file, &statbuf);
   if(rc != 0) {
      int errsv = errno;
      LOG_ERROR("unable to stat output file <%s>", strerror(errsv));
      crtl_file_close(&fd_file);
      return(false);
   }
   
   off_t offset_end = crtl_seek(fd_file, 0, SEEK_END);
   if(offset_end < 0) {
      int errsv = errno;
      LOG_ERROR("unable to seek end of output file <%s>", strerror(errsv));
      crtl_file_close(&fd_file);
      return(false);
   }
   
   *fd         = fd_file;
   *block_size = statbuf.st_blksize;
   *file_size  = offset_end;
   return(true);
}

void crtl_file_close(int *fd) {
   if(fd == NULL || *fd < 0) {
      return;
   }
   crtl_close(*fd);
   *fd = -1;
}

int crtl_process_input(int fd, uint64_t *file_size_cur, uint64_t file_size_max, uint32_t logical_block_size, const char *buffer, uint32_t data_size) {
   if(*file_size_cur + data_size > file_size_max) { // Log file is full or oversized, deallocate blocks
      int numblocks = (*file_size_cur + data_size - file_size_max + (logical_block_size - 1)) / logical_block_size;
      if(0 > crtl_fallocate(fd, FALLOC_FL_COLLAPSE_RANGE, 0, logical_block_size * numblocks)) {
         int errsv = errno;
         LOG_ERROR("error fallocate output file <%s>", strerror(errsv));
         return(-1);
      } else {
         LOG_DEBUG("truncated output file from %" PRIu64 " to %" PRIu64 " bytes(numblocks: %d)",
            *file_size_cur, *file_size_cur - (logical_block_size * numblocks), numblocks);
            *file_size_cur -= (logical_block_size * numblocks);

         // Reset the file pointer to the new end of the file
         off_t offset_end = crtl_seek(fd, 0, SEEK_END);
         if(offset_end < 0) {
            int errsv = errno;
            LOG_ERROR("unable to seek end of output file <%s>", strerror(errsv));
            return(-1);
         }
      }
   }
   // Write to output file
   int rc = crtl_write(fd, buffer, data_size);
   if(rc < 0) {
      int errsv = errno;
      LOG_ERROR("error writing to output file <%s>", strerror(errsv));
   } else {
      *file_size_cur += rc;
   }
   return(rc);
}
