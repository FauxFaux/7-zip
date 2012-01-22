// UpdateGUI.cpp

#include "StdAfx.h"

#include "UpdateGUI.h"

#include "resource.h"
#include "Common/StringConvert.h"
#include "Common/IntToString.h"

#include "Windows/FileDir.h"
#include "Windows/Error.h"
#include "Windows/FileFind.h"
#include "Windows/Thread.h"

#include "../../FileManager/FormatUtils.h"
#include "../../FileManager/ExtractCallback.h"
#include "../../FileManager/StringUtils.h"

#include "../Common/ArchiveExtractCallback.h"
#include "../Common/WorkDir.h"
#include "../Explorer/MyMessages.h"
#include "../Resource/Extract/resource.h"

#include "OpenCallbackGUI.h"
#include "CompressDialog.h"
#include "UpdateGUI.h"

using namespace NWindows;
using namespace NFile;

static const wchar_t *kIncorrectOutDir = L"Incorrect output directory path";
static const wchar_t *kDefaultSfxModule = L"7zC.sfx";
static const wchar_t *kSFXExtension = L"exe";

struct CThreadUpdating
{
  CUpdateCallbackGUI *UpdateCallbackGUI;

  const NWildcard::CCensor *WildcardCensor;
  CUpdateOptions *Options;
  COpenCallbackGUI *OpenCallback;

  CUpdateErrorInfo *ErrorInfo;
  HRESULT Result;
  
  DWORD Process()
  {
    UpdateCallbackGUI->ProgressDialog.WaitCreating();
    try
    {
      Result = UpdateArchive(*WildcardCensor, *Options, 
        *ErrorInfo, OpenCallback, UpdateCallbackGUI);
    }
    catch(const UString &s)
    {
      ErrorInfo->Message = s;
      Result = E_FAIL;
    } 
    catch(const wchar_t *s)
    {
      ErrorInfo->Message = s;
      Result = E_FAIL;
    } 
    catch(const char *s)
    {
      ErrorInfo->Message = GetUnicodeString(s);
      Result = E_FAIL;
    }
    catch(...)
    {
      Result = E_FAIL;
    }
    UpdateCallbackGUI->ProgressDialog.MyClose();
    return 0;
  }
  static DWORD WINAPI MyThreadFunction(void *param)
  {
    return ((CThreadUpdating *)param)->Process();
  }
};

static void AddProp(CObjectVector<CProperty> &properties, 
    const UString &name, const UString &value)
{
  CProperty prop;
  prop.Name = name;
  prop.Value = value;
  properties.Add(prop);
}

static void AddProp(CObjectVector<CProperty> &properties, 
    const UString &name, UInt32 value)
{
  wchar_t tmp[32];
  ConvertUInt64ToString(value, tmp);
  AddProp(properties, name, tmp);
}

static void AddProp(CObjectVector<CProperty> &properties, 
    const UString &name, bool value)
{
  AddProp(properties, name, value ? UString(L"on"): UString(L"off"));
}

static void ParseAndAddPropertires(CObjectVector<CProperty> &properties, 
    const UString &propertiesString)
{
  UStringVector strings;
  SplitString(propertiesString, strings);
  for (int i = 0; i < strings.Size(); i++)
  {
    const UString &s = strings[i];
    CProperty property;
    int index = s.Find(L'=');
    if (index < 0)
      property.Name = s;
    else
    {
      property.Name = s.Left(index);
      property.Value = s.Mid(index + 1);
    }
    properties.Add(property);
  }
}

static void SetOutProperties(
    CObjectVector<CProperty> &properties,
    bool is7z,
    UInt32 level, 
    const UString &method,
    UInt32 dictionary,
    bool orderMode,
    UInt32 order,
    bool solidModeIsAllowed, bool solidMode, 
    bool multiThreadIsAllowed, bool multiThread, 
    bool encryptHeadersIsAllowed, bool encryptHeaders,
    bool sfxMode)
{
  if (level != (UInt32)(Int32)-1)
    AddProp(properties, L"x", (UInt32)level);
  if (!method.IsEmpty())
    AddProp(properties, is7z ? L"0": L"m", method);
  if (dictionary != (UInt32)(Int32)-1)
  {
    UString name;
    if (is7z)
      name = L"0";
    if (orderMode)
      name += L"mem";
    else
      name += L"d";
    wchar_t s[32];
    ConvertUInt64ToString(dictionary, s);
    wcscat(s, L"B");
    AddProp(properties, name, UString(s));
  }
  if (order != (UInt32)(Int32)-1)
  {
    UString name;
    if (is7z)
      name = L"0";
    if (orderMode)
      name += L"o";
    else
      name += L"fb";
    AddProp(properties, name, (UInt32)order);
  }
  
  if (encryptHeadersIsAllowed)
    AddProp(properties, L"he", encryptHeaders);
  if (solidModeIsAllowed)
    AddProp(properties, L"s", solidMode);
  if (multiThreadIsAllowed)
    AddProp(properties, L"mt", multiThread);
}

