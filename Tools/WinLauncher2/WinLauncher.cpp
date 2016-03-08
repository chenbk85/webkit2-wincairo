/*
 * Copyright (C) 2006, 2008, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2011 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Appcelerator, Inc. All rights reserved.
 * Copyright (C) 2013 Alex Christensen. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "stdafx.h"
#include "WinLauncher.h"

#include "AccessibilityDelegate.h"
#include "DOMDefaultImpl.h"
#include "PrintWebUIDelegate.h"
#include "WinLauncherLibResource.h"
#include "WinLauncherReplace.h"
#include <WebKit/WKArray.h>
#include <WebKit/WKBackForwardListRef.h>
#include <WebKit/WKBackForwardListItemRef.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKFrame.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKString.h>
#include <WebKit/WKStringWin.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewWin.h>
#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#endif

#if USE(GLIB)
#include <glib.h>
#endif

#include <dbghelp.h>
#include <functional>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <string>
#include <vector>

#include "HostWindow.h"

#define MAX_LOADSTRING 100
#define URLBAR_HEIGHT  24
#define CONTROLBUTTON_WIDTH 24

static const int maxHistorySize = 10;

//typedef _com_ptr_t<_com_IIID<IWebFrame, &__uuidof(IWebFrame)>> IWebFramePtr;
//typedef _com_ptr_t<_com_IIID<IWebInspector, &__uuidof(IWebInspector)>> IWebInspectorPtr;
//typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;
//typedef _com_ptr_t<_com_IIID<IWebCache, &__uuidof(IWebCache)>> IWebCachePtr;

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hMainWnd;
HWND hURLBarWnd;
HWND hBackButtonWnd;
HWND hForwardButtonWnd;
WNDPROC DefEditProc = 0;
WNDPROC DefButtonProc = 0;
WNDPROC DefWebKitProc = 0;
//IWebInspectorPtr gInspector;
WKContextRef gWKContext = 0;
WKPageGroupRef gWKPageGroup = 0;
WKPreferencesRef gWKPreferences = 0;
WKViewRef gWKView = 0;
WKPageRef gWKPage = 0;
WKFrameRef gWKFrame = 0;
HWND gViewWindow = 0;
PrintWebUIDelegate* gPrintDelegate = 0;
AccessibilityDelegate* gAccessibilityDelegate = 0;
std::vector<WKBackForwardListItemRef> gHistoryItems;
TCHAR szTitle[MAX_LOADSTRING];                    // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Support moving the transparent window
POINT s_windowPosition = { 100, 100 };
SIZE s_windowSize = { 800, 400 };
bool s_usesLayeredWebView = false;
bool s_fullDesktop = false;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    EditProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    BackButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ForwardButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ReloadButtonProc(HWND, UINT, WPARAM, LPARAM);

static void loadURL(BSTR urlBStr);

static bool usesLayeredWebView()
{
    return s_usesLayeredWebView;
}

static bool shouldUseFullDesktop()
{
    return s_fullDesktop;
}

class SimpleEventListener : public DOMEventListener {
public:
    SimpleEventListener(LPWSTR type)
    {
        wcsncpy_s(m_eventType, 100, type, 100);
        m_eventType[99] = 0;
    }

    virtual HRESULT STDMETHODCALLTYPE handleEvent(IDOMEvent* evt)
    {
        wchar_t message[255];
        wcscpy_s(message, 255, m_eventType);
        wcscat_s(message, 255, L" event fired!");
        ::MessageBox(0, message, L"Event Handler", MB_OK);
        return S_OK;
    }

private:
    wchar_t m_eventType[100];
};

static const wchar_t emptyString[] = L"";

static _bstr_t convertWKStringToBSTR(WKStringRef string)
{
    size_t stringLength = WKStringGetLength(string);
    if (stringLength == 0)
        return emptyString;

    std::vector<wchar_t> stringVector(stringLength + 1);
    WKStringGetCharacters(string, stringVector.data(), stringLength);

    return _bstr_t(stringVector.data());
}

static void updateMenuItemForHistoryItem(HMENU menu, WKBackForwardListItemRef historyItem, int currentHistoryItem, bool checked = false)
{
    UINT menuID = IDM_HISTORY_LINK0 + currentHistoryItem;

    MENUITEMINFO menuItemInfo = {0};
    menuItemInfo.cbSize = sizeof(MENUITEMINFO);
    menuItemInfo.fMask = MIIM_TYPE;
    menuItemInfo.fType = MFT_STRING;

    WKRetainPtr<WKStringRef> itemTitle = WKBackForwardListItemCopyTitle(historyItem);
    _bstr_t title = convertWKStringToBSTR(itemTitle.get());
    menuItemInfo.dwTypeData = static_cast<LPWSTR>(title);

    ::SetMenuItemInfo(menu, menuID, FALSE, &menuItemInfo);
    ::EnableMenuItem(menu, menuID, MF_BYCOMMAND | MF_ENABLED);
    ::CheckMenuItem(menu, menuID, (checked) ? MF_CHECKED : MF_UNCHECKED);
}

static void updateMenuHistoryLinks(WKPageRef page)
{
    HMENU menu = ::GetMenu(hMainWnd);

    WKBackForwardListRef backForwardList = WKPageGetBackForwardList(page);
    if (!backForwardList)
        return;

    int backCount = WKBackForwardListGetBackListCount(backForwardList);

    UINT backSetting = MF_BYCOMMAND | (backCount) ? MF_ENABLED : MF_DISABLED;
    ::EnableMenuItem(menu, IDM_HISTORY_BACKWARD, backSetting);

    int forwardCount = WKBackForwardListGetForwardListCount(backForwardList);

    UINT forwardSetting = MF_BYCOMMAND | (forwardCount) ? MF_ENABLED : MF_DISABLED;
    ::EnableMenuItem(menu, IDM_HISTORY_FORWARD, forwardSetting);

    WKBackForwardListItemRef currentItem = WKBackForwardListGetCurrentItem(backForwardList);
    if (!currentItem)
        return;

    static const int maxHistory = 10;

    WKRetainPtr<WKArrayRef> backList = WKBackForwardListCopyBackListWithLimit(backForwardList, std::max(backCount, maxHistory - 1));
    WKRetainPtr<WKArrayRef> forwardList = WKBackForwardListCopyForwardListWithLimit(backForwardList, std::max(forwardCount, maxHistory - 1));

    int backListFirst = -WKArrayGetSize(backList.get());
    int forwardListLast = WKArrayGetSize(forwardList.get());
    int backForwardListMedian = (forwardListLast - backListFirst) / 2;
    std::pair<int, int> backForwardListRange(std::max(backListFirst, backForwardListMedian - maxHistory / 2), std::min(forwardListLast, backForwardListMedian + maxHistory / 2));

    gHistoryItems.clear();

    // Add forward list items.
    int currentHistoryItem = 0;
    WKBackForwardListItemRef item = 0;
    for (int forwardListItem = backForwardListRange.second - 1; forwardListItem >= 0; --forwardListItem) {
        item = (WKBackForwardListItemRef)WKArrayGetItemAtIndex(forwardList.get(), forwardListItem);
        updateMenuItemForHistoryItem(menu, item, currentHistoryItem);
        gHistoryItems.push_back(item);
        ++currentHistoryItem;
    }

    // Add current list item.
    updateMenuItemForHistoryItem(menu, currentItem, currentHistoryItem, true);
    gHistoryItems.push_back(WKBackForwardListGetCurrentItem(WKPageGetBackForwardList(gWKPage)));
    ++currentHistoryItem;

    // Add back list items.
    for (int backListItem = -backForwardListRange.first - 1; backListItem >= 0; --backListItem) {
        item = (WKBackForwardListItemRef)WKArrayGetItemAtIndex(backList.get(), backListItem);
        updateMenuItemForHistoryItem(menu, item, currentHistoryItem);
        gHistoryItems.push_back(item);
        ++currentHistoryItem;
    }

    // Hide any history we aren't using yet.
    for (int i = currentHistoryItem; i < maxHistorySize; ++i)
        ::EnableMenuItem(menu, IDM_HISTORY_LINK0 + i, MF_BYCOMMAND | MF_DISABLED);
}

static void resizeSubViews()
{
    if (usesLayeredWebView() || !gViewWindow)
        return;

    RECT rcClient;
    GetClientRect(hMainWnd, &rcClient);
    MoveWindow(hBackButtonWnd, 0, 0, CONTROLBUTTON_WIDTH, URLBAR_HEIGHT, TRUE);
    MoveWindow(hForwardButtonWnd, CONTROLBUTTON_WIDTH, 0, CONTROLBUTTON_WIDTH, URLBAR_HEIGHT, TRUE);
    MoveWindow(hURLBarWnd, CONTROLBUTTON_WIDTH * 2, 0, rcClient.right, URLBAR_HEIGHT, TRUE);
    MoveWindow(gViewWindow, 0, URLBAR_HEIGHT, rcClient.right, rcClient.bottom - URLBAR_HEIGHT, TRUE);
}

static void subclassForLayeredWindow()
{
    hMainWnd = gViewWindow;
#if defined _M_AMD64 || defined _WIN64
    DefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hMainWnd, GWLP_WNDPROC));
    ::SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
#else
    DefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLong(hMainWnd, GWL_WNDPROC));
    ::SetWindowLong(hMainWnd, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
#endif
}

static void computeFullDesktopFrame()
{
    RECT desktop;
    if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, static_cast<void*>(&desktop), 0))
        return;

    s_windowPosition.x = 0;
    s_windowPosition.y = 0;
    s_windowSize.cx = desktop.right - desktop.left;
    s_windowSize.cy = desktop.bottom - desktop.top;
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        hInst = dllInstance;

    return TRUE;
}

static bool getAppDataFolder(_bstr_t& directory)
{
    wchar_t appDataDirectory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory)))
        return false;

    wchar_t executablePath[MAX_PATH];
    ::GetModuleFileNameW(0, executablePath, MAX_PATH);
    ::PathRemoveExtensionW(executablePath);

    directory = _bstr_t(appDataDirectory) + L"\\" + ::PathFindFileNameW(executablePath);

    return true;
}

static bool setToDefaultPreferences()
{
#if USE(CG)
    WKPreferencesSetAVFoundationEnabled(gWKPreferences, true);
    WKPreferencesSetAcceleratedCompositingEnabled(gWKPreferences, true);
#endif

    WKPreferencesSetFullScreenEnabled(gWKPreferences, true);
    WKPreferencesSetCompositingBordersVisible(gWKPreferences, false);
    WKPreferencesSetCompositingRepaintCountersVisible(gWKPreferences, false);

    WKPreferencesSetLoadsImagesAutomatically(gWKPreferences, true);
    WKPreferencesSetAuthorAndUserStylesEnabled(gWKPreferences, true);
    WKPreferencesSetJavaScriptEnabled(gWKPreferences, true);
    WKPreferencesSetUniversalAccessFromFileURLsAllowed(gWKPreferences, false);
    WKPreferencesSetFileAccessFromFileURLsAllowed(gWKPreferences, true);

    WKPreferencesSetDeveloperExtrasEnabled(gWKPreferences, true);

    return true;
}

static bool setCacheFolder()
{
    //IWebCachePtr webCache;

    //HRESULT hr = WebKitCreateInstance(CLSID_WebCache, 0, __uuidof(webCache), reinterpret_cast<void**>(&webCache.GetInterfacePtr()));
    //if (FAILED(hr))
    //    return false;

    //_bstr_t appDataFolder;
    //if (!getAppDataFolder(appDataFolder))
        return false;

    //appDataFolder += L"\\cache";
    //webCache->setCacheFolder(appDataFolder);

    //return true;
}

void createCrashReport(EXCEPTION_POINTERS* exceptionPointers)
{
    _bstr_t directory;

    if (!getAppDataFolder(directory))
        return;

    if (::SHCreateDirectoryEx(0, directory, 0) != ERROR_SUCCESS
        && ::GetLastError() != ERROR_FILE_EXISTS
        && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return;

    std::wstring fileName = directory + L"\\CrashReport.dmp";
    HANDLE miniDumpFile = ::CreateFile(fileName.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (miniDumpFile && miniDumpFile != INVALID_HANDLE_VALUE) {

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = ::GetCurrentThreadId();
        mdei.ExceptionPointers  = exceptionPointers;
        mdei.ClientPointers = 0;

#ifdef _DEBUG
        MINIDUMP_TYPE dumpType = MiniDumpWithFullMemory;
#else
        MINIDUMP_TYPE dumpType = MiniDumpNormal;
#endif

        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), miniDumpFile, dumpType, &mdei, 0, 0);
        ::CloseHandle(miniDumpFile);
        processCrashReport(fileName.c_str());
    }
}

_bstr_t requestedURL;
_bstr_t mainFrameURL;

static void setURLBarText(BSTR& bstr)
{
    ::SendMessage(hURLBarWnd, static_cast<UINT>(WM_SETTEXT), 0, reinterpret_cast<LPARAM>(bstr));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif

     // TODO: Place code here.
    MSG msg = {0};
    HACCEL hAccelTable;

    INITCOMMONCONTROLSEX InitCtrlEx;

    InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitCtrlEx.dwICC  = 0x00004000; //ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&InitCtrlEx);

    int argc = 0;
    WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        if (!wcsicmp(argv[i], L"--transparent"))
            s_usesLayeredWebView = true;
        else if (!wcsicmp(argv[i], L"--desktop"))
            s_fullDesktop = true;
        else if (!requestedURL)
            requestedURL = argv[i];
    }

    // Initialize global strings
    LoadString(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInst, IDC_WINLAUNCHER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInst);

    if (shouldUseFullDesktop())
        computeFullDesktopFrame();

    // Init COM
    OleInitialize(NULL);

    if (usesLayeredWebView()) {
        hURLBarWnd = CreateWindow(L"EDIT", L"Type URL Here",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, 
                    s_windowPosition.x, s_windowPosition.y + s_windowSize.cy, s_windowSize.cx, URLBAR_HEIGHT,
                    0,
                    0,
                    hInst, 0);
    } else {
        hMainWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInst, 0);

        if (!hMainWnd)
            return FALSE;

        hBackButtonWnd = CreateWindow(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE  | BS_TEXT, 0, 0, 0, 0, hMainWnd, 0, hInst, 0);
        hForwardButtonWnd = CreateWindow(L"BUTTON", L">", WS_CHILD | WS_VISIBLE  | BS_TEXT, CONTROLBUTTON_WIDTH, 0, 0, 0, hMainWnd, 0, hInst, 0);
        hURLBarWnd = CreateWindow(L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, CONTROLBUTTON_WIDTH * 2, 0, 0, 0, hMainWnd, 0, hInst, 0);

        ShowWindow(hMainWnd, nCmdShow);
        UpdateWindow(hMainWnd);
    }

    DefEditProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hURLBarWnd, GWLP_WNDPROC));
    DefButtonProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hBackButtonWnd, GWLP_WNDPROC));
    SetWindowLongPtr(hURLBarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
    SetWindowLongPtr(hBackButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BackButtonProc));
    SetWindowLongPtr(hForwardButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ForwardButtonProc));

    SetFocus(hURLBarWnd);

    RECT clientRect = { s_windowPosition.x, s_windowPosition.y, s_windowPosition.x + s_windowSize.cx, s_windowPosition.y + s_windowSize.cy };

    WKRetainPtr<WKStringRef> defaultPageGroupIdentifier = adoptWK(WKStringCreateWithUTF8CString("defaultPageGroup"));

    gWKContext = WKContextCreate();
    if (!gWKContext)
        goto exit;

    WKContextConnectionClientV0 connectionClient;
    memset(&connectionClient, 0, sizeof(WKContextConnectionClientV0));

    connectionClient.base.version = 0;
    connectionClient.base.clientInfo = 0;
    connectionClient.didCreateConnection = [] (WKContextRef context, WKConnectionRef connection, const void* clientInfo) {
        if (!requestedURL) {
            WKRetainPtr<WKStringRef> htmlString = WKStringCreateWithWCharString(defaultHTML);
            WKPageLoadHTMLString(gWKPage, htmlString.get(), 0);
        }
    };

    WKContextSetConnectionClient(gWKContext, &connectionClient.base);

    WKContextHistoryClientV0 historyClient;
    memset(&historyClient, 0, sizeof(WKContextHistoryClientV0));

    historyClient.base.version = 0;
    historyClient.base.clientInfo = 0;
    historyClient.didUpdateHistoryTitle = [] (WKContextRef context, WKPageRef page, WKStringRef title, WKURLRef URL, WKFrameRef frame, const void *clientInfo) {
        updateMenuHistoryLinks(page);
    };

    WKContextSetHistoryClient(gWKContext, &historyClient.base);

    gWKPageGroup = WKPageGroupCreateWithIdentifier(defaultPageGroupIdentifier.get());
    if (!gWKPageGroup)
        goto exit;

    gWKPreferences = WKPreferencesCreateWithIdentifier(defaultPageGroupIdentifier.get());
    if (!gWKPreferences)
        goto exit;

    WKPageGroupSetPreferences(gWKPageGroup, gWKPreferences);

    if (!setToDefaultPreferences())
        goto exit;

    gWKView = WKViewCreate(gWKContext, gWKPageGroup);
    if (!gWKView)
        goto exit;

    WKViewInitialize(gWKView);

    gWKPage = WKViewGetPage(gWKView);
    if (!gWKPage)
        goto exit;

    //if (!setCacheFolder())
    //    goto exit;

    WKPageLoaderClientV4 pageLoaderClient;
    memset(&pageLoaderClient, 0, sizeof(WKPageLoaderClientV4));

    pageLoaderClient.base.version = 4;
    pageLoaderClient.base.clientInfo = 0;
    pageLoaderClient.didStartProvisionalLoadForFrame = [] (WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo) {
        WKFrameRef mainFrame = WKPageGetMainFrame(page);
        if (frame != mainFrame)
            return;

        mainFrameURL = emptyString;

        WKRetainPtr<WKURLRef> frameURL = WKFrameCopyProvisionalURL(frame);
        if (!frameURL) {
            setURLBarText(mainFrameURL.GetBSTR());
            return;
        }

        WKRetainPtr<WKStringRef> url = WKURLCopyString(frameURL.get());
        mainFrameURL = convertWKStringToBSTR(url.get());
        setURLBarText(mainFrameURL.GetBSTR());
    };
    pageLoaderClient.didFailProvisionalLoadWithErrorForFrame = [] (WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void *clientInfo) {
        WKFrameRef mainFrame = WKPageGetMainFrame(page);
        if (frame != mainFrame)
            return;

        _bstr_t errorDescription;
        WKRetainPtr<WKStringRef> localizedDescription = WKErrorCopyLocalizedDescription(error);
        if (!localizedDescription)
            errorDescription = L"Failed to load page and to localize error description.";
        else {
            errorDescription = convertWKStringToBSTR(localizedDescription.get());
        }

        if (_wcsicmp(errorDescription, L"Cancelled"))
            ::MessageBoxW(0, static_cast<LPCWSTR>(errorDescription), L"Error", MB_APPLMODAL | MB_OK);
    };
    pageLoaderClient.didFinishLoadForFrame = [] (WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo) {
        WKFrameRef mainFrame = WKPageGetMainFrame(page);
        if (frame != mainFrame)
            return;

        // TODO: Put onclick DOM event listener on webkit logo.
    };
    pageLoaderClient.didChangeBackForwardList = [] (WKPageRef page, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void *clientInfo) {
        updateMenuHistoryLinks(page);
    };

    WKPageSetPageLoaderClient(gWKPage, &pageLoaderClient.base);

    //gPrintDelegate = new PrintWebUIDelegate;
    //gPrintDelegate->AddRef();
    //hr = gWebView->setUIDelegate(gPrintDelegate);
    //if (FAILED (hr))
    //    goto exit;

    //gAccessibilityDelegate = new AccessibilityDelegate;
    //gAccessibilityDelegate->AddRef();
    //hr = gWebView->setAccessibilityDelegate(gAccessibilityDelegate);
    //if (FAILED (hr))
    //    goto exit;

    HostWindowExtra windowExtra;
    windowExtra.webView = gWKView;
    windowExtra.isBeingDestroyed = false;

    gViewWindow = createViewHostWindow(hInst, hMainWnd, &windowExtra);

    WKViewClientV0 viewClient;
    memset(&viewClient, 0, sizeof(WKViewClientV0));

    viewClient.base.version = 0;
    viewClient.base.clientInfo = 0;
    viewClient.viewNeedsDisplay = [] (WKViewRef view, WKRect area, const void* clientInfo) {
        ::InvalidateRect(gViewWindow, NULL, FALSE);
    };

    WKViewSetViewClient(gWKView, &viewClient.base);

    //hr = gWebViewPrivate->setTransparent(usesLayeredWebView());
    //if (FAILED(hr))
    //    goto exit;

    //hr = gWebViewPrivate->setUsesLayeredWindow(usesLayeredWebView());
    //if (FAILED(hr))
    //    goto exit;

    //hr = gWebViewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&gViewWindow));
    //if (FAILED(hr) || !gViewWindow)
    //    goto exit;

    if (usesLayeredWebView())
        subclassForLayeredWindow();

    resizeSubViews();

    ShowWindow(gViewWindow, nCmdShow);
    UpdateWindow(gViewWindow);

    hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_WINLAUNCHER));

    if (requestedURL.length())
        loadURL(requestedURL.GetBSTR());

#pragma warning(disable:4509)

    // Main message loop:
    __try {
        while (GetMessage(&msg, 0, 0, 0)) {
#if USE(CF)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
#if USE(GLIB)
            g_main_context_iteration(0, false);
#endif
        }
    } __except(createCrashReport(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) { }

exit:
    gPrintDelegate->Release();

    //shutDownWebKit();
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    // Shut down COM.
    OleUninitialize();
    
    return static_cast<int>(msg.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINLAUNCHER));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_WINLAUNCHER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

static BOOL CALLBACK AbortProc(HDC hDC, int Error)
{
    MSG msg;
    while (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return TRUE;
}

static HDC getPrinterDC()
{
    PRINTDLG pdlg;
    memset(&pdlg, 0, sizeof(PRINTDLG));
    pdlg.lStructSize = sizeof(PRINTDLG);
    pdlg.Flags = PD_PRINTSETUP | PD_RETURNDC;

    ::PrintDlg(&pdlg);

    return pdlg.hDC;
}

static void initDocStruct(DOCINFO* di, TCHAR* docname)
{
    memset(di, 0, sizeof(DOCINFO));
    di->cbSize = sizeof(DOCINFO);
    di->lpszDocName = docname;
}

//typedef _com_ptr_t<_com_IIID<IWebFramePrivate, &__uuidof(IWebFramePrivate)>> IWebFramePrivatePtr;

void PrintView(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC printDC = getPrinterDC();
    if (!printDC) {
        ::MessageBoxW(0, L"Error creating printing DC", L"Error", MB_APPLMODAL | MB_OK);
        return;
    }

    if (::SetAbortProc(printDC, AbortProc) == SP_ERROR) {
        ::MessageBoxW(0, L"Error setting up AbortProc", L"Error", MB_APPLMODAL | MB_OK);
        return;
    }

    //IWebFramePtr frame;
    //IWebFramePrivatePtr framePrivate;
    //if (FAILED(gWebView->mainFrame(&frame.GetInterfacePtr())))
    //    return;

    //if (FAILED(frame->QueryInterface(&framePrivate.GetInterfacePtr())))
    //    return;

    //framePrivate->setInPrintingMode(TRUE, printDC);

    //UINT pageCount = 0;
    //framePrivate->getPrintedPageCount(printDC, &pageCount);

    //DOCINFO di;
    //initDocStruct(&di, L"WebKit Doc");
    //::StartDoc(printDC, &di);

    //// FIXME: Need CoreGraphics implementation
    //void* graphicsContext = 0;
    //for (size_t page = 1; page <= pageCount; ++page) {
    //    ::StartPage(printDC);
    //    framePrivate->spoolPages(printDC, page, page, graphicsContext);
    //    ::EndPage(printDC);
    //}

    //framePrivate->setInPrintingMode(FALSE, printDC);

    ::EndDoc(printDC);
    ::DeleteDC(printDC);
}

static void ToggleMenuItem(HWND hWnd, UINT menuID)
{
    HMENU menu = ::GetMenu(hWnd);

    MENUITEMINFO info;
    ::memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    if (!::GetMenuItemInfo(menu, menuID, FALSE, &info))
        return;

    BOOL newState = !(info.fState & MFS_CHECKED);

    //if (!gStandardPreferences || !gPrefsPrivate)
    //    return;

    //switch (menuID) {
    //case IDM_AVFOUNDATION:
    //    gStandardPreferences->setAVFoundationEnabled(newState);
    //    break;
    //case IDM_ACC_COMPOSITING:
    //    gPrefsPrivate->setAcceleratedCompositingEnabled(newState);
    //    break;
    //case IDM_WK_FULLSCREEN:
    //    gPrefsPrivate->setFullScreenEnabled(newState);
    //    break;
    //case IDM_COMPOSITING_BORDERS:
    //    gPrefsPrivate->setShowDebugBorders(newState);
    //    gPrefsPrivate->setShowRepaintCounter(newState);
    //    break;
    //case IDM_DISABLE_IMAGES:
    //    gStandardPreferences->setLoadsImagesAutomatically(!newState);
    //    break;
    //case IDM_DISABLE_STYLES:
    //    gPrefsPrivate->setAuthorAndUserStylesEnabled(!newState);
    //    break;
    //case IDM_DISABLE_JAVASCRIPT:
    //    gStandardPreferences->setJavaScriptEnabled(!newState);
    //    break;
    //case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
    //    gPrefsPrivate->setAllowUniversalAccessFromFileURLs(newState);
    //    gPrefsPrivate->setAllowFileAccessFromFileURLs(newState);
    //    break;
    //}

    info.fState = (newState) ? MFS_CHECKED : MFS_UNCHECKED;

    ::SetMenuItemInfo(menu, menuID, FALSE, &info);
}

static void LaunchInspector(HWND hwnd)
{
    //if (!gWebViewPrivate)
    //    return;

    //if (!SUCCEEDED(gWebViewPrivate->inspector(&gInspector.GetInterfacePtr())))
    //    return;

    //gInspector->show();
}

static void NavigateForwardOrBackward(HWND hWnd, UINT menuID)
{
    if (!gWKView)
        return;

    if (IDM_HISTORY_FORWARD == menuID)
        WKPageGoForward(gWKPage);
    else
        WKPageGoBack(gWKPage);
}

static void NavigateToHistory(HWND hWnd, UINT menuID)
{
    if (!gWKView)
        return;

    int historyEntry = menuID - IDM_HISTORY_LINK0;
    if (historyEntry > gHistoryItems.size())
        return;

    WKBackForwardListItemRef desiredHistoryItem = gHistoryItems[historyEntry];
    if (!desiredHistoryItem)
        return;

    WKPageGoToBackForwardListItem(gWKPage, desiredHistoryItem);
}

static const int dragBarHeight = 30;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WNDPROC parentProc = usesLayeredWebView() ? DefWebKitProc : DefWindowProc;

    switch (message) {
    case WM_NCHITTEST:
        if (usesLayeredWebView()) {
            RECT window;
            ::GetWindowRect(hWnd, &window);
            // For testing our transparent window, we need a region to use as a handle for
            // dragging. The right way to do this would be to query the web view to see what's
            // under the mouse. However, for testing purposes we just use an arbitrary
            // 30 pixel band at the top of the view as an arbitrary gripping location.
            //
            // When we are within this bad, return HT_CAPTION to tell Windows we want to
            // treat this region as if it were the title bar on a normal window.
            int y = HIWORD(lParam);

            if ((y > window.top) && (y < window.top + dragBarHeight))
                return HTCAPTION;
        }
        return CallWindowProc(parentProc, hWnd, message, wParam, lParam);
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmId >= IDM_HISTORY_LINK0 && wmId <= IDM_HISTORY_LINK9) {
            NavigateToHistory(hWnd, wmId);
            break;
        }
        // Parse the menu selections:
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDM_PRINT:
            PrintView(hWnd, message, wParam, lParam);
            break;
        case IDM_WEB_INSPECTOR:
            LaunchInspector(hWnd);
            break;
        case IDM_HISTORY_BACKWARD:
        case IDM_HISTORY_FORWARD:
            NavigateForwardOrBackward(hWnd, wmId);
            break;
        case IDM_AVFOUNDATION:
        case IDM_ACC_COMPOSITING:
        case IDM_WK_FULLSCREEN:
        case IDM_COMPOSITING_BORDERS:
        case IDM_DISABLE_IMAGES:
        case IDM_DISABLE_STYLES:
        case IDM_DISABLE_JAVASCRIPT:
        case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
            ToggleMenuItem(hWnd, wmId);
            break;
        default:
            return CallWindowProc(parentProc, hWnd, message, wParam, lParam);
        }
        }
        break;
    case WM_DESTROY:
#if USE(CF)
        CFRunLoopStop(CFRunLoopGetMain());
#endif
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        //if (!gWebView || usesLayeredWebView())
        //   return CallWindowProc(parentProc, hWnd, message, wParam, lParam);

        resizeSubViews();
        break;
    default:
        return CallWindowProc(parentProc, hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK EditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CHAR:
        if (wParam == 13) { // Enter Key
            wchar_t strPtr[INTERNET_MAX_URL_LENGTH];
            *((LPWORD)strPtr) = INTERNET_MAX_URL_LENGTH; 
            int strLen = SendMessage(hDlg, EM_GETLINE, 0, (LPARAM)strPtr);

            strPtr[strLen] = 0;
            _bstr_t bstr(strPtr);
            loadURL(bstr.GetBSTR());

            return 0;
        } 
    default:
        return CallWindowProc(DefEditProc, hDlg, message, wParam, lParam);
    }
}

LRESULT CALLBACK BackButtonProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    BOOL wentBack = FALSE;
    switch (message) {
    case WM_LBUTTONUP:
        WKPageGoBack(gWKPage);
    default:
        return CallWindowProc(DefButtonProc, hDlg, message, wParam, lParam);
    }
}

LRESULT CALLBACK ForwardButtonProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    BOOL wentForward = FALSE;
    switch (message) {
    case WM_LBUTTONUP:
        WKPageGoForward(gWKPage);
    default:
        return CallWindowProc(DefButtonProc, hDlg, message, wParam, lParam);
    }
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

static void loadURL(BSTR passedURL)
{
    _bstr_t urlBStr(passedURL);
    if (!!urlBStr && (::PathFileExists(urlBStr) || ::PathIsUNC(urlBStr))) {
        TCHAR fileURL[INTERNET_MAX_URL_LENGTH];
        DWORD fileURLLength = sizeof(fileURL)/sizeof(fileURL[0]);

        if (SUCCEEDED(::UrlCreateFromPath(urlBStr, fileURL, &fileURLLength, 0)))
            urlBStr = fileURL;
    }

    WKRetainPtr<WKStringRef> address = WKStringCreateWithWCharString(urlBStr.GetBSTR());
    size_t addressLength = WKStringGetMaximumUTF8CStringSize(address.get());
    std::vector<char> utf8Address(addressLength);
    WKStringGetUTF8CString(address.get(), utf8Address.data(), addressLength);

    WKRetainPtr<WKURLRef> url = WKURLCreateWithUTF8CString(utf8Address.data());
    WKPageLoadURL(gWKPage, url.get());

    SetFocus(gViewWindow);
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpstrCmdLine, int nCmdShow)
{
    return wWinMain(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
