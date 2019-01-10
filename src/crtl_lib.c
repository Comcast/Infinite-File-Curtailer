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
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/stat.h>
#include "curtail.h"
#include "crtl_private.h"

#define CRTL_SIGNAL_QTY (7)

typedef enum {
   CRTL_EVENT_TERMINATE = 0,
   CRTL_EVENT_SIG_QUIT  = 1,
   CRTL_EVENT_SIG_TERM  = 2,
   CRTL_EVENT_SIG_INT   = 3,
   CRTL_EVENT_INVALID   = 4
} crtl_event_type_t;

typedef struct {
   crtl_event_type_t type;
   sem_t *           semaphore;
} crtl_event_t;

typedef struct {
   bool   waiting;
   sem_t *semaphore;
   int    fd_input;
   int    fd_event;
} crtl_thread_params_t;

typedef struct {
   int              signum;
   bool             installed;
   struct sigaction act;
} crtl_signals_t;

typedef struct {
   crtl_log_level_t level;
   bool             initialized;
   bool             interactive;
   crtl_signals_t   signals[CRTL_SIGNAL_QTY];
   uint64_t         out_file_size_max;
   uint64_t         out_file_size_cur;
   pthread_t        main_thread;
   sem_t            semaphore;
   int              fd_output;
   int              fd_input_rd;
   int              fd_input_wr;
   int              fd_event;
   int              fd_stdout;
   int              fd_stderr;
   uint32_t         logical_block_size;
   char             buffer[4096];
} crtl_global_t;

static crtl_global_t g_crtl = { .level              = CRTL_LEVEL_ERROR,
                                .initialized        = false,
                                .interactive        = false,
                                .fd_output          = -1,
                                .fd_event           = -1,
                                .fd_stdout          = -1,
                                .fd_stderr          = -1,
                                .logical_block_size = DEFAULT_SECTOR_SIZE,
                                .out_file_size_max  = LOGR_LOG_SIZE_MAX_DEFAULT,
                                .out_file_size_cur  = 0
                              };

static bool  crtl_signals_register(void);
static void  crtl_signals_unregister(void);
static void  crtl_signal_handler(int signal);
static void  crtl_abort(void);
static void *crtl_main_thread(void *param);

#if 0
static void crtl_write_raw(const char *data, uint32_t size);
#endif

bool crtl_log_enabled(crtl_log_level_t level) {
   return(g_crtl.level <= level);
}

bool crtl_init(const char *filename, uint64_t size_max, crtl_log_level_t level, bool include_stderr) {
   if(g_crtl.initialized) {
      LOG_WARN("already initialized");
      errno = 0;
      return(false);
   }
   g_crtl.level = level;

   if(isatty(STDIN_FILENO)) {
      g_crtl.interactive = true;
      g_crtl.initialized = true;
      return(true);
   }
   
   g_crtl.interactive = false;
   
   if(!crtl_signals_register()) {
      LOG_ERROR("unable to register signals");
      crtl_signals_unregister();
      return(false);
   }

   if(!crtl_file_open(filename, &g_crtl.fd_output, &g_crtl.logical_block_size, &g_crtl.out_file_size_cur)) {
      LOG_ERROR("unable to open output file");
      crtl_signals_unregister();
      return(false);
   }

   g_crtl.out_file_size_max = size_max;
   
   if(g_crtl.out_file_size_max % DEFAULT_SECTOR_SIZE) {
      LOG_WARN("file size should be an integer multiple of the block size (%u bytes)", DEFAULT_SECTOR_SIZE);
      g_crtl.out_file_size_max -= g_crtl.out_file_size_max % DEFAULT_SECTOR_SIZE;
   }
   if(g_crtl.out_file_size_max < 2 * DEFAULT_SECTOR_SIZE) {
      LOG_WARN("file size must be greater than 2x block size (%u bytes)", DEFAULT_SECTOR_SIZE);
      g_crtl.out_file_size_max = 2 * DEFAULT_SECTOR_SIZE;
   }
   
   LOG_INFO("output file <%s>", filename);
   LOG_INFO("logical block size %u bytes", g_crtl.logical_block_size);
   LOG_INFO("current file size %" PRIu64 " bytes", g_crtl.out_file_size_cur);
   LOG_INFO("maximum file size %" PRIu64 " bytes", g_crtl.out_file_size_max);

   // Initialize semaphore
   sem_init(&g_crtl.semaphore, 0, 0);

   int pipefd_input[2];
   int pipefd_event[2];
   if(pipe(pipefd_input) == -1) {
      crtl_signals_unregister();
      return(false);
   }
   if(pipe(pipefd_event) == -1) {
      close(pipefd_input[1]);
      crtl_signals_unregister();
      return(false);
   }
   g_crtl.fd_event    = pipefd_event[1];
   g_crtl.fd_input_rd = pipefd_input[0];
   g_crtl.fd_input_wr = pipefd_input[1];

   crtl_thread_params_t params;
   params.semaphore = &g_crtl.semaphore;
   params.fd_input  = pipefd_input[0];
   params.fd_event  = pipefd_event[0];

   // Save old stdout fd
   g_crtl.fd_stdout = dup(STDOUT_FILENO);

   // reroute stdout and stderr to our new pipe
   dup2(pipefd_input[1], STDOUT_FILENO);
   if(include_stderr) {
      // Save old stderr fd
      g_crtl.fd_stderr = dup(STDERR_FILENO);
      dup2(pipefd_input[1], STDERR_FILENO);
   }

   if(0 != pthread_create(&g_crtl.main_thread, NULL, crtl_main_thread, &params)) {
      crtl_signals_unregister();
      return(false);
   }

   // Block until initialization is complete
   sem_wait(&g_crtl.semaphore);

   g_crtl.initialized = true;
   return(true);
}

