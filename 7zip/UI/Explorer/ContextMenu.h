// ContextMenu.h

#ifndef __CONTEXTMENU_H
#define __CONTEXTMENU_H

// {23170F69-40C1-278A-1000-000100020000}
DEFINE_GUID(CLSID_CZipContextMenu, 
0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00);

#include <vector>

#include "Common/String.h"

#include "../../FileManager/PluginInterface.h"



class CZipContextMenu: 
  public IContextMenu,
  public IShellExtInit,
  public IInitContextMenu,
  public CComObjectRoot,
  public CComCoClass<CZipContextMenu, &CLSID_CZipContextMenu>
{

public:

  enum ECommandInternalID
  {
    kCommandNULL,
    kOpen,
    kExtract,
    kExtractHere,
    kExtractTo,
    kTest,
    kCompress,
    kCompressTo,
    kCompressEmail,
    kCompressToEmail
  };
  
  struct CCommandMapItem
  {
    ECommandInternalID CommandInternalID;
    UString Verb;
    UString HelpString;
    UString Folder;
    UString Archive;
  };

BEGIN_COM_MAP(CZipContextMenu)
  COM_INTERFACE_ENTRY(IContextMenu)
  COM_INTERFACE_ENTRY(IShellExtInit)
  COM_INTERFACE_ENTRY(IInitContextMenu)
END_COM_MAP()

DECLARE_NOT_AGGREGATABLE(CZipContextMenu)

DECLARE_REGISTRY(CZipContextMenu, 
    // _T("SevenZip.ContextMenu.1"), _T("SevenZip.ContextMenu"), 
    TEXT("SevenZip.1"), TEXT("SevenZip"),
    UINT(0), THREADFLAGS_APARTMENT)

  ///////////////////////////////
  // IShellExtInit

  STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder, 
      LPDATAOBJECT dataObject, HKEY hkeyProgID);

  /////////////////////////////
  // IContextMenu
  
  STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu,
      UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
  STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO lpici);
  STDMETHOD(GetCommandString)(UINT idCmd, UINT uType, UINT *pwReserved,
      LPSTR pszName, UINT cchMax);


  // IInitContextMenu
  STDMETHOD(InitContextMenu)(const wchar_t *folder, const wchar_t **names, UINT32 numFiles);  
private:
  UStringVector _fileNames;
  std::vector<CCommandMapItem> _commandMap;
  HRESULT GetFileNames(LPDATAOBJECT dataObject, CSysStringVector &fileNames);
  UINT FindVerb(const UString &verb);

  void FillCommand(ECommandInternalID id, UString &mainString, 
      CCommandMapItem &commandMapItem);
  void FillCommand2(ECommandInternalID id, UString &mainString, 
      CCommandMapItem &commandMapItem);
};

#endif
