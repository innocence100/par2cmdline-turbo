//  This file is part of par2cmdline-turbo (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//  Copyright (c) 2019 Michael D. Nahas
//
//  par2cmdline is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  par2cmdline is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __APPEND7Z_H__
#define __APPEND7Z_H__

#include <ostream>
#include <string>
#include <vector>

#include "libpar2.h"

Result par2create_append(std::ostream &sout,
                         std::ostream &serr,
                         const NoiseLevel noiselevel,
                         const size_t memorylimit,
                         const std::string &basepath,
                         const u32 nthreads,
                         const u32 filethreads,
                         const std::string &parfilename,
                         const std::vector<std::string> &extrafiles,
                         const u64 blocksize,
                         const u32 firstblock,
                         const Scheme recoveryfilescheme,
                         const u32 recoveryfilecount,
                         const u32 recoveryblockcount);

Result par2repair_appended(std::ostream &sout,
                           std::ostream &serr,
                           const NoiseLevel noiselevel,
                           const size_t memorylimit,
                           const std::string &basepath,
                           const u32 nthreads,
                           const u32 filethreads,
                           const std::string &parfilename,
                           const bool dorepair,
                           const bool purgefiles,
                           const bool renameonly,
                           const bool skipdata,
                           const u64 skipleaway);

#endif // __APPEND7Z_H__
