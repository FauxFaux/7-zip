// OutStreamWithCRC.cpp

#include "StdAfx.h"

#include "OutStreamWithCRC.h"

STDMETHODIMP COutStreamWithCRC::Write(const void *data, 
    UINT32 size, UINT32 *processedSize)
{
  HRESULT result;
  UINT32 realProcessedSize;
  if(!_stream)
  {
    realProcessedSize = size;
    result = S_OK;
  }
  else
    result = _stream->Write(data, size, &realProcessedSize);
  _crc.Update(data, realProcessedSize);
  if(processedSize != NULL)
    *processedSize = realProcessedSize;
  return result;
}

STDMETHODIMP COutStreamWithCRC::WritePart(const void *data, 
    UINT32 size, UINT32 *processedSize)
{
  UINT32 realProcessedSize;
  HRESULT result;
  if(!_stream)
  {
    realProcessedSize = size;
    result = S_OK;
  }
  else
    result = _stream->WritePart(data, size, &realProcessedSize);
  _crc.Update(data, realProcessedSize);
  if(processedSize != NULL)
    *processedSize = realProcessedSize;
  return result;
}
