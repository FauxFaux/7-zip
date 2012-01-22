// Crypto/RarAES/RarAES.h
// This code is based on UnRar sources

#include "StdAfx.h"

#include "../../Common/StreamObjects.h"
#include "../../Archive/Common/CoderLoader.h"

#include "Windows/Defs.h"

#include "RarAES.h"
#include "sha1.h"

extern void GetCryptoFolderPrefix(TCHAR *path);

// {23170F69-40C1-278B-0601-010000000000}
DEFINE_GUID(CLSID_CCrypto_AES128_Decoder, 
0x23170F69, 0x40C1, 0x278B, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00);

namespace NCrypto {
namespace NRar29 {

CDecoder::CDecoder():
  _thereIsSalt(false),
  _needCalculate(true)
{
  for (int i = 0; i < sizeof(_salt); i++)
    _salt[i] = 0;
}

STDMETHODIMP CDecoder::SetDecoderProperties(ISequentialInStream *inStream)
{
  bool thereIsSaltPrev = _thereIsSalt;
  _thereIsSalt = false;
  UINT32 processedSize;
  BYTE salt[8];
  RINOK(inStream->Read(salt, sizeof(salt), &processedSize));
  if (processedSize == 0)
    _thereIsSalt = false;
  if (processedSize != sizeof(salt))
    return E_INVALIDARG;
  _thereIsSalt = true;
  bool same = false;
  if (_thereIsSalt == thereIsSaltPrev)
  {
    same = true;
    if (_thereIsSalt)
    {
      for (int i = 0; i < sizeof(_salt); i++)
        if (_salt[i] != salt[i])
        {
          same = false;
          break;
        }
    }
  }
  for (int i = 0; i < sizeof(_salt); i++)
    _salt[i] = salt[i];
  if (!_needCalculate && !same)
    _needCalculate = true;
  return S_OK;
}

STDMETHODIMP CDecoder::CryptoSetPassword(const BYTE *data, UINT32 size)
{
  bool same = false;
  if (size == buffer.GetCapacity())
  {
    same = true;
    for (UINT32 i = 0; i < size; i++)
      if (data[i] != buffer[i])
      {
        same = false;
        break;
      }
  }
  if (!_needCalculate && !same)
    _needCalculate = true;
  buffer.SetCapacity(size);
  memcpy(buffer, data, size);
  return S_OK;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, UINT64 const *inSize, 
      const UINT64 *outSize,ICompressProgressInfo *progress)
{
  if (_needCalculate)
  {
    const MAXPASSWORD = 128;
    const SALT_SIZE = 8;
    
    BYTE rawPassword[2 * MAXPASSWORD+ SALT_SIZE];
    
    memcpy(rawPassword, buffer, buffer.GetCapacity());
    
    int rawLength = buffer.GetCapacity();
    
    if (_thereIsSalt)
    {
      memcpy(rawPassword + rawLength, _salt, SALT_SIZE);
      rawLength += SALT_SIZE;
    }
    
    hash_context c;
    hash_initial(&c);
    
    const int hashRounds = 0x40000;
    int i;
    for (i = 0; i < hashRounds; i++)
    {
      hash_process(&c, rawPassword, rawLength);
      BYTE pswNum[3];
      pswNum[0] = (BYTE)i;
      pswNum[1] = (BYTE)(i >> 8);
      pswNum[2] = (BYTE)(i >> 16);
      hash_process(&c, pswNum, 3);
      if (i % (hashRounds / 16) == 0)
      {
        hash_context tempc = c;
        UINT32 digest[5];
        hash_final(&tempc, digest);
        aesInit[i / (hashRounds / 16)] = (BYTE)digest[4];
      }
    }
    UINT32 digest[5];
    hash_final(&c, digest);
    for (i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        aesKey[i * 4 + j] = (BYTE)(digest[i] >> (j * 8));
  }
  _needCalculate = false;

  TCHAR aesLibPath[MAX_PATH + 64];
  GetCryptoFolderPrefix(aesLibPath);
  lstrcat(aesLibPath, TEXT("AES.dll"));
  CCoderLibrary aesLib;
  CMyComPtr<ICompressCoder2> aesDecoder;
  RINOK(aesLib.LoadAndCreateCoder2(aesLibPath, CLSID_CCrypto_AES128_Decoder, &aesDecoder));

  CSequentialInStreamImp *ivStreamSpec = new CSequentialInStreamImp;
  CMyComPtr<ISequentialInStream> ivStream(ivStreamSpec);
  ivStreamSpec->Init(aesInit, 16);

  CSequentialInStreamImp *keyStreamSpec = new CSequentialInStreamImp;
  CMyComPtr<ISequentialInStream> keyStream(keyStreamSpec);
  keyStreamSpec->Init(aesKey, 16);

  ISequentialInStream *inStreams[3] = { inStream, ivStream, keyStream };
  UINT64 ivSize = 16;
  UINT64 keySize = 16;
  const UINT64 *inSizes[3] = { inSize, &ivSize, &ivSize, };
  return aesDecoder->Code(inStreams, inSizes, 3, 
      &outStream, &outSize, 1, progress);
}

}}
