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

#include "append7z.h"

#include "diskfile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <list>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

static const u8 SIG_7Z[] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
static const size_t SIG_7Z_LEN = 6;
static const u8 PAR2_MAGIC[] = {'P', 'A', 'R', '2', '\0', 'P', 'K', 'T'};
static const size_t PAR2_MAGIC_LEN = 8;
static const size_t COPY_BUFFER_SIZE = 1 << 16;
static const size_t SEARCH_CHUNK_SIZE = 1 << 20;

std::string JoinPath(const std::string &dir, const std::string &name)
{
  if (dir.empty())
    return name;

  char last = dir[dir.length() - 1];
  if (last == '/' || last == '\\')
    return dir + name;

  return dir + PATHSEP + name;
}

std::string Remove7zExtension(const std::string &filename)
{
  if (filename.length() >= 3 && 0 == stricmp(filename.substr(filename.length() - 3).c_str(), ".7z"))
    return filename.substr(0, filename.length() - 3);

  return filename;
}

u64 Get7zArchiveSize(const std::string &filepath)
{
  std::ifstream f(filepath.c_str(), std::ios::binary);
  if (!f)
    return 0;

  u8 header[32];
  f.read((char *)header, sizeof(header));
  if ((size_t)f.gcount() != sizeof(header))
    return 0;

  if (memcmp(header, SIG_7Z, SIG_7Z_LEN) != 0)
    return 0;

  u64 nextHeaderOffset = 0;
  u64 nextHeaderSize = 0;
  memcpy(&nextHeaderOffset, header + 12, sizeof(nextHeaderOffset));
  memcpy(&nextHeaderSize, header + 20, sizeof(nextHeaderSize));

  return 32 + nextHeaderOffset + nextHeaderSize;
}

bool Validate7zFile(const std::string &filepath)
{
  u64 expectedSize = Get7zArchiveSize(filepath);
  if (expectedSize == 0)
    return false;

  u64 actualSize = DiskFile::GetFileSize(filepath);
  return actualSize >= expectedSize;
}

u64 FindAppendedPar2Offset(const std::string &filepath, u64 &archiveSize)
{
  archiveSize = Get7zArchiveSize(filepath);
  if (archiveSize == 0)
    return 0;

  u64 filesize = DiskFile::GetFileSize(filepath);
  if (filesize <= archiveSize)
    return 0;

  std::ifstream f(filepath.c_str(), std::ios::binary);
  if (!f)
    return 0;

  std::vector<u8> buffer(SEARCH_CHUNK_SIZE + PAR2_MAGIC_LEN);
  for (u64 offset = archiveSize; offset < filesize; offset += SEARCH_CHUNK_SIZE)
  {
    u64 toRead = std::min<u64>(SEARCH_CHUNK_SIZE + PAR2_MAGIC_LEN, filesize - offset);
    f.seekg((std::streamoff)offset, std::ios::beg);
    f.read((char *)&buffer[0], (std::streamsize)toRead);
    size_t bytesRead = (size_t)f.gcount();
    if (bytesRead < PAR2_MAGIC_LEN)
      continue;

    for (size_t i = 0; i + PAR2_MAGIC_LEN <= bytesRead; ++i)
    {
      if (memcmp(&buffer[i], PAR2_MAGIC, PAR2_MAGIC_LEN) == 0)
        return offset + i;
    }
  }

  return 0;
}

bool CopyFileSlice(const std::string &source,
                   const std::string &target,
                   u64 start,
                   u64 length,
                   bool append = false)
{
  std::ifstream in(source.c_str(), std::ios::binary);
  if (!in)
    return false;

  std::ios::openmode mode = std::ios::binary | std::ios::out;
  mode |= append ? std::ios::app : std::ios::trunc;
  std::ofstream out(target.c_str(), mode);
  if (!out)
    return false;

  in.seekg((std::streamoff)start, std::ios::beg);
  std::vector<char> buffer(COPY_BUFFER_SIZE);
  while (length > 0)
  {
    size_t chunk = (size_t)std::min<u64>(length, buffer.size());
    in.read(&buffer[0], (std::streamsize)chunk);
    size_t got = (size_t)in.gcount();
    if (got == 0)
      return false;

    out.write(&buffer[0], (std::streamsize)got);
    if (!out)
      return false;

    length -= got;
  }

  return true;
}

bool CopyWholeFile(const std::string &source, const std::string &target)
{
  return CopyFileSlice(source, target, 0, DiskFile::GetFileSize(source));
}

bool RemoveFileIfExists(const std::string &path)
{
  if (!DiskFile::FileExists(path))
    return true;

  return std::remove(path.c_str()) == 0;
}

bool ReplaceFile(const std::string &source, const std::string &target)
{
#ifdef _WIN32
  if (MoveFileExA(source.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == 0)
    return false;
  return true;
#else
  return std::rename(source.c_str(), target.c_str()) == 0;
#endif
}

bool RemoveDirectoryOne(const std::string &path)
{
#ifdef _WIN32
  return RemoveDirectoryA(path.c_str()) != 0;
#else
  return rmdir(path.c_str()) == 0;
#endif
}

class TempWorkspace {
public:
  explicit TempWorkspace(const std::string &dir) : path(dir) {}
  ~TempWorkspace()
  {
    if (path.empty())
      return;

    std::unique_ptr<std::list<std::string> > files = DiskFile::FindFiles(path, "*", false);
    for (std::list<std::string>::const_iterator it = files->begin(); it != files->end(); ++it)
      RemoveFileIfExists(*it);

    RemoveDirectoryOne(path);
  }

  std::string path;
};

std::string CreateTempDirectory(const std::string &parent)
{
#ifdef _WIN32
  char tempPath[MAX_PATH + 1];
  strncpy(tempPath, parent.c_str(), MAX_PATH);
  tempPath[MAX_PATH] = 0;
  if (GetTempFileNameA(tempPath, "p2t", 0, tempPath) == 0)
    return "";
  DeleteFileA(tempPath);
  if (!CreateDirectoryA(tempPath, NULL))
    return "";
  return tempPath;
#else
  std::string pattern = JoinPath(parent, ".par2tmp-XXXXXX");
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back('\0');
  char *created = mkdtemp(&writable[0]);
  return created ? std::string(created) : std::string();
#endif
}

std::vector<std::string> CollectPar2Files(const std::string &tempDir, const std::string &parBaseName)
{
  std::vector<std::string> files;
  std::unique_ptr<std::list<std::string> > found = DiskFile::FindFiles(tempDir, parBaseName + "*.par2", false);
  for (std::list<std::string>::const_iterator it = found->begin(); it != found->end(); ++it)
    files.push_back(*it);

  std::sort(files.begin(), files.end());
  return files;
}

Result AppendPar2Files(std::ostream &serr,
                       const std::string &archivePath,
                       const std::vector<std::string> &par2Files,
                       u64 &bytesWritten)
{
  bytesWritten = 0;
  for (size_t i = 0; i < par2Files.size(); ++i)
  {
    u64 size = DiskFile::GetFileSize(par2Files[i]);
    if (!CopyFileSlice(par2Files[i], archivePath, 0, size, true))
    {
      serr << "Failed to append PAR2 data from " << par2Files[i] << std::endl;
      return eFileIOError;
    }
    bytesWritten += size;
  }

  return eSuccess;
}

u64 ChooseAppendBlockSize(u64 filesize, u64 blocksize)
{
  if (filesize < 100 * 1024)
    return (filesize + 3) & ~3;

  if (blocksize == 0)
    return ((filesize / 20) + 3) & ~3;

  u32 estimatedBlocks = (u32)((filesize + blocksize - 1) / blocksize);
  if (estimatedBlocks > 100)
    return ((filesize / 20) + 3) & ~3;

  return blocksize;
}

u32 ChooseAppendRecoveryBlocks(u32 sourceblockcount)
{
  u32 recoveryblockcount = (sourceblockcount * 5 + 50) / 100;
  return std::max<u32>(1, recoveryblockcount);
}

} // namespace

Result par2create_append(std::ostream &sout,
                         std::ostream &serr,
                         const NoiseLevel noiselevel,
                         const size_t memorylimit,
                         const std::string & /*basepath*/,
                         const u32 nthreads,
                         const u32 filethreads,
                         const std::string & /*parfilename*/,
                         const std::vector<std::string> &extrafiles,
                         const u64 blocksize,
                         const u32 firstblock,
                         const Scheme recoveryfilescheme,
                         const u32 recoveryfilecount,
                         const u32 recoveryblockcount)
{
  if (extrafiles.size() != 1)
  {
    serr << "--append requires exactly one input file" << std::endl;
    return eInvalidCommandLineArguments;
  }

  const std::string &source7z = extrafiles[0];
  if (!Validate7zFile(source7z))
  {
    serr << "Invalid 7z archive: " << source7z << std::endl;
    return eFileIOError;
  }

  std::string sourceDir;
  std::string sourceName;
  DiskFile::SplitFilename(source7z, sourceDir, sourceName);

  std::string tempDir = CreateTempDirectory(sourceDir.empty() ? "." : sourceDir);
  if (tempDir.empty())
  {
    serr << "Failed to create temporary directory" << std::endl;
    return eFileIOError;
  }
  TempWorkspace workspace(tempDir);

  std::string tempArchive = JoinPath(workspace.path, sourceName);
  if (!CopyWholeFile(source7z, tempArchive))
  {
    serr << "Failed to create temporary archive copy" << std::endl;
    return eFileIOError;
  }

  u64 filesize = DiskFile::GetFileSize(source7z);
  u64 adjustedBlockSize = ChooseAppendBlockSize(filesize, blocksize);
  u32 sourceblockcount = (u32)((filesize + adjustedBlockSize - 1) / adjustedBlockSize);
  u32 adjustedRecoveryBlockCount = ChooseAppendRecoveryBlocks(sourceblockcount);

  if (noiselevel > nlSilent)
  {
    sout << "Source file: " << source7z << std::endl;
    sout << "File size: " << filesize << " bytes" << std::endl;
    sout << "Block size: " << adjustedBlockSize;
    if (adjustedBlockSize != blocksize && blocksize != 0)
      sout << " (adjusted)";
    sout << std::endl;
    sout << "Source blocks: " << sourceblockcount << std::endl;
    sout << "Recovery blocks: " << adjustedRecoveryBlockCount << std::endl;
  }

  std::string tempParBaseName = Remove7zExtension(sourceName) + "_append";
  std::string tempParBase = JoinPath(workspace.path, tempParBaseName);

  Result result = par2create(sout,
                             serr,
                             noiselevel,
                             memorylimit,
                             sourceDir,
                             nthreads,
                             filethreads,
                             tempParBase,
                             extrafiles,
                             adjustedBlockSize,
                             firstblock,
                             recoveryfilescheme,
                             recoveryfilecount,
                             adjustedRecoveryBlockCount);
  if (result != eSuccess)
    return result;

  std::vector<std::string> par2Files = CollectPar2Files(workspace.path, tempParBaseName);
  if (par2Files.empty())
  {
    serr << "No PAR2 files were created" << std::endl;
    return eFileIOError;
  }

  u64 bytesAppended = 0;
  result = AppendPar2Files(serr, tempArchive, par2Files, bytesAppended);
  if (result != eSuccess)
    return result;

  if (!ReplaceFile(tempArchive, source7z))
  {
    serr << "Failed to replace archive with appended result" << std::endl;
    return eFileIOError;
  }

  if (noiselevel > nlSilent)
  {
    sout << "Successfully appended " << bytesAppended << " bytes of PAR2 data to 7z archive" << std::endl;
    sout << "Total archive size now: " << (filesize + bytesAppended) << " bytes" << std::endl;
  }

  return eSuccess;
}

Result par2repair_appended(std::ostream &sout,
                           std::ostream &serr,
                           const NoiseLevel noiselevel,
                           const size_t memorylimit,
                           const std::string & /*basepath*/,
                           const u32 nthreads,
                           const u32 filethreads,
                           const std::string &parfilename,
                           const bool dorepair,
                           const bool purgefiles,
                           const bool renameonly,
                           const bool skipdata,
                           const u64 skipleaway)
{
  u64 archiveSize = 0;
  u64 par2Offset = FindAppendedPar2Offset(parfilename, archiveSize);
  if (par2Offset == 0)
  {
    serr << "No appended PAR2 data found in file: " << parfilename << std::endl;
    return eInsufficientCriticalData;
  }

  std::string sourceDir;
  std::string sourceName;
  DiskFile::SplitFilename(parfilename, sourceDir, sourceName);

  std::string tempDir = CreateTempDirectory(sourceDir.empty() ? "." : sourceDir);
  if (tempDir.empty())
  {
    serr << "Failed to create temporary directory" << std::endl;
    return eFileIOError;
  }
  TempWorkspace workspace(tempDir);

  std::string tempArchive = JoinPath(workspace.path, sourceName);
  std::string tempParBaseName = Remove7zExtension(sourceName) + "_appended";
  std::string tempParBase = JoinPath(workspace.path, tempParBaseName);
  std::string tempParFile = tempParBase + ".par2";

  if (!CopyFileSlice(parfilename, tempArchive, 0, archiveSize))
  {
    serr << "Failed to extract 7z data from appended archive" << std::endl;
    return eFileIOError;
  }

  u64 appendedSize = DiskFile::GetFileSize(parfilename) - par2Offset;
  if (!CopyFileSlice(parfilename, tempParFile, par2Offset, appendedSize))
  {
    serr << "Failed to extract appended PAR2 data" << std::endl;
    return eFileIOError;
  }

  std::vector<std::string> extrafiles;
  extrafiles.push_back(tempArchive);

  Result result = par2repair(sout,
                             serr,
                             noiselevel,
                             memorylimit,
                             workspace.path + PATHSEP,
                             nthreads,
                             filethreads,
                             tempParFile,
                             extrafiles,
                             dorepair,
                             purgefiles,
                             renameonly,
                             skipdata,
                             skipleaway);

  if (result == eSuccess && dorepair)
  {
    std::string repairedOutput = parfilename + ".repaired";
    if (!CopyWholeFile(tempArchive, repairedOutput))
    {
      serr << "Failed to write repaired archive: " << repairedOutput << std::endl;
      return eFileIOError;
    }
  }

  return result;
}
