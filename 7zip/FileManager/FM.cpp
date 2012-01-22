// FAM.cpp

#include "StdAfx.h"

#include "resource.h"
#include "Panel.h"

#include "Common/Defs.h"
#include "Common/StringConvert.h"

#include "Windows/Control/Toolbar.h"
#include "Windows/Error.h"
#include "Windows/COM.h"

#include "ViewSettings.h"

#include "App.h"
#include "StringUtils.h"

#include "MyLoadMenu.h"
#include "LangUtils.h"

using namespace NWindows;

// NWindows::NCOM::CComInitializer aComInitializer;

#define MAX_LOADSTRING 100

#define MENU_HEIGHT 26

HINSTANCE	 g_hInstance;	
HWND g_HWND;
static UString g_MainPath;

const int kNumDefaultPanels = 1;

const int kSplitterWidth = 4;
int kSplitterRateMax = 1 << 16;

// bool OnMenuCommand(HWND hWnd, int id);

static CSysString GetProgramPath()
{
  TCHAR fullPath[MAX_PATH + 1];
  ::GetModuleFileName(g_hInstance, fullPath, MAX_PATH);
  return fullPath;
}

CSysString GetProgramFolderPrefix()
{
  CSysString path = GetProgramPath();
  int pos = path.ReverseFind(TEXT('\\'));
  return path.Left(pos + 1);
}


class CSplitterPos
{
  int _ratio; // 10000 is max
  int _pos;
  int _fullWidth;
  void SetRatioFromPos(HWND hWnd)
    { _ratio = (_pos + kSplitterWidth / 2) * kSplitterRateMax / 
        MyMax(GetWidth(hWnd), 1); }
public:
  int GetPos() const
    { return _pos; }
  int GetWidth(HWND hWnd) const
  {
    RECT rect;
    ::GetClientRect(hWnd, &rect);
    return rect.right;
  }
  void SetRatio(HWND hWnd, int aRatio)
  { 
    _ratio = aRatio; 
    SetPosFromRatio(hWnd);
  }
  void SetPosPure(HWND hWnd, int pos)
  {
    int posMax = GetWidth(hWnd) - kSplitterWidth;
    if (pos > posMax)
      pos = posMax;
    if (pos < 0)
      pos = 0;
    _pos = pos;
  }
  void SetPos(HWND hWnd, int pos)
  {
    _fullWidth = GetWidth(hWnd);
    SetPosPure(hWnd, pos);
    SetRatioFromPos(hWnd);
  }
  void SetPosFromRatio(HWND hWnd)
  { 
    int fullWidth = GetWidth(hWnd);
    if (_fullWidth != fullWidth)
    {
      _fullWidth = fullWidth;
      SetPosPure(hWnd, GetWidth(hWnd) * _ratio / kSplitterRateMax - kSplitterWidth / 2); 
    }
  }
};

bool g_CanChangeSplitter = false;
UINT32 g_SplitterPos = 0;
CSplitterPos g_Splitter;
bool g_PanelsInfoDefined = false;

int g_StartCaptureMousePos;
int g_StartCaptureSplitterPos;

CApp g_App;