void *crtl_main_thread(void *param) {
   // Make a copy of input parameters
   crtl_thread_params_t params = *((crtl_thread_params_t *)param);

   if(params.semaphore != NULL) { // Unblock the caller that launched this thread
      sem_post(params.semaphore);
   }

   bool running = true;
   do { // Read from fd's and write to file
      int nfds = params.fd_input;
      if(params.fd_event > nfds) {
         nfds = params.fd_event;
      }
      nfds++;

      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(params.fd_input, &rfds);
      FD_SET(params.fd_event, &rfds);

      int src = select(nfds, &rfds, NULL, NULL, NULL);

      if(src < 0) { // error occurred
         LOG_ERROR("select failed, rc=%d", src);
         break;
      }
      if(FD_ISSET(params.fd_event, &rfds)) {
         crtl_event_t event;
         int rc = crtl_read(params.fd_event, &event, sizeof(event));
         if(rc <= 0 || rc != sizeof(event)) {
            LOG_ERROR("event receive failed");
            running = false;
            break;
         }
         switch(event.type) {
            case CRTL_EVENT_TERMINATE: {
               if(event.semaphore != NULL) {
                  sem_post(event.semaphore);
               }
               running = false;
               break;
            }
            case CRTL_EVENT_SIG_QUIT: {
               // In case of sigquit, need to attempt one last read to flush all data to the file before exiting
               running = false;
               break;
            }
            default: {
               running = false;
            }
         }
      }
      if(FD_ISSET(params.fd_input, &rfds)) {
         int rc = crtl_read(params.fd_input, g_crtl.buffer, sizeof(g_crtl.buffer));
         if(rc <= 0) {
            running = false;
         } else {
            rc = crtl_process_input(g_crtl.fd_output, &g_crtl.out_file_size_cur, g_crtl.out_file_size_max, g_crtl.logical_block_size, g_crtl.buffer, rc);

            if(rc < 0) {
               running = false;
            }
         }
      }
   } while(running);

   // Restore stdout and stderr
   if(g_crtl.fd_stdout >= 0) {
      dup2(g_crtl.fd_stdout, STDOUT_FILENO);
   }
   if(g_crtl.fd_stderr >= 0) {
      dup2(g_crtl.fd_stderr, STDERR_FILENO);
   }

   return(NULL);
}

int crtl_fsync(void) {
   if(!g_crtl.initialized) {
      errno = 0;
      return(-1);
   }
   
   if(g_crtl.interactive) {
      return(fsync(STDOUT_FILENO));
   } else {
      return(fsync(g_crtl.fd_output));
   }
}

