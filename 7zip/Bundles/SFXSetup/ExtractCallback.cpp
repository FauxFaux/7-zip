// ExtractCallback.h

#include "StdAfx.h"

#include "ExtractCallback.h"

#include "Common/Wildcard.h"
#include "Common/StringConvert.h"

#include "Windows/COM.h"
#include "Windows/FileDir.h"
#include "Windows/FileFind.h"
#include "Windows/Time.h"
#include "Windows/Defs.h"
#include "Windows/PropVariant.h"

#include "Windows/PropVariantConversions.h"

using namespace NWindows;
using namespace NFile;

static LPCWSTR kErrorTitle = L"7-Zip";
static LPCWSTR kCantDeleteFile = L"Can not delete output file";
static LPCWSTR kCantOpenFile = L"Can not open output file";
static LPCWSTR kUnsupportedMethod = L"Unsupported Method";
static LPCWSTR kCRCFailed = L"CRC Failed";
static LPCWSTR kDataError = L"Data Error";
// static LPCTSTR kUnknownError = TEXT("Unknown Error");

void CExtractCallbackImp::Init(IInArchive *archiveHandler,
    const UString &directoryPath,   
    const UString &itemDefaultName,
    const FILETIME &utcLastWriteTimeDefault,
    UINT32 attributesDefault)
{
  _numErrors = 0;
  _itemDefaultName = itemDefaultName;
  _utcLastWriteTimeDefault = utcLastWriteTimeDefault;
  _attributesDefault = attributesDefault;
  _archiveHandler = archiveHandler;
  _directoryPath = directoryPath;
  NName::NormalizeDirPathPrefix(_directoryPath);
}