void MoveSubWindows(HWND hWnd);
void OnSize(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

//  FUNCTION: InitInstance(HANDLE, int)
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND	hWnd = NULL;
	TCHAR	windowClass[MAX_LOADSTRING];		// The window class name
  lstrcpy(windowClass, TEXT("FM"));

	g_hInstance = hInstance;

  ReloadLang();

  // LoadString(hInstance, IDS_CLASS, windowClass, MAX_LOADSTRING);

  // LoadString(hInstance, IDS_APP_TITLE, title, MAX_LOADSTRING);
  UString title = LangLoadStringW(IDS_APP_TITLE, 0x03000000);

	/*
  //If it is already running, then focus on the window
	hWnd = FindWindow(windowClass, title);	
	if (hWnd) 
	{
		SetForegroundWindow ((HWND) (((DWORD)hWnd) | 0x01));    
		return 0;
	} 
  */

	WNDCLASS	wc;

  // wc.style			= CS_HREDRAW | CS_VREDRAW;
  wc.style = 0;
  wc.lpfnWndProc		= (WNDPROC) WndProc;
  wc.cbClsExtra		= 0;
  wc.cbWndExtra		= 0;
  wc.hInstance		= hInstance;
  wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FAM));

  // wc.hCursor			= LoadCursor (NULL, IDC_ARROW);
  wc.hCursor			= ::LoadCursor(0, IDC_SIZEWE);
  // wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
  wc.hbrBackground	= (HBRUSH) (COLOR_BTNFACE + 1);

  wc.lpszMenuName		= MAKEINTRESOURCE(IDM_MENU);
  wc.lpszClassName	= windowClass;

	RegisterClass(&wc);
	
  // RECT	rect;
	// GetClientRect(hWnd, &rect);

  DWORD style = WS_OVERLAPPEDWINDOW;
  // DWORD style = 0;
  
  RECT rect;
  bool maximized = false;
  int x , y, xSize, ySize;
  x = y = xSize = ySize = CW_USEDEFAULT;
  bool windowPosIsRead = ReadWindowSize(rect, maximized);

  if (windowPosIsRead)
  {
    // x = rect.left;
    // y = rect.top;
    xSize = rect.right - rect.left;
    ySize = rect.bottom - rect.top;
  }

  UINT32 numPanels, currentPanel;
  g_PanelsInfoDefined = ReadPanelsInfo(numPanels, currentPanel, g_SplitterPos);
  if (g_PanelsInfoDefined)
  {
    if (numPanels < 1 || numPanels > 2)
      numPanels = kNumDefaultPanels;
    if (currentPanel >= 2)
      currentPanel = 0;
  }
  else
  {
    numPanels = kNumDefaultPanels;
    currentPanel = 0;
  }
  g_App.NumPanels = numPanels;
  g_App.LastFocusedPanel = currentPanel;

	hWnd = CreateWindow(windowClass, GetSystemString(title), style,
		  x, y, xSize, ySize, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return FALSE;
  g_HWND = hWnd;

  CWindow window(hWnd);

  WINDOWPLACEMENT placement;
  placement.length = sizeof(placement);
  if (window.GetPlacement(&placement))
  {
    if (nCmdShow == SW_SHOWNORMAL || nCmdShow == SW_SHOW || 
        nCmdShow == SW_SHOWDEFAULT)
    {
      if (maximized)
        placement.showCmd = SW_SHOWMAXIMIZED;
      else
        placement.showCmd = SW_SHOWNORMAL;
    }
    else
      placement.showCmd = nCmdShow;
    if (windowPosIsRead)
      placement.rcNormalPosition = rect;
    window.SetPlacement(&placement);
    // window.Show(nCmdShow);
  }
  else
    window.Show(nCmdShow);
	return TRUE;
}

/*
static void GetCommands(const UString &aCommandLine, UString &aCommands)
{
  UString aProgramName;
  aCommands.Empty();
  bool aQuoteMode = false;
  for (int i = 0; i < aCommandLine.Length(); i++)
  {
    wchar_t aChar = aCommandLine[i];
    if (aChar == L'\"')
      aQuoteMode = !aQuoteMode;
    else if (aChar == L' ' && !aQuoteMode)
    {
      if (!aQuoteMode)
      {
        i++;
        break;
      }
    }
    else 
      aProgramName += aChar;
  }
  aCommands = aCommandLine.Mid(i);
}
*/

DWORD GetDllVersion(LPCTSTR lpszDllName)
{
  HINSTANCE hinstDll;
  DWORD dwVersion = 0;
  hinstDll = LoadLibrary(lpszDllName);
  if(hinstDll)
  {
    DLLGETVERSIONPROC pDllGetVersion;
    pDllGetVersion = (DLLGETVERSIONPROC) GetProcAddress(hinstDll, "DllGetVersion");
    
    /*Because some DLLs might not implement this function, you
    must test for it explicitly. Depending on the particular 
    DLL, the lack of a DllGetVersion function can be a useful
    indicator of the version.
    */
    if(pDllGetVersion)
    {
      DLLVERSIONINFO dvi;
      HRESULT hr;
      
      ZeroMemory(&dvi, sizeof(dvi));
      dvi.cbSize = sizeof(dvi);
      
      hr = (*pDllGetVersion)(&dvi);
      
      if(SUCCEEDED(hr))
      {
        dwVersion = MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
      }
    }
    FreeLibrary(hinstDll);
  }
  return dwVersion;
}

DWORD g_ComCtl32Version;

