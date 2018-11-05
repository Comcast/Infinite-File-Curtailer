#
# Copyright 2018 Comcast Cable Communications Management, LLC
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 
# SPDX-License-Identifier: Apache-2.0
#

bin_PROGRAMS = curtail
curtail_SOURCES = crtl_main.c crtl_common.c crtl_file_io.c
curtail_CFLAGS  = $(AM_CFLAGS)

include_HEADERS = curtail.h
lib_LTLIBRARIES = libcurtail.la
libcurtail_la_SOURCES = crtl_lib.c crtl_common.c crtl_file_io.c
