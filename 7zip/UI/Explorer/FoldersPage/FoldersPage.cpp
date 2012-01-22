// FoldersDialog.cpp

#include "StdAfx.h"

#include "resource.h"
#include "FoldersPage.h"

#include "Common/StringConvert.h"
#include "Windows/Defs.h"
#include "Windows/Shell.h"
#include "Windows/ResourceString.h"

#include "../../../FileManager/HelpUtils.h"
#include "../../Common/ZipRegistry.h"

#include "../../../FileManager/LangUtils.h"

using namespace NWindows;

static CIDLangPair kIDLangPairs[] = 
{
  { IDC_FOLDERS_STATIC_WORKING_FOLDER,    0x01000210 },
  { IDC_FOLDERS_WORK_RADIO_SYSTEM,        0x01000211 },
  { IDC_FOLDERS_WORK_RADIO_CURRENT,       0x01000212 },
  { IDC_FOLDERS_WORK_RADIO_SPECIFIED,     0x01000213 },
  { IDC_FOLDERS_WORK_CHECK_FOR_REMOVABLE, 0x01000214 }
};

static const int kWorkModeButtons[] =
{
  IDC_FOLDERS_WORK_RADIO_SYSTEM,
  IDC_FOLDERS_WORK_RADIO_CURRENT,
  IDC_FOLDERS_WORK_RADIO_SPECIFIED
};

static const int kNumWorkModeButtons = sizeof(kWorkModeButtons) / sizeof(kWorkModeButtons[0]);
 
bool CFoldersPage::OnInit() 
{
  LangSetDlgItemsText(HWND(*this), kIDLangPairs, sizeof(kIDLangPairs) / sizeof(kIDLangPairs[0]));
  // CZipRegistryManager aRegistryManager;
  ReadWorkDirInfo(m_WorkDirInfo);

  CheckButton(IDC_FOLDERS_WORK_CHECK_FOR_REMOVABLE, m_WorkDirInfo.ForRemovableOnly);
  
  CheckRadioButton(kWorkModeButtons[0], kWorkModeButtons[kNumWorkModeButtons - 1], 
      kWorkModeButtons[m_WorkDirInfo.Mode]);

  m_WorkPath.Init(*this, IDC_FOLDERS_WORK_EDIT_PATH);
  m_ButtonSetWorkPath.Init(*this, IDC_FOLDERS_WORK_BUTTON_PATH);

  m_WorkPath.SetText(m_WorkDirInfo.Path);

  MyEnableControls();
  
  return CPropertyPage::OnInit();
}

int CFoldersPage::GetWorkMode() const
{
  for (int i = 0; i < kNumWorkModeButtons; i++)
    if(IsButtonCheckedBool(kWorkModeButtons[i]))
      return i;
  throw 0;
}

void CFoldersPage::MyEnableControls()
{
  bool anEnablePath = (GetWorkMode() == NWorkDir::NMode::kSpecified);
  m_WorkPath.Enable(anEnablePath);
  m_ButtonSetWorkPath.Enable(anEnablePath);
}

void CFoldersPage::GetWorkDir(NWorkDir::CInfo &aWorkDirInfo)
{
  m_WorkPath.GetText(aWorkDirInfo.Path);
  aWorkDirInfo.ForRemovableOnly = IsButtonCheckedBool(IDC_FOLDERS_WORK_CHECK_FOR_REMOVABLE);
  aWorkDirInfo.Mode = NWorkDir::NMode::EEnum(GetWorkMode());
}

/*
bool CFoldersPage::WasChanged()
{
  NWorkDir::CInfo aWorkDirInfo;
  GetWorkDir(aWorkDirInfo);
  return (aWorkDirInfo.Mode != m_WorkDirInfo.Mode ||
      aWorkDirInfo.ForRemovableOnly != m_WorkDirInfo.ForRemovableOnly ||
      aWorkDirInfo.Path.Compare(m_WorkDirInfo.Path) != 0);
}
*/

void CFoldersPage::ModifiedEvent()
{
  Changed();
  /*
  if (WasChanged())
    Changed();
  else
    UnChanged();
  */
}

bool CFoldersPage::OnButtonClicked(int aButtonID, HWND aButtonHWND)
{ 
  for (int i = 0; i < kNumWorkModeButtons; i++)
    if (aButtonID == kWorkModeButtons[i])
    {
      MyEnableControls();
      ModifiedEvent();
      return true;
    }
  switch(aButtonID)
  {
    case IDC_FOLDERS_WORK_BUTTON_PATH:
      OnFoldersWorkButtonPath();
      break;
    case IDC_FOLDERS_WORK_CHECK_FOR_REMOVABLE:
      break;
    default:
      return CPropertyPage::OnButtonClicked(aButtonID, aButtonHWND);
  }
  ModifiedEvent();
  return true;
}

bool CFoldersPage::OnCommand(int aCode, int anItemID, LPARAM lParam)
{
  if (aCode == EN_CHANGE && anItemID == IDC_FOLDERS_WORK_EDIT_PATH)
  {
    ModifiedEvent();
    return true;
  }
  return CPropertyPage::OnCommand(aCode, anItemID, lParam);
}

void CFoldersPage::OnFoldersWorkButtonPath() 
{
  CSysString currentPath;
  m_WorkPath.GetText(currentPath);
  
  UString title = LangLoadStringW(IDS_FOLDERS_SET_WORK_PATH_TITLE, 0x01000281);

  CSysString resultPath;
  if (NShell::BrowseForFolder(HWND(*this), GetSystemString(title), 
      currentPath, resultPath))
    m_WorkPath.SetText(resultPath);
}

LONG CFoldersPage::OnApply() 
{
  GetWorkDir(m_WorkDirInfo);
  SaveWorkDirInfo(m_WorkDirInfo);
  return PSNRET_NOERROR;
}

static LPCWSTR kFoldersTopic = L"fm/plugins/7-zip/options.htm#folders";

void CFoldersPage::OnNotifyHelp() 
{
  ShowHelpWindow(NULL, kFoldersTopic);
}