int WINAPI WinMain(	HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					LPSTR    lpCmdLine,
					int       nCmdShow)
{
  InitCommonControls();

  g_ComCtl32Version = ::GetDllVersion(TEXT("comctl32.dll"));

  // OleInitialize is required for drag and drop.
  OleInitialize(NULL); 
  // Maybe needs CoInitializeEx also ?
  // NCOM::CComInitializer comInitializer;

  UString programString, commandsString;
  // MessageBoxW(0, GetCommandLineW(), L"", 0);
  SplitStringToTwoStrings(GetCommandLineW(), programString, commandsString);

  commandsString.Trim();
  UString paramString, tailString;
  SplitStringToTwoStrings(commandsString, paramString, tailString);
  paramString.Trim();
 
  if (!paramString.IsEmpty())
  {
    g_MainPath = paramString;
    // MessageBoxW(0, paramString, L"", 0);
  }


	MSG msg;
	if (!InitInstance (hInstance, nCmdShow)) 
		return FALSE;

  MyLoadMenu(g_HWND);

  HACCEL  hAccels = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(g_HWND, hAccels, &msg)) 
		{
      // if (g_Hwnd != NULL || !IsDialogMessage(g_Hwnd, &msg))
      // if (!IsDialogMessage(g_Hwnd, &msg))
      {
        TranslateMessage(&msg);
			  DispatchMessage(&msg);
      }
		}
	}

  g_HWND = 0;
  OleUninitialize();
	return msg.wParam;
}

static void SaveWindowInfo(HWND aWnd)
{
  /*
  RECT rect;
  if (!::GetWindowRect(aWnd, &rect))
    return;
  */
  WINDOWPLACEMENT placement;
  placement.length = sizeof(placement);
  if (!::GetWindowPlacement(aWnd, &placement))
    return;
  SaveWindowSize(placement.rcNormalPosition, 
      BOOLToBool(::IsZoomed(aWnd)));
  SavePanelsInfo(g_App.NumPanels, g_App.LastFocusedPanel, 
      g_Splitter.GetPos());
}

