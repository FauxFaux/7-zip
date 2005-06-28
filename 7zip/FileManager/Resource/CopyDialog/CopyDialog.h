// CopyDialog.h

#ifndef __COPYDIALOG_H
#define __COPYDIALOG_H

#include "Windows/Control/Dialog.h"
#include "Windows/Control/ComboBox.h"
#include "resource.h"

class CCopyDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _path;
  virtual void OnOK();
  virtual bool OnInit();
  void OnButtonSetPath();
  bool OnButtonClicked(int buttonID, HWND buttonHWND);
public:
  UString Title;
  UString Static;
  UString Value;
  UStringVector Strings;

  INT_PTR Create(HWND parentWindow = 0)
    { return CModalDialog::Create(MAKEINTRESOURCE(IDD_DIALOG_COPY), parentWindow); }
};

#endif
