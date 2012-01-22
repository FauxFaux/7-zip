// M68.cpp

#include "StdAfx.h"
#include "M68.h"

#include "Windows/Defs.h"

static HRESULT BC_M68_B_Code(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, const UINT64 *inSize, const UINT64 *outSize,
      ICompressProgressInfo *progress, BYTE *buffer, bool encoding)
{
  UINT32 nowPos = 0;
  UINT64 nowPos64 = 0;
  UINT32 bufferPos = 0;
  while(true)
  {
    UINT32 processedSize;
    UINT32 size = kBufferSize - bufferPos;
    RINOK(inStream->Read(buffer + bufferPos, size, &processedSize));
    UINT32 endPos = bufferPos + processedSize;
    if (endPos < 4)
    {
      if (endPos > 0)
      {
        RINOK(outStream->Write(buffer, endPos, &processedSize));
        if (endPos != processedSize)
          return E_FAIL;
      }
      return S_OK;
    }
    for (bufferPos = 0; bufferPos <= endPos - 4;)
    {
      if (buffer[bufferPos] == 0x61 && buffer[bufferPos + 1] == 0x00)
      {
        UINT32 src = 
            (buffer[bufferPos + 2] << 8) |
            (buffer[bufferPos + 3]);

        UINT32 dest;
        if (encoding)
          dest = nowPos + bufferPos + 2 + src;
        else
          dest = src - (nowPos + bufferPos + 2);
        buffer[bufferPos + 2] = (dest >> 8);
        buffer[bufferPos + 3] = dest;
        bufferPos += 4;
      }
      else
        bufferPos += 2;
    }
    nowPos += bufferPos;
    nowPos64 += bufferPos;
    RINOK(outStream->Write(buffer, bufferPos, &processedSize));
    if (bufferPos != processedSize)
      return E_FAIL;
    if (progress != NULL)
    {
      RINOK(progress->SetRatioInfo(&nowPos64, &nowPos64));
    }
    
    UINT32 i = 0;
    while(bufferPos < endPos)
      buffer[i++] = buffer[bufferPos++];
    bufferPos = i;
  }
}

MyClassImp(BC_M68_B)