void ExecuteCommand(UINT commandID)
{
  switch (commandID)
  {
    case kAddCommand:
      g_App.AddToArchive();
      break;
    case kExtractCommand:
      g_App.ExtractArchives();
      break;
    case kTestCommand:
      g_App.TestArchives();
      break;
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	switch (message) 
	{
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
      if ((HWND) lParam != NULL && wmEvent != 0)
        break;
      if (wmId >= kToolbarStartID)
      {
        ExecuteCommand(wmId);
        return 0;
      }
      if (OnMenuCommand(hWnd, wmId))
        return 0;
      break;
		case WM_INITMENUPOPUP:
      OnMenuActivating(hWnd, HMENU(wParam), LOWORD(lParam));
      break;

    /*
    It doesn't help
    case WM_EXITMENULOOP:
      {
        OnMenuUnActivating(hWnd);
        break;
      }
    case WM_UNINITMENUPOPUP:
      OnMenuUnActivating(hWnd, HMENU(wParam), lParam);
      break;
    */

    case WM_CREATE:
    {

      /*
      INITCOMMONCONTROLSEX icex;
      icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
      icex.dwICC  = ICC_BAR_CLASSES;
      InitCommonControlsEx(&icex);
      
      // Toolbar buttons used to create the first 4 buttons.
      TBBUTTON tbb [ ] = 
      {
        // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
        // {VIEW_PARENTFOLDER, kParentFolderID, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
          // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
        {VIEW_NEWFOLDER, ID_FILE_CREATEFOLDER, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
      };
      
      int baseID = 100;
      NWindows::NControl::CToolBar aToolBar;
      aToolBar.Attach(::CreateToolbarEx (hWnd, 
        WS_CHILD | WS_BORDER | WS_VISIBLE | TBSTYLE_TOOLTIPS, //  | TBSTYLE_FLAT 
        baseID + 2, 11, 
        (HINSTANCE)HINST_COMMCTRL, IDB_VIEW_SMALL_COLOR, 
        (LPCTBBUTTON)&tbb, sizeof(tbb) / sizeof(tbb[0]), 
        0, 0, 100, 30, sizeof (TBBUTTON)));
      */
      // HCURSOR cursor = ::LoadCursor(0, IDC_SIZEWE);
      // ::SetCursor(cursor);

      if (g_PanelsInfoDefined)
        g_Splitter.SetPos(hWnd, g_SplitterPos);
      else
      {
        g_Splitter.SetRatio(hWnd, kSplitterRateMax / 2);
        g_SplitterPos = g_Splitter.GetPos();
      }

      RECT rect;
      ::GetClientRect(hWnd, &rect);
      int xSize = rect.right;
      int xSizes[2];
      xSizes[0] = g_Splitter.GetPos();
      xSizes[1] = xSize - kSplitterWidth - xSizes[0];
      if (xSizes[1] < 0)
        xSizes[1] = 0;

      g_App.CreateDragTarget();
      g_App.Create(hWnd, g_MainPath, xSizes);
      // g_SplitterPos = 0;

      // ::DragAcceptFiles(hWnd, TRUE);
      RegisterDragDrop(hWnd, g_App._dropTarget);

      break;
    }
		case WM_DESTROY:
    {
      // ::DragAcceptFiles(hWnd, FALSE);  
      RevokeDragDrop(hWnd);
      g_App._dropTarget.Release();

      g_App.Save();
      g_App.Release();
      SaveWindowInfo(hWnd);
			PostQuitMessage(0);
      break;
    }
    /*
    case WM_MOVE:
    {
			break;
    }
    */
    case WM_LBUTTONDOWN:
      g_StartCaptureMousePos = LOWORD(lParam);
      g_StartCaptureSplitterPos = g_Splitter.GetPos();
      ::SetCapture(hWnd);
			break;
    case WM_LBUTTONUP:
    {
      ::ReleaseCapture();
      break;
    }
    case WM_MOUSEMOVE: 
    {
      if ((wParam & MK_LBUTTON) != 0 && ::GetCapture() == hWnd)
      {
        g_Splitter.SetPos(hWnd, g_StartCaptureSplitterPos + 
            (short)LOWORD(lParam) - g_StartCaptureMousePos);
        MoveSubWindows(hWnd);
      }
			break;
    }

    case WM_SIZE:
    {
      if (g_CanChangeSplitter)
        g_Splitter.SetPosFromRatio(hWnd);
      else
      {
        g_Splitter.SetPos(hWnd, g_SplitterPos );
        g_CanChangeSplitter = true;
      }

      OnSize(hWnd);
      /*
      int xSize = LOWORD(lParam);
      int ySize = HIWORD(lParam);
      // int xSplitter = 2;
      int xWidth = g_SplitPos;
      // int xSplitPos = xWidth;
      g_Panel[0]._listView.MoveWindow(0, 0, xWidth, ySize);
      g_Panel[1]._listView.MoveWindow(xSize - xWidth, 0, xWidth, ySize);
      */
      return 0;
      break;
    }
    case WM_SETFOCUS:
      // g_App.SetFocus(g_App.LastFocusedPanel);
      g_App.SetFocusToLastItem();
      break;
    /*
    case WM_ACTIVATE:
    {
      int fActive = LOWORD(wParam); 
      switch (fActive)
      {
        case WA_INACTIVE:
        {
          // g_FocusIndex = g_App.LastFocusedPanel;
          // g_App.LastFocusedPanel = g_App.GetFocusedPanelIndex();
          // return 0;
        }
      }
      break;
    }
    */
    /*
    case kLangWasChangedMessage:
      MyLoadMenu(g_HWND);
      return 0;
    */
      
		/*
    case WM_SETTINGCHANGE:
      break;
    */
    case WM_NOTIFY:
    {
      g_App.OnNotify(wParam, (LPNMHDR)lParam);
      break;
    }
    /*
    case WM_DROPFILES:
    {
      g_App.GetFocusedPanel().CompressDropFiles((HDROP)wParam);
      return 0 ;
    }
    */
   }
	 return DefWindowProc(hWnd, message, wParam, lParam);
}

void OnSize(HWND hWnd)
{
  if (g_App._rebar)
  {
    RECT rect;
    ::GetClientRect(hWnd, &rect);
    int xSize = rect.right;
    int ySize = rect.bottom;
    // rect.bottom = 0;
    // g_App._rebar.SizeToRect(&rect);
    // g_App._rebar.Move(0, 0, xSize, ySize);
  }
  MoveSubWindows(hWnd);
}

void MoveSubWindows(HWND hWnd)
{
  RECT rect;
  ::GetClientRect(hWnd, &rect);
  int xSize = rect.right;
  int kHeaderSize = 0;
  int ySize = MyMax(int(rect.bottom - kHeaderSize), 0);
  if (g_App._rebar)
  {
    RECT barRect;
    g_App._rebar.GetWindowRect(&barRect);
    kHeaderSize = barRect.bottom - barRect.top;
    ySize = MyMax(int(rect.bottom - kHeaderSize), 0);
  }
 
  // g_App._headerToolBar.Move(0, 2, xSize, kHeaderSize - 2);
  RECT rect2 = rect;
  rect2.bottom = 0;
  // g_App._headerReBar.SizeToRect(&rect2);
  if (g_App.NumPanels > 1)
  {
    g_App.Panels[0].Move(0, kHeaderSize, g_Splitter.GetPos(), ySize);
    int xWidth1 = g_Splitter.GetPos() + kSplitterWidth;
    g_App.Panels[1].Move(xWidth1, kHeaderSize, xSize - xWidth1, ySize);
  }
  else
  {
    /*
    int otherPanel = 1 - g_App.LastFocusedPanel;
    if (g_App.PanelsCreated[otherPanel])
      g_App.Panels[otherPanel].Move(0, kHeaderSize, 0, ySize);
    */
    g_App.Panels[g_App.LastFocusedPanel].Move(0, kHeaderSize, xSize, ySize);
  }
}
