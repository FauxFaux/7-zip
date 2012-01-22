// Zip/Handler.cpp

#include "StdAfx.h"

#include "ZipHandler.h"

#include "Common/Defs.h"
#include "Common/CRC.h"
#include "Common/StringConvert.h"
#include "Common/ComTry.h"

#include "Windows/Time.h"
#include "Windows/PropVariant.h"

#include "../../IPassword.h"
// #include "../../../Compress/Interface/CompressInterface.h"

#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamObjects.h"
// #include "Interface/EnumStatProp.h"
#include "../../Common/StreamObjects.h"

#include "../../Compress/Copy/CopyCoder.h"


#include "../Common/ItemNameUtils.h"
#include "../Common/OutStreamWithCRC.h"
#include "../Common/CoderMixer.h"
#include "../7z/7zMethods.h"


#ifdef COMPRESS_DEFLATE
#include "../../Compress/Deflate/DeflateDecoder.h"
#else
// {23170F69-40C1-278B-0401-080000000000}
DEFINE_GUID(CLSID_CCompressDeflateDecoder, 
0x23170F69, 0x40C1, 0x278B, 0x04, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00);
#endif

#ifdef COMPRESS_DEFLATE64
#include "../../Compress/Deflate/DeflateDecoder.h"
#else
// {23170F69-40C1-278B-0401-090000000000}
DEFINE_GUID(CLSID_CCompressDeflate64Decoder, 
0x23170F69, 0x40C1, 0x278B, 0x04, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00);
#endif

#ifdef COMPRESS_IMPLODE
#include "../../Compress/Implode/ImplodeDecoder.h"
#else
// {23170F69-40C1-278B-0401-060000000000}
DEFINE_GUID(CLSID_CCompressImplodeDecoder, 
0x23170F69, 0x40C1, 0x278B, 0x04, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00);
#endif

#ifdef COMPRESS_BZIP2
#include "../../Compress/BZip2/BZip2Decoder.h"
#else
// {23170F69-40C1-278B-0402-020000000000}
DEFINE_GUID(CLSID_CCompressBZip2Decoder, 
0x23170F69, 0x40C1, 0x278B, 0x04, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00);
#endif