static HRESULT ShowDialog(const NWildcard::CCensor &censor,
    CUpdateOptions &options, CUpdateCallbackGUI *callback)
{
  if (options.Commands.Size() != 1)
    throw "It must be one command";
  CObjectVector<CArchiverInfo> archivers;
  CArchiverInfo archiverInfo;
  ReadArchiverInfoList(archivers);
  UString currentDirPrefix;
  {
    if (!NDirectory::MyGetCurrentDirectory(currentDirPrefix))
      return E_FAIL;
    NName::NormalizeDirPathPrefix(currentDirPrefix);
  }

  bool oneFile = false;
  NFind::CFileInfoW fileInfo;
  if (censor.Pairs.Size() > 0)
  {
    const NWildcard::CPair &pair = censor.Pairs[0];
    if (pair.Head.Items.Size() > 0)
    {
      const NWildcard::CItem &item = pair.Head.Items[0];
      if (item.ForFile)
      {
        UString name = pair.Prefix;
        for (int i = 0; i < item.PathParts.Size(); i++)
        {
          if (i > 0)
            name += L'\\';
          name += item.PathParts[i];
        }
        if (NFind::FindFile(name, fileInfo))
        {
          if (censor.Pairs.Size() == 1 && pair.Head.Items.Size() == 1)
            oneFile = !fileInfo.IsDirectory();
        }
      }
    }
  }
    
  CCompressDialog dialog;
  NCompressDialog::CInfo &di = dialog.Info;
  for(int i = 0; i < archivers.Size(); i++)
  {
    const CArchiverInfo &ai = archivers[i];
    if (ai.UpdateEnabled && (oneFile || !ai.KeepName))
      dialog.m_ArchiverInfoList.Add(ai);
  }
  if(dialog.m_ArchiverInfoList.Size() == 0)
  {
    MyMessageBox(L"No Update Engines");
    return E_FAIL;
  }

  // di.ArchiveName = options.ArchivePath.GetFinalPath();
  di.ArchiveName = options.ArchivePath.GetPathWithoutExt();
  dialog.OriginalFileName = fileInfo.Name;
    
  di.CurrentDirPrefix = currentDirPrefix;
  di.SFXMode = options.SfxMode;
  
  di.Solid = true;
  di.MultiThread = false;

  if (callback->PasswordIsDefined)
    di.Password = callback->Password;
    
  di.KeepName = !oneFile;
    
  if(dialog.Create(0) != IDOK)
    return E_ABORT;
    
  options.VolumesSizes = di.VolumeSizes;
  /*
  if (di.VolumeSizeIsDefined)
  {
    MyMessageBox(L"Splitting to volumes is not supported");
    return E_FAIL;
  }
  */
  
  NUpdateArchive::CActionSet &actionSet = options.Commands.Front().ActionSet;
  
  switch(di.UpdateMode)
  {
    case NCompressDialog::NUpdateMode::kAdd:
      actionSet = NUpdateArchive::kAddActionSet;
      break;
    case NCompressDialog::NUpdateMode::kUpdate:
      actionSet = NUpdateArchive::kUpdateActionSet;
      break;
    case NCompressDialog::NUpdateMode::kFresh:
      actionSet = NUpdateArchive::kFreshActionSet;
      break;
    case NCompressDialog::NUpdateMode::kSynchronize:
      actionSet = NUpdateArchive::kSynchronizeActionSet;
      break;
    default:
      throw 1091756;
  }
  archiverInfo = dialog.m_ArchiverInfoList[di.ArchiverInfoIndex];
  if (callback->PasswordIsDefined = (!di.Password.IsEmpty()))
    callback->Password = di.Password;

  options.MethodMode.Properties.Clear();

  SetOutProperties(
      options.MethodMode.Properties,
      archiverInfo.Name.CompareNoCase(L"7z") == 0,
      di.Level, 
      di.Method, 
      di.Dictionary, 
      di.OrderMode, di.Order,
      di.SolidIsAllowed, di.Solid, 
      di.MultiThreadIsAllowed, di.MultiThread, 
      di.EncryptHeadersIsAllowed, di.EncryptHeaders,
      di.SFXMode);
  
  ParseAndAddPropertires(options.MethodMode.Properties, 
      di.Options);

  if (di.SFXMode)
    options.SfxMode = true;
  options.MethodMode.FilePath = archiverInfo.FilePath;
  options.MethodMode.ClassID = archiverInfo.ClassID;

  options.ArchivePath.VolExtension = archiverInfo.GetMainExtension();
  if(di.SFXMode)
    options.ArchivePath.BaseExtension = kSFXExtension;
  else
    options.ArchivePath.BaseExtension = options.ArchivePath.VolExtension;
  options.ArchivePath.ParseFromPath(di.ArchiveName);

  NWorkDir::CInfo workDirInfo;
  ReadWorkDirInfo(workDirInfo);
  options.WorkingDir.Empty();
  if (workDirInfo.Mode != NWorkDir::NMode::kCurrent)
  {
    UString fullPath;
    NDirectory::MyGetFullPathName(di.ArchiveName, fullPath);
    options.WorkingDir = GetWorkDir(workDirInfo, fullPath);
    NFile::NDirectory::CreateComplexDirectory(options.WorkingDir);
  }
  return S_OK;
}

HRESULT UpdateGUI(
    const NWildcard::CCensor &censor, 
    CUpdateOptions &options,
    bool showDialog,
    CUpdateErrorInfo &errorInfo,
    COpenCallbackGUI *openCallback,
    CUpdateCallbackGUI *callback)
{
  if (showDialog)
  {
    RINOK(ShowDialog(censor, options, callback));
  }
  if (options.SfxMode && options.SfxModule.IsEmpty())
    options.SfxModule = kDefaultSfxModule;

  CThreadUpdating tu;

  tu.UpdateCallbackGUI = callback;
  tu.UpdateCallbackGUI->Init();

  tu.WildcardCensor = &censor;
  tu.Options = &options;
  tu.OpenCallback = openCallback;
  tu.ErrorInfo = &errorInfo;

  CThread thread;
  if (!thread.Create(CThreadUpdating::MyThreadFunction, &tu))
    throw 271824;
  const UString title = LangLoadStringW(IDS_PROGRESS_COMPRESSING, 0x02000DC0);
  tu.UpdateCallbackGUI->StartProgressDialog(title);
  return tu.Result;
}


