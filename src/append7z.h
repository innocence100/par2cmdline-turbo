//  This file is part of par2cmdline-turbo (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//  Copyright (c) 2019 Michael D. Nahas
//  Copyright (c) 2024 Appender feature
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

#include <string>
#include <vector>
#include <ostream>

#include "libpar2.h"

// Validate that a file is a valid 7z archive
// Returns true if the file has correct 7z signature and size
bool Validate7zFile(const std::string &filepath);

// Get the size of the original 7z archive (without any appended data)
// Returns the expected 7z archive size, or 0 on error
u64 Get7zArchiveSize(const std::string &filepath);

// Find the offset where PAR2 data is appended to a file
// Returns: offset where PAR2 data starts, or 0 if not found
// If found, also sets archiveSize to the size of the 7z portion
u64 FindAppendedPar2Offset(const std::string &filepath, u64 &archiveSize);

// Append PAR2 recovery data to a 7z archive
// This creates a self-contained recovery record appended to the 7z file
// Parameters:
//   sout - output stream for messages
//   serr - error stream for messages  
//   source7zPath - path to the source 7z file
//   outputPath - where to write the output (if different from source)
//   parfilename - base name for PAR2 data (without extension)
//   recoveryData - vector containing the PAR2 packet data to append
// Returns: Result code indicating success or type of failure
Result AppendTo7z(
    std::ostream &sout,
    std::ostream &serr,
    const std::string &source7zPath,
    const std::string &outputPath,
    const std::string &parfilename,
    const std::vector< std::vector<u8> > &recoveryData);

// Get the file extension in lowercase
std::string GetFileExtensionLower(const std::string &filepath);

#endif // __APPEND7Z_H__