// Archive/cpio/ItemInfo.h

#pragma once

#ifndef __ARCHIVE_CPIO_ITEMINFO_H
#define __ARCHIVE_CPIO_ITEMINFO_H

#include <sys/stat.h>

#include "Common/Types.h"
#include "Common/String.h"
#include "CpioHeader.h"

namespace NArchive {
namespace NCpio {

struct CItem
{
  AString Name;
  UINT32 inode;
  UINT32 Mode;
  UINT32 UID;
  UINT32 GID;
  UINT32 Size;
  time_t ModificationTime;

  // char LinkFlag;
  // AString LinkName; ?????
  char Magic[8];
  UINT32 NumLinks;
  UINT32 DevMajor;
  UINT32 DevMinor;
  UINT32 RDevMajor;
  UINT32 RDevMinor;
  UINT32 ChkSum;

  bool OldHeader;

  bool IsDirectory() const 
    { return (Mode & _S_IFMT) == _S_IFDIR; }
};

class CItemEx: public CItem
{
public:
  UINT64 HeaderPosition;
  UINT32 HeaderSize;
  UINT64 GetDataPosition() const { return HeaderPosition + HeaderSize; };
};

}}

#endif