#ifdef CRYPTO_ZIP
#include "../../Crypto/Zip/ZipCipher.h"
#else
// {23170F69-40C1-278B-06F1-0101000000000}
DEFINE_GUID(CLSID_CCryptoZipDecoder, 
0x23170F69, 0x40C1, 0x278B, 0x06, 0xF1, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
#endif

#ifndef EXCLUDE_COM
#include "../Common/CoderLoader.h"
#endif

// using namespace std;

using namespace NWindows;
using namespace NTime;

namespace NArchive {
namespace NZip {

const wchar_t *kHostOS[] = 
{
  L"FAT",
  L"AMIGA",
  L"VMS",
  L"Unix",
  L"VM_CMS",
  L"Atari",  // what if it's a minix filesystem? [cjh]
  L"HPFS",  // filesystem used by OS/2 (and NT 3.x)
  L"Mac",
  L"Z_System",
  L"CPM",
  L"TOPS20", // pkzip 2.50 NTFS 
  L"NTFS", // filesystem used by Windows NT 
  L"QDOS ", // SMS/QDOS
  L"Acorn", // Archimedes Acorn RISC OS
  L"VFAT", // filesystem used by Windows 95, NT
  L"MVS",
  L"BeOS", // hybrid POSIX/database filesystem
                        // BeBOX or PowerMac 
  L"Tandem",
  L"THEOS"
};


static const int kNumHostOSes = sizeof(kHostOS) / sizeof(kHostOS[0]);

static const wchar_t *kUnknownOS = L"Unknown";


/*
enum // PropID
{
  kpidUnPackVersion, 
};
*/

STATPROPSTG kProperties[] = 
{
  { NULL, kpidPath, VT_BSTR},
  { NULL, kpidIsFolder, VT_BOOL},
  { NULL, kpidSize, VT_UI8},
  { NULL, kpidPackedSize, VT_UI8},
  { NULL, kpidLastWriteTime, VT_FILETIME},
  { NULL, kpidAttributes, VT_UI4},

  { NULL, kpidEncrypted, VT_BOOL},
  { NULL, kpidCommented, VT_BOOL},
    
  { NULL, kpidCRC, VT_UI4},

  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidHostOS, VT_BSTR}

  // { L"UnPack Version", kpidUnPackVersion, VT_UI1},
};

const wchar_t *kMethods[] = 
{
  L"Store",
  L"Shrunk",
  L"Reduced1",
  L"Reduced2",
  L"Reduced2",
  L"Reduced3",
  L"Implode",
  L"Tokenizing",
  L"Deflate",
  L"Deflate64",
  L"PKImploding",
  L"Unknown",
  L"BZip2"
};

const int kNumMethods = sizeof(kMethods) / sizeof(kMethods[0]);
const wchar_t *kUnknownMethod = L"Unknown";

CHandler::CHandler():
  m_ArchiveIsOpen(false)
{
  InitMethodProperties();
  m_Method.MethodSequence.Add(NFileHeader::NCompressionMethod::kDeflated);
  m_Method.MethodSequence.Add(NFileHeader::NCompressionMethod::kStored);
}

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  value->vt = VT_EMPTY;
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfProperties(UINT32 *numProperties)
{
  *numProperties = sizeof(kProperties) / sizeof(kProperties[0]);
  return S_OK;
}

STDMETHODIMP CHandler::GetPropertyInfo(UINT32 index,     
      BSTR *name, PROPID *propID, VARTYPE *varType)
{
  if(index >= sizeof(kProperties) / sizeof(kProperties[0]))
    return E_INVALIDARG;
  const STATPROPSTG &srcItem = kProperties[index];
  *propID = srcItem.propid;
  *varType = srcItem.vt;
  *name = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfArchiveProperties(UINT32 *numProperties)
{
  *numProperties = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetArchivePropertyInfo(UINT32 index,     
      BSTR *name, PROPID *propID, VARTYPE *varType)
{
  return E_NOTIMPL;
}

STDMETHODIMP CHandler::GetNumberOfItems(UINT32 *numItems)
{
  *numItems = m_Items.Size();
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UINT32 index, PROPID aPropID,  PROPVARIANT *aValue)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant propVariant;
  const CItemEx &item = m_Items[index];
  switch(aPropID)
  {
    case kpidPath:
      propVariant = NItemName::GetOSName2(
          MultiByteToUnicodeString(item.Name, item.GetCodePage()));
      break;
    case kpidIsFolder:
      propVariant = item.IsDirectory();
      break;
    case kpidSize:
      propVariant = item.UnPackSize;
      break;
    case kpidPackedSize:
      propVariant = item.PackSize;
      break;
    case kpidLastWriteTime:
    {
      FILETIME aLocalFileTime, anUTCFileTime;
      if (DosTimeToFileTime(item.Time, aLocalFileTime))
      {
        if (!LocalFileTimeToFileTime(&aLocalFileTime, &anUTCFileTime))
          anUTCFileTime.dwHighDateTime = anUTCFileTime.dwLowDateTime = 0;
      }
      else
        anUTCFileTime.dwHighDateTime = anUTCFileTime.dwLowDateTime = 0;
      propVariant = anUTCFileTime;
      break;
    }
    case kpidAttributes:
      propVariant = item.GetWinAttributes();
      break;
    case kpidEncrypted:
      propVariant = item.IsEncrypted();
      break;
    case kpidCommented:
      propVariant = item.IsCommented();
      break;
    case kpidCRC:
      propVariant = item.FileCRC;
      break;
    case kpidMethod:
    {
      UString method;
      if (item.CompressionMethod < kNumMethods)
        method = kMethods[item.CompressionMethod];
      else
        method = kUnknownMethod;
      propVariant = method;
      // propVariant = item.CompressionMethod;
      break;
    }
    case kpidHostOS:
      propVariant = (item.MadeByVersion.HostOS < kNumHostOSes) ?
        (kHostOS[item.MadeByVersion.HostOS]) : kUnknownOS;
      break;
  }
  propVariant.Detach(aValue);
  return S_OK;
  COM_TRY_END
}

class CPropgressImp: public CProgressVirt
{
  CMyComPtr<IArchiveOpenCallback> m_OpenArchiveCallback;
public:
  STDMETHOD(SetCompleted)(const UINT64 *numFiles);
  void Init(IArchiveOpenCallback *openArchiveCallback)
    { m_OpenArchiveCallback = openArchiveCallback; }
};

STDMETHODIMP CPropgressImp::SetCompleted(const UINT64 *numFiles)
{
  if (m_OpenArchiveCallback)
    return m_OpenArchiveCallback->SetCompleted(numFiles, NULL);
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *inStream, 
    const UINT64 *maxCheckStartPosition, IArchiveOpenCallback *openArchiveCallback)
{
  COM_TRY_BEGIN
  // try
  {
    if(!m_Archive.Open(inStream, maxCheckStartPosition))
      return S_FALSE;
    m_ArchiveIsOpen = true;
    m_Items.Clear();
    if (openArchiveCallback != NULL)
    {
      RINOK(openArchiveCallback->SetTotal(NULL, NULL));
    }
    CPropgressImp propgressImp;
    propgressImp.Init(openArchiveCallback);
    RINOK(m_Archive.ReadHeaders(m_Items, &propgressImp));
  }
  /*
  catch(...)
  {
    return S_FALSE;
  }
  */
  COM_TRY_END
  return S_OK;
}

STDMETHODIMP CHandler::Close()
{
  m_Archive.Close();
  m_ArchiveIsOpen = false;
  return S_OK;
}

//////////////////////////////////////
// CHandler::DecompressItems

struct CMethodItem
{
  BYTE ZipMethod;
  CMyComPtr<ICompressCoder> Coder;
};

STDMETHODIMP CHandler::Extract(const UINT32* indices, UINT32 numItems,
    INT32 _aTestMode, IArchiveExtractCallback *_anExtractCallback)
{
  COM_TRY_BEGIN
  CMyComPtr<ICryptoGetTextPassword> getTextPassword;
  bool testMode = (_aTestMode != 0);
  CMyComPtr<IArchiveExtractCallback> extractCallback = _anExtractCallback;
  UINT64 totalUnPacked = 0, totalPacked = 0;
  bool allFilesMode = (numItems == UINT32(-1));
  if (allFilesMode)
    numItems = m_Items.Size();
  if(numItems == 0)
    return S_OK;
  UINT32 i;
  for(i = 0; i < numItems; i++)
  {
    const CItemEx &item = m_Items[allFilesMode ? i : indices[i]];
    totalUnPacked += item.UnPackSize;
    totalPacked += item.PackSize;
  }
  extractCallback->SetTotal(totalUnPacked);

  UINT64 currentTotalUnPacked = 0, currentTotalPacked = 0;
  UINT64 currentItemUnPacked, currentItemPacked;
  
 
  #ifndef EXCLUDE_COM
  N7z::LoadMethodMap();
  CCoderLibraries libraries;
  #endif
  CObjectVector<CMethodItem> methodItems;
  /*
  CCoderLibraries _libraries;
  #ifndef COMPRESS_IMPLODE
  CCoderLibrary implodeLib;
  #endif
  

  CMyComPtr<ICompressCoder> implodeDecoder;
  CMyComPtr<ICompressCoder> deflateDecoder;
  CMyComPtr<ICompressCoder> deflate64Decoder;
  CMyComPtr<ICompressCoder> bzip2Decoder;

  #ifndef CRYPTO_ZIP
  CCoderLibrary cryptoLib;
  #endif
  */

  CMyComPtr<ICompressCoder> cryptoDecoder;
  CCoderMixer *mixerCoderSpec;
  CMyComPtr<ICompressCoder> mixerCoder;

  UINT16 mixerCoderMethod;

  for(i = 0; i < numItems; i++, currentTotalUnPacked += currentItemUnPacked,
      currentTotalPacked += currentItemPacked)
  {
    currentItemUnPacked = 0;
    currentItemPacked = 0;

    RINOK(extractCallback->SetCompleted(&currentTotalUnPacked));
    CMyComPtr<ISequentialOutStream> realOutStream;
    INT32 askMode;
    askMode = testMode ? NArchive::NExtract::NAskMode::kTest :
        NArchive::NExtract::NAskMode::kExtract;
    INT32 index = allFilesMode ? i : indices[i];
    const CItemEx &item = m_Items[index];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    if(item.IsDirectory() || item.IgnoreItem())
    {
      // if (!testMode)
      {
        RINOK(extractCallback->PrepareOperation(askMode));
        RINOK(extractCallback->SetOperationResult(NArchive::NExtract::NOperationResult::kOK));
      }
      continue;
    }

    if (!testMode && (!realOutStream)) 
      continue;

    RINOK(extractCallback->PrepareOperation(askMode));
    currentItemUnPacked = item.UnPackSize;
    currentItemPacked = item.PackSize;

    {
      COutStreamWithCRC *outStreamSpec = new COutStreamWithCRC;
      CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
      outStreamSpec->Init(realOutStream);
      realOutStream.Release();
      
      CMyComPtr<ISequentialInStream> inStream;
      inStream.Attach(m_Archive.CreateLimitedStream(item.GetDataPosition(),
          item.PackSize));

      CLocalProgress *localProgressSpec = new CLocalProgress;
      CMyComPtr<ICompressProgressInfo> progress = localProgressSpec;
      localProgressSpec->Init(extractCallback, false);

      CLocalCompressProgressInfo *localCompressProgressSpec = 
          new CLocalCompressProgressInfo;
      CMyComPtr<ICompressProgressInfo> compressProgress = localCompressProgressSpec;
      localCompressProgressSpec->Init(progress, 
          &currentTotalPacked,
          &currentTotalUnPacked);

      if (item.IsEncrypted())
      {
        if (!cryptoDecoder)
        {
          #ifdef CRYPTO_ZIP
          cryptoDecoder = new NCrypto::NZip::CDecoder;
          #else
          RINOK(cryptoLib.LoadAndCreateCoder(
              GetBaseFolderPrefix() + TEXT("\\Crypto\\Zip.dll"),
              CLSID_CCryptoZipDecoder, &cryptoDecoder));
          #endif
        }
        CMyComPtr<ICryptoSetPassword> cryptoSetPassword;
        RINOK(cryptoDecoder.QueryInterface(
            IID_ICryptoSetPassword, &cryptoSetPassword));

        if (!getTextPassword)
          extractCallback.QueryInterface(
              IID_ICryptoGetTextPassword, &getTextPassword);

        if (getTextPassword)
        {
          CMyComBSTR password;
          RINOK(getTextPassword->CryptoGetTextPassword(&password));
          AString anOemPassword = UnicodeStringToMultiByte(
              (const wchar_t *)password, CP_OEMCP);
          RINOK(cryptoSetPassword->CryptoSetPassword(
              (const BYTE *)(const char *)anOemPassword, anOemPassword.Length()));
        }
        else
        {
          RINOK(cryptoSetPassword->CryptoSetPassword(0, 0));
        }
      }

      int m;
      for (m = 0; m < methodItems.Size(); m++)
        if (methodItems[m].ZipMethod == item.CompressionMethod)
          break;
      if (m == methodItems.Size())
      {
        CMethodItem mi;
        mi.ZipMethod = item.CompressionMethod;
        #ifdef EXCLUDE_COM
        switch(item.CompressionMethod)
        {
          case NFileHeader::NCompressionMethod::kStored:
            mi.Coder = new NCompress::CCopyCoder;
            break;
          case NFileHeader::NCompressionMethod::kImploded:
            mi.Coder = new NCompress::NImplode::NDecoder::CCoder;
            break;
          case NFileHeader::NCompressionMethod::kDeflated:
            mi.Coder = new NCompress::NDeflate::NDecoder::CCOMCoder;
            break;
          case NFileHeader::NCompressionMethod::kDeflated64:
            mi.Coder = new NCompress::NDeflate::NDecoder::CCOMCoder64;
            break;
          case NFileHeader::NCompressionMethod::kBZip2:
            mi.Coder = new NCompress::NBZip2::CDecoder;
            break;
          default:
            RINOK(extractCallback->SetOperationResult(
              NArchive::NExtract::NOperationResult::kUnSupportedMethod));
            continue;
        }
        #else
        N7z::CMethodID methodID = { { 0x04, 0x01 } , 3 };
        methodID.ID[2] = item.CompressionMethod;
        if (item.CompressionMethod == NFileHeader::NCompressionMethod::kStored)
        {
          methodID.ID[0] = 0;
          methodID.IDSize = 1;
        }
        else if (item.CompressionMethod == NFileHeader::NCompressionMethod::kBZip2)
        {
          methodID.ID[1] = 0x02;
          methodID.ID[2] = 0x02;
        }
         
        N7z::CMethodInfo methodInfo;
        if (!N7z::GetMethodInfo(methodID, methodInfo))
        {
          RINOK(extractCallback->SetOperationResult(
              NArchive::NExtract::NOperationResult::kUnSupportedMethod));
          continue;
        }
        RINOK(libraries.CreateCoder(methodInfo.FilePath, 
              methodInfo.Decoder, &mi.Coder));
        #endif
        m = methodItems.Add(mi);
      }
      ICompressCoder *coder = methodItems[m].Coder;

      CMyComPtr<ICompressSetDecoderProperties> compressSetDecoderProperties;
      if (coder->QueryInterface(IID_ICompressSetDecoderProperties, (void **)&compressSetDecoderProperties) == S_OK)
      {
        // BYTE properties = (item.Flags & 6);
        BYTE properties = item.Flags;
        CSequentialInStreamImp *inStreamSpec = new CSequentialInStreamImp;
        CMyComPtr<ISequentialInStream> inStreamProperties(inStreamSpec);
        inStreamSpec->Init(&properties, 1);
        RINOK(compressSetDecoderProperties->SetDecoderProperties(inStreamProperties));
      }

      // case NFileHeader::NCompressionMethod::kImploded:
      // switch(item.CompressionMethod)
      try
      {
        HRESULT result;
        if (item.IsEncrypted())
        {
          if (!mixerCoder || mixerCoderMethod != item.CompressionMethod)
          {
            mixerCoder.Release();
            mixerCoderSpec = new CCoderMixer;
            mixerCoder = mixerCoderSpec;
            mixerCoderSpec->AddCoder(cryptoDecoder);
            mixerCoderSpec->AddCoder(coder);
            mixerCoderSpec->FinishAddingCoders();
            mixerCoderMethod = item.CompressionMethod;
          }
          mixerCoderSpec->ReInit();

          switch(item.CompressionMethod)
          {
            case NFileHeader::NCompressionMethod::kStored:
              mixerCoderSpec->SetCoderInfo(0, &currentItemPacked, 
                &currentItemUnPacked);
              mixerCoderSpec->SetCoderInfo(1, NULL, NULL);
              break;
            default:
              mixerCoderSpec->SetCoderInfo(0, &currentItemPacked, NULL);
              mixerCoderSpec->SetCoderInfo(1, NULL, &currentItemUnPacked);
              break;
          }

          mixerCoderSpec->SetProgressCoderIndex(1);
          result = mixerCoder->Code(inStream, outStream,
            NULL, NULL, compressProgress);
        }
        else
        {
          result = coder->Code(inStream, outStream,
            NULL, &currentItemUnPacked, compressProgress);
        }
        if (result == S_FALSE)
          throw "data error";
        if (result != S_OK)
          return result;
      }
      catch(...)
      {
        outStream.Release();
        RINOK(extractCallback->SetOperationResult(
          NArchive::NExtract::NOperationResult::kDataError));
        continue;
      }
      bool crcOK = outStreamSpec->GetCRC() == item.FileCRC;
      outStream.Release();
      RINOK(extractCallback->SetOperationResult(crcOK ? NArchive::NExtract::NOperationResult::kOK :
          NArchive::NExtract::NOperationResult::kCRCError))
    }
  }
  return S_OK;
  COM_TRY_END
}

}}