void crtl_term(void) {
   if(g_crtl.initialized) {
      // Terminate processing thread
      sem_t        semaphore;
      crtl_event_t event;
      event.type      = CRTL_EVENT_TERMINATE;
      event.semaphore = &semaphore;
      // Initialize semaphore
      sem_init(&semaphore, 0, 0);

      crtl_write(g_crtl.fd_event, &event, sizeof(event));

      // Wait for acknowledgement
      int rc = -1;
      struct timespec end_time;
      if(clock_gettime(CLOCK_REALTIME, &end_time) != 0) {
         LOG_ERROR("unable to get time");
      } else {
         end_time.tv_sec += 5;
         do {
            errno = 0;
            rc = sem_timedwait(&semaphore, &end_time);
            if(rc == -1 && errno == EINTR) {
               LOG_INFO("interrupted");
            } else {
               break;
            }
         } while(1);
      }

      if(rc != 0) { // no response received
         LOG_INFO("Do NOT wait for thread to exit");
      } else {
         // Wait for thread to exit
         LOG_INFO("Waiting for thread to exit");
         void *retval;
         if(0 != pthread_join(g_crtl.main_thread, &retval)) {
            LOG_ERROR("thread join failed.");
         } else {
            LOG_INFO("thread exited.");
         }
      }

      crtl_fsync();
      crtl_file_close(&g_crtl.fd_output);
      if(g_crtl.fd_event >= 0) {
         crtl_close(g_crtl.fd_event);
         g_crtl.fd_event = -1;
      }
      crtl_signals_unregister();
      g_crtl.initialized = false;
   }
}

// Signals - Handle all signals that result in a core dump.  Flush the output pipe, write to the file and raise the default signal handler.
bool crtl_signals_register(void) {
   memset(&g_crtl.signals, 0, sizeof(g_crtl.signals));

   g_crtl.signals[0].signum = SIGQUIT;
   g_crtl.signals[1].signum = SIGILL;
   g_crtl.signals[2].signum = SIGABRT;
   g_crtl.signals[3].signum = SIGFPE;
   g_crtl.signals[4].signum = SIGSEGV;
   g_crtl.signals[5].signum = SIGBUS;
   g_crtl.signals[6].signum = SIGSYS;

   for(uint8_t index = 0; index < CRTL_SIGNAL_QTY; index++) {
      struct sigaction act;
      memset(&act, 0, sizeof(act));

      act.sa_handler = crtl_signal_handler;

      int rc = sigaction(g_crtl.signals[index].signum, &act, &g_crtl.signals[index].act);
      if(rc < 0) {
         return(false);
      }
      g_crtl.signals[index].installed = true;
   }
   return(true);
}

void crtl_signals_unregister(void) {
   for(uint8_t index = 0; index < CRTL_SIGNAL_QTY; index++) {
      if(g_crtl.signals[index].installed) {
         sigaction(g_crtl.signals[index].signum, &g_crtl.signals[index].act, NULL);
         g_crtl.signals[index].installed = false;
      }
   }
}

void crtl_signal_handler(int signal) {
   switch(signal) {
      case SIGQUIT: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[0].act, NULL);
         raise(signal);
         break;
      }
      case SIGILL: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[1].act, NULL);
         raise(signal);
         break;
      }
      case SIGABRT: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[2].act, NULL);
         raise(signal);
         break;
      }
      case SIGFPE: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[3].act, NULL);
         raise(signal);
         break;
      }
      case SIGSEGV: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[4].act, NULL);
         raise(signal);
         break;
      }
      case SIGBUS: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[5].act, NULL);
         raise(signal);
         break;
      }
      case SIGSYS: {
         crtl_abort();
         sigaction(signal, &g_crtl.signals[6].act, NULL);
         raise(signal);
         break;
      }
      default: {
         break;
      }
   }
}

#if 0
void crtl_write_raw(const char *data, uint32_t size) {
   crtl_process_input(g_crtl.fd_output, &g_crtl.out_file_size_cur, g_crtl.out_file_size_max, g_crtl.logical_block_size, data, size);
   crtl_fsync();
}
#endif

// This function allows the program to flush data to disk when the program crashes
// IMPORTANT: must be signal safe
void crtl_abort(void) {
   if(!g_crtl.interactive) {
      if(0 == ftrylockfile(stdout)) { // lock obtained. proceed to flush stdout
         fflush_unlocked(stdout);
         funlockfile(stdout);
      }

      // Set input to non-blocking
      int flags = fcntl(g_crtl.fd_input_rd, F_GETFL, 0);
      fcntl(g_crtl.fd_input_rd, F_SETFL, flags | O_NONBLOCK);

      // Attempt one more read from input fd in case there is unprocessed data
      int rc = crtl_read(g_crtl.fd_input_rd, g_crtl.buffer, sizeof(g_crtl.buffer));
      if(rc > 0) { // Process the data
         crtl_process_input(g_crtl.fd_output, &g_crtl.out_file_size_cur, g_crtl.out_file_size_max, g_crtl.logical_block_size, g_crtl.buffer, rc);
      }
   }

   // Flush the data to the file
   crtl_fsync();
}
