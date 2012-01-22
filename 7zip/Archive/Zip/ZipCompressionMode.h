// CompressionMode.h

#pragma once

#ifndef __ZIP_COMPRESSIONMETHOD_H
#define __ZIP_COMPRESSIONMETHOD_H

#include "Common/Vector.h"
#include "Common/String.h"

namespace NArchive {
namespace NZip {

struct CCompressionMethodMode
{
  CRecordVector<BYTE> MethodSequence;
  // bool MaximizeRatio;
  UINT32 NumPasses;
  UINT32 NumFastBytes;
  bool PasswordIsDefined;
  AString Password;
};

}}

#endif
