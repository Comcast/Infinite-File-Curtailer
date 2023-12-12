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

AC_INIT([curtail], [1.0], [David_Wolaver@cable.comcast.com])
AM_INIT_AUTOMAKE([foreign])
AM_PROG_AR
LT_INIT

AC_PROG_CC

CFLAGS+=" -std=c11 -fPIC -D_REENTRANT -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Werror -rdynamic"

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_OUTPUT