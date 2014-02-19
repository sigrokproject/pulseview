##
## This file is part of the PulseView project.
##
## Copyright (C) 2014 Marcus Comstedt <marcus@mc.pp.se>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

include(CheckCSourceRuns)

function (memaccess_check_unaligned_le _var)
IF (NOT CMAKE_CROSSCOMPILING)
CHECK_C_SOURCE_RUNS("
#include <stdint.h>
int main() {
    int i;
    union { uint64_t u64; uint8_t u8[16]; } d;
    uint64_t v;
    for (i=0; i<16; i++)
	d.u8[i] = i;
    v = *(uint64_t *)(d.u8+1);
    if (v != 0x0807060504030201ULL)
       return 1;
    return 0;
}" ${_var})
ENDIF (NOT CMAKE_CROSSCOMPILING)
IF (CMAKE_CROSSCOMPILING)
  MESSAGE(WARNING "Cross compiling - using portable code for memory access")
ENDIF (CMAKE_CROSSCOMPILING)
endfunction()
