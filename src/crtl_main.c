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
#include <signal.h>
#include <linux/limits.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <argp.h>
#include "curtail.h"
#include "crtl_private.h"

// TODO Need to clean up
// TODO document the program
// TODO Check kernel revision and fs type to ensure fallocate is supported
// TODO Add a output clear api
// TODO Allow stdout buffering to be changed

#define LOGR_VERSION "1.0"

static bool    crtl_cmdline_args(int argc, char *argv[]);
static error_t crtl_parse_opt(int key, char *arg, struct argp_state *state);
static bool    crtl_main_init(void);
static void    crtl_main(void);
static void    crtl_main_term(void);
static void    crtl_signals_register(void);
static void    crtl_signal_handler(int signal);

typedef struct {
   crtl_log_level_t level;
   bool             sig_quit;
   char *           out_file_path;
   uint64_t         out_file_size_max;
   uint64_t         out_file_size_cur;
   int              fd_output;
   uint32_t         logical_block_size;
   char             buffer[4096];
} crtl_global_t;

const char *argp_program_version     =  "curtail " LOGR_VERSION;
const char *argp_program_bug_address = "<david_wolaver@cable.comcast.com>";

static char doc[] = "curtail -- a program that reads stdin and writes to a fixed size file";

static char args_doc[] = "<output file>";

static struct argp_option options[] = {
  {"verbose",  'v', 0,      0,  "Produce verbose output" },
  {"quiet",    'q', 0,      0,  "Don't produce any output" },
  {"size",     's', "size", 0,  "Maximum size of the output file (ie. 8192, 64K, 10M, 1G, etc)" },
  { 0 }
};

static struct argp argp = { options, crtl_parse_opt, args_doc, doc };

static crtl_global_t g_crtl = { .level              = CRTL_LEVEL_INFO,
                                .sig_quit           = false,
                                .out_file_path      = "",
                                .fd_output          = -1,
                                .logical_block_size = DEFAULT_SECTOR_SIZE,
                                .out_file_size_max  = LOGR_LOG_SIZE_MAX_DEFAULT,
                                .out_file_size_cur  = 0
                              };

bool crtl_log_enabled(crtl_log_level_t level) {
   return(g_crtl.level <= level);
}

int main(int argc, char *argv[]) {
   // Set stdout to be line buffered
   setvbuf(stdout, NULL, _IOLBF, 0);

   // Parse command line arguments
   if(!crtl_cmdline_args(argc, argv)) {
      return(-1);
   }

   LOG_DEBUG("Starting process ver %s", LOGR_VERSION);

   crtl_signals_register();

   if(crtl_main_init()) {
      crtl_main();
   }
   crtl_main_term();
   LOG_DEBUG("return");
   return(0);
}

error_t crtl_parse_opt(int key, char *arg, struct argp_state *state) {
   // Get the input argument from argp_parse, which we know is a pointer to our arguments structure.
   crtl_global_t *arguments = state->input;

   switch(key) {
      case 'q': {
         arguments->level = CRTL_LEVEL_ERROR;
         break;
      }
      case 'v': {
         arguments->level = CRTL_LEVEL_DEBUG;
         break;
      }
      case 's': {
         LOG_DEBUG("size arg %s", arg);
         size_t length = strlen(arg);
         char last_char = arg[length - 1];
         uint64_t multiplier = 0;
         if(last_char == 'k' || last_char == 'K') {
            multiplier = 1024;
            arg[length - 1] = '\0';
         } else if(last_char == 'm' || last_char == 'M') {
            multiplier = 1024 * 1024;
            arg[length - 1] = '\0';
         } else if(last_char == 'g' || last_char == 'G') {
            multiplier = 1024 * 1024 * 1024;
            arg[length - 1] = '\0';
         }
         
         int size = atoi(arg);
         if(size > 0) {
            arguments->out_file_size_max = size * multiplier;
         }
         break;
      }
      case ARGP_KEY_ARG: {
         if(state->arg_num >= 1) { // Too many arguments.
           argp_usage(state);
         }
         arguments->out_file_path = arg;
         break;
      }
      case ARGP_KEY_END: {
         if (state->arg_num < 1) { // Not enough arguments.
           argp_usage (state);
         }
         break;
      }
      default: {
         return ARGP_ERR_UNKNOWN;
      }
   }
   return 0;
}

bool crtl_cmdline_args(int argc, char *argv[]) {
   argp_parse(&argp, argc, argv, 0, 0, &g_crtl);
   
   LOG_INFO("log level <%s>",  crtl_log_level_str(g_crtl.level));
   LOG_INFO("output file size <%llu>", g_crtl.out_file_size_max);
   LOG_INFO("output file path <%s>",   g_crtl.out_file_path);
   
   if(g_crtl.out_file_size_max % DEFAULT_SECTOR_SIZE) {
      LOG_WARN("file size should be an integer multiple of the block size (%u bytes)", DEFAULT_SECTOR_SIZE);
      g_crtl.out_file_size_max -= g_crtl.out_file_size_max % DEFAULT_SECTOR_SIZE;
   }
   if(g_crtl.out_file_size_max < 2 * DEFAULT_SECTOR_SIZE) {
      LOG_WARN("file size must be greater than 2x block size (%u bytes)", DEFAULT_SECTOR_SIZE);
      g_crtl.out_file_size_max = 2 * DEFAULT_SECTOR_SIZE;
   }

   return(true);
}

bool crtl_main_init(void) {
   if(isatty(STDIN_FILENO)) {
      LOG_ERROR("cannot run from a terminal.");
      return(false);
   }
   
   if(!crtl_file_open(g_crtl.out_file_path, &g_crtl.fd_output, &g_crtl.logical_block_size, &g_crtl.out_file_size_cur)) {
      LOG_ERROR("unable to open output file");
      return(false);
   }
   
   LOG_INFO("logical block size %u bytes", g_crtl.logical_block_size);
   LOG_INFO("current file size %llu bytes", g_crtl.out_file_size_cur);

   return(true);
}

void crtl_main_term(void) {
   LOG_DEBUG("fd %d", g_crtl.fd_output);
   crtl_file_close(&g_crtl.fd_output);
}

void crtl_main(void) {
   bool running = true;
   do { // Read stdin and write to file
      if(g_crtl.sig_quit) { // In case of sigquit, need to attempt one last read to flush all data to the file before exiting
         running = false;
      }
      int rc = crtl_read(STDIN_FILENO, g_crtl.buffer, sizeof(g_crtl.buffer));
      if(rc > 0) {
         if(0 > crtl_process_input(g_crtl.fd_output, &g_crtl.out_file_size_cur, g_crtl.out_file_size_max, g_crtl.logical_block_size, g_crtl.buffer, rc)) {
            LOG_ERROR("%s: error processing stdin\n", __FUNCTION__);
            running = false;
         }
      } else if(rc < 0) {
         int errsv = errno;
         LOG_ERROR("%s: error reading from stdin <%s>\n", __FUNCTION__, strerror(errsv));
         running = false;
      } else {
         running = false;
      }
   } while(running);
}

void crtl_signals_register(void) {
   struct sigaction action;
   action.sa_handler = crtl_signal_handler;
   action.sa_flags   = SA_RESTART;
   sigemptyset(&action.sa_mask);
   
   // Handle these signals
   LOG_DEBUG("Registering SIGINT...");
   if(sigaction(SIGINT, &action, NULL) != 0) {
      int errsv = errno;
      LOG_ERROR("Unable to register for SIGINT. <%s>", strerror(errsv));
   }
   LOG_DEBUG("Registering SIGTERM...");
   if(sigaction(SIGTERM, &action, NULL) != 0) {
      int errsv = errno;
      LOG_ERROR("Unable to register for SIGTERM. <%s>", strerror(errsv));
   }
   LOG_DEBUG("Registering SIGQUIT...");
   if(sigaction(SIGQUIT, &action, NULL) != 0) {
      int errsv = errno;
      LOG_ERROR("Unable to register for SIGQUIT. <%s>", strerror(errsv));
   }
}

void crtl_signal_handler(int signal) {
   switch(signal) {
      case SIGTERM:
      case SIGINT: {
         LOG_DEBUG("Received %s", signal == SIGINT ? "SIGINT" : "SIGTERM");
         // Ignore SIGINT and SIGTERM
         break;
      }
      case SIGQUIT: {
         LOG_DEBUG("Received SIGQUIT");
         g_crtl.sig_quit = true;
         break;
      }
      default:
         LOG_DEBUG("Received unhandled signal %d", signal);
         break;
   }
}