STDMETHODIMP CExtractCallbackImp::SetTotal(UINT64 size)
{
  #ifndef _NO_PROGRESS
  ProgressDialog.ProgressSynch.SetProgress(size, 0);
  #endif
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetCompleted(const UINT64 *completeValue)
{
  #ifndef _NO_PROGRESS
  while(true)
  {
    if(ProgressDialog.ProgressSynch.GetStopped())
      return E_ABORT;
    if(!ProgressDialog.ProgressSynch.GetPaused())
      break;
    ::Sleep(100);
  }
  if (completeValue != NULL)
    ProgressDialog.ProgressSynch.SetPos(*completeValue);
  #endif
  return S_OK;
}

void CExtractCallbackImp::CreateComplexDirectory(const UStringVector &dirPathParts)
{
  UString fullPath = _directoryPath;
  for(int i = 0; i < dirPathParts.Size(); i++)
  {
    fullPath += dirPathParts[i];
    NDirectory::MyCreateDirectory(fullPath);
    fullPath += NName::kDirDelimiter;
  }
}

STDMETHODIMP CExtractCallbackImp::GetStream(UINT32 index,
    ISequentialOutStream **outStream, INT32 askExtractMode)
{
  #ifndef _NO_PROGRESS
  if(ProgressDialog.ProgressSynch.GetStopped())
    return E_ABORT;
  #endif
  _outFileStream.Release();
  NCOM::CPropVariant propVariantName;
  RINOK(_archiveHandler->GetProperty(index, kpidPath, &propVariantName));
  UString fullPath;
  if(propVariantName.vt == VT_EMPTY)
    fullPath = _itemDefaultName;
  else 
  {
    if(propVariantName.vt != VT_BSTR)
      return E_FAIL;
    fullPath = propVariantName.bstrVal;
  }
  _filePath = fullPath;

  // m_CurrentFilePath = GetSystemString(fullPath, _codePage);
  
  if(askExtractMode == NArchive::NExtract::NAskMode::kExtract)
  {
    NCOM::CPropVariant propVariant;
    RINOK(_archiveHandler->GetProperty(index, kpidAttributes, &propVariant));
    if (propVariant.vt == VT_EMPTY)
      _processedFileInfo.Attributes = _attributesDefault;
    else
    {
      if (propVariant.vt != VT_UI4)
        return E_FAIL;
      _processedFileInfo.Attributes = propVariant.ulVal;
    }

    RINOK(_archiveHandler->GetProperty(index, kpidIsFolder, &propVariant));
    _processedFileInfo.IsDirectory = VARIANT_BOOLToBool(propVariant.boolVal);

    bool isAnti = false;
    {
      NCOM::CPropVariant propVariantTemp;
      RINOK(_archiveHandler->GetProperty(index, kpidIsAnti, 
          &propVariantTemp));
      if (propVariantTemp.vt == VT_BOOL)
        isAnti = VARIANT_BOOLToBool(propVariantTemp.boolVal);
    }

    RINOK(_archiveHandler->GetProperty(index, kpidLastWriteTime, &propVariant));
    switch(propVariant.vt)
    {
      case VT_EMPTY:
        _processedFileInfo.UTCLastWriteTime = _utcLastWriteTimeDefault;
        break;
      case VT_FILETIME:
        _processedFileInfo.UTCLastWriteTime = propVariant.filetime;
        break;
      default:
        return E_FAIL;
    }

    UStringVector pathParts; 
    SplitPathToParts(fullPath, pathParts);
    if(pathParts.IsEmpty())
      return E_FAIL;

    UString processedPath = fullPath;

    if(!_processedFileInfo.IsDirectory)
      pathParts.DeleteBack();
    if (!pathParts.IsEmpty())
    {
      if (!isAnti)
        CreateComplexDirectory(pathParts);
    }

    UString fullProcessedPath = _directoryPath + processedPath;

    if(_processedFileInfo.IsDirectory)
    {
      _diskFilePath = fullProcessedPath;

      if (isAnti)
        NDirectory::MyRemoveDirectory(_diskFilePath);
      return S_OK;
    }

    NFind::CFileInfoW fileInfo;
    if(NFind::FindFile(fullProcessedPath, fileInfo))
    {
      if (!NDirectory::DeleteFileAlways(fullProcessedPath))
      {
        #ifdef _SILENT
        _message = kCantDeleteFile;
        #else
        MessageBoxW(0, kCantDeleteFile, kErrorTitle, 0);
        #endif
        // g_StdOut << GetOemString(fullProcessedPath);
        // return E_ABORT;
        return E_ABORT;
      }
    }

    if (!isAnti)
    {
      _outFileStreamSpec = new COutFileStream;
      CMyComPtr<ISequentialOutStream> outStreamLoc(_outFileStreamSpec);
      if (!_outFileStreamSpec->Open(fullProcessedPath))
      {
        #ifdef _SILENT
        _message = kCantOpenFile;
        #else
        MessageBoxW(0, kCantOpenFile, kErrorTitle, 0);
        #endif
        return E_ABORT;
      }
      _outFileStream = outStreamLoc;
      *outStream = outStreamLoc.Detach();
    }
    _diskFilePath = fullProcessedPath;
  }
  else
  {
    *outStream = NULL;
  }
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::PrepareOperation(INT32 askExtractMode)
{
  _extractMode = false;
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:
      _extractMode = true;
      break;
  };
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetOperationResult(INT32 resultEOperationResult)
{
  switch(resultEOperationResult)
  {
    case NArchive::NExtract::NOperationResult::kOK:
    {
      break;
    }
    default:
    {
      _numErrors++;
      UString errorMessage;
      _outFileStream.Release();
      switch(resultEOperationResult)
      {
        case NArchive::NExtract::NOperationResult::kUnSupportedMethod:
          errorMessage = kUnsupportedMethod;
          break;
        case NArchive::NExtract::NOperationResult::kCRCError:
          errorMessage = kCRCFailed;
          break;
        case NArchive::NExtract::NOperationResult::kDataError:
          errorMessage = kDataError;
          break;
        /*
        default:
          errorMessage = kUnknownError;
        */
      }
      #ifdef _SILENT
      _message = errorMessage;
      #else
      MessageBoxW(0, errorMessage, kErrorTitle, 0);
      #endif
      return E_FAIL;
    }
  }
  if(_outFileStream != NULL)
    _outFileStreamSpec->File.SetLastWriteTime(&_processedFileInfo.UTCLastWriteTime);
  _outFileStream.Release();
  if (_extractMode)
    NDirectory::MySetFileAttributes(_diskFilePath, _processedFileInfo.Attributes);
  return S_OK;
}

 
