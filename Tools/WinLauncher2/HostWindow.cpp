/*
 * Copyright (C) 2014 Daewoong Jang (daewoong.jang@navercorp.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include "HostWindow.h"

#include <WebKit/WKFrame.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKString.h>
#include <WebKit/WKStringWin.h>
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
#include <cairo/cairo-win32.h>

#define ASSERT assert

// Value used in WebViewWndProc for Gestures
#define WM_GESTURE 0x0119
#define WM_GESTURENOTIFY 0x011A

LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

const LPCWSTR kWebViewWindowClassName = L"WebViewHostWindowClass";

const int WM_XP_THEMECHANGED = 0x031A;
const int WM_VISTA_MOUSEHWHEEL = 0x020E;

bool registerWebViewWindowClass(HINSTANCE hInstance)
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    haveRegisteredWindowClass = true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = WebViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(HostWindowExtra*);
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = ::LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebViewWindowClassName;
    wcex.hIconSm        = 0;

    return !!RegisterClassEx(&wcex);
}

HWND createViewHostWindow(HINSTANCE hInstance, HWND hMainWnd, HostWindowExtra* extra)
{
    if (!registerWebViewWindowClass(hInstance))
        return NULL;

    RECT frameRect;
    GetClientRect(hMainWnd, &frameRect);

    HWND hHostWnd = CreateWindowEx(0, kWebViewWindowClassName, 0, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, hMainWnd, 0, hInstance, 0);
    if (!hHostWnd)
        return NULL;

    ASSERT(::IsWindow(hHostWnd));

    SetWindowLongPtr(hHostWnd, 0, (LONG_PTR)extra);
    SetParent(hHostWnd, hMainWnd);

    return hHostWnd;
}

static MSG toMSG(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG msg = { hWnd, message, wParam, lParam, ::GetTickCount() };
    return msg;
}

LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    HostWindowExtra* windowExtra = reinterpret_cast<HostWindowExtra*>(longPtr);
    if (!windowExtra)
        return DefWindowProc(hWnd, message, wParam, lParam);

    // We shouldn't be trying to handle any window messages after WM_DESTROY.
    // See <http://webkit.org/b/55054>.
    ASSERT(!windowExtra->isBeingDestroyed);

    // hold a ref, since the WebView could go away in an event handler.
    WKRetainPtr<WKViewRef> protector(windowExtra->webView);
    ASSERT(windowExtra->webView);

    WKViewRef webView = protector.get();

    // Windows Media Player has a modal message loop that will deliver messages
    // to us at inappropriate times and we will crash if we handle them when
    // they are delivered. We repost paint messages so that we eventually get
    // a chance to paint once the modal loop has exited, but other messages
    // aren't safe to repost, so we just drop them.
    //if (PluginView::isCallingPlugin()) {
    //    if (message == WM_PAINT)
    //        PostMessage(hWnd, message, wParam, lParam);
    //    return 0;
    //}

    bool handled = true;

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hWnd, &paint);
        if (hdc) {
            cairo_surface_t *surface = cairo_win32_surface_create(hdc);
            WKViewPaintToCairoSurface(webView, surface);
            cairo_surface_finish(surface);
            cairo_surface_destroy(surface);
            //if (webView->usesLayeredWindow())
            //    webView->performLayeredWindowUpdate();
            EndPaint(hWnd, &paint);
        }
        break;
    }
    case WM_ERASEBKGND:
        // Don't perform a background erase.
        handled = true;
        lResult = 1;
        break;
    case WM_PRINTCLIENT:
        //webView->paint((HDC)wParam, lParam);
        break;
    case WM_DESTROY:
        //webView->setIsBeingDestroyed();
        //webView->close();
        break;
    case WM_GESTURENOTIFY:
        //handled = webView->gestureNotify(wParam, lParam);
        break;
    case WM_GESTURE:
        //handled = webView->gesture(wParam, lParam);
        break;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        ::SetFocus(hWnd);
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_CANCELMODE:
        WKViewSendMouseEvent(webView, toMSG(hWnd, message, wParam, lParam));
        break;
    case WM_MOUSEWHEEL:
    case WM_VISTA_MOUSEHWHEEL:
        if (LOWORD(wParam) & MK_SHIFT) {
            static const SHORT kLinesPerScroll = 20;
            SHORT mouseX = (SHORT)LOWORD(lParam);
            SHORT mouseY = (SHORT)HIWORD(lParam);
            SHORT mouseZ = (SHORT)HIWORD(wParam) / WHEEL_DELTA;
            WKViewScrollBy(webView, WKPointMake(mouseX, mouseY), WKPointMake(0, mouseZ * kLinesPerScroll));
        } else
            WKViewSendWheelEvent(webView, toMSG(hWnd, message, wParam, lParam));
        break;
    case WM_SYSKEYDOWN:
        //handled = webView->keyDown(wParam, lParam, true);
        break;
    case WM_KEYDOWN:
        //handled = webView->keyDown(wParam, lParam);
        break;
    case WM_SYSKEYUP:
        //handled = webView->keyUp(wParam, lParam, true);
        break;
    case WM_KEYUP:
        //handled = webView->keyUp(wParam, lParam);
        break;
    case WM_SYSCHAR:
        //handled = webView->keyPress(wParam, lParam, true);
        break;
    case WM_CHAR:
        //handled = webView->keyPress(wParam, lParam);
        break;
        // FIXME: We need to check WM_UNICHAR to support supplementary characters (that don't fit in 16 bits).
    case WM_SIZE:
        WKViewSetSize(webView, WKSizeMake(LOWORD(lParam), HIWORD(lParam)));
        WKViewSetIsVisible(webView, (lParam != 0));
        break;
    case WM_SHOWWINDOW:
        lResult = DefWindowProc(hWnd, message, wParam, lParam);
        if (wParam == 0) {
            // The window is being hidden (e.g., because we switched tabs).
            // Null out our backing store.
            //webView->deleteBackingStore();
        }
        break;
    case WM_SETFOCUS: {
        WKViewSetIsFocused(webView, true);
        //COMPtr<IWebUIDelegate> uiDelegate;
        //COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate;
        //if (SUCCEEDED(webView->uiDelegate(&uiDelegate)) && uiDelegate
        //    && SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**) &uiDelegatePrivate)) && uiDelegatePrivate)
        //    uiDelegatePrivate->webViewReceivedFocus(webView);

        //FocusController& focusController = webView->page()->focusController();
        //if (Frame* frame = focusController.focusedFrame()) {
        //    // Send focus events unless the previously focused window is a
        //    // child of ours (for example a plugin).
        //    if (!IsChild(hWnd, reinterpret_cast<HWND>(wParam)))
        //        focusController.setFocused(true);
        //} else
        //    focusController.setFocused(true);
        break;
    }
    case WM_KILLFOCUS: {
        WKViewSetIsFocused(webView, false);
        //COMPtr<IWebUIDelegate> uiDelegate;
        //COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate;
        //HWND newFocusWnd = reinterpret_cast<HWND>(wParam);
        //if (SUCCEEDED(webView->uiDelegate(&uiDelegate)) && uiDelegate
        //    && SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**) &uiDelegatePrivate)) && uiDelegatePrivate)
        //    uiDelegatePrivate->webViewLostFocus(webView, (OLE_HANDLE)(ULONG64)newFocusWnd);

        //FocusController& focusController = webView->page()->focusController();
        //Frame& frame = focusController.focusedOrMainFrame();
        //webView->resetIME(&frame);
        //// Send blur events unless we're losing focus to a child of ours.
        //if (!IsChild(hWnd, newFocusWnd))
        //    focusController.setFocused(false);

        //// If we are pan-scrolling when we lose focus, stop the pan scrolling.
        //frame.eventHandler().stopAutoscrollTimer();

        break;
    }
    case WM_WINDOWPOSCHANGED:
        //if (reinterpret_cast<WINDOWPOS*>(lParam)->flags & SWP_SHOWWINDOW)
        //    webView->updateActiveStateSoon();
        handled = false;
        break;
    case WM_CUT:
        //webView->cut(0);
        break;
    case WM_COPY:
        //webView->copy(0);
        break;
    case WM_PASTE:
        //webView->paste(0);
        break;
    case WM_CLEAR:
        //webView->delete_(0);
        break;
    case WM_COMMAND:
        //if (HIWORD(wParam))
        //    handled = webView->execCommand(wParam, lParam);
        //else // If the high word of wParam is 0, the message is from a menu
        //    webView->performContextMenuAction(wParam, lParam, false);
        break;
    case WM_MENUCOMMAND:
        //webView->performContextMenuAction(wParam, lParam, true);
        break;
    case WM_CONTEXTMENU:
        //handled = webView->handleContextMenuEvent(wParam, lParam);
        break;
    case WM_INITMENUPOPUP:
        //handled = webView->onInitMenuPopup(wParam, lParam);
        break;
    case WM_MEASUREITEM:
        //handled = webView->onMeasureItem(wParam, lParam);
        break;
    case WM_DRAWITEM:
        //handled = webView->onDrawItem(wParam, lParam);
        break;
    case WM_UNINITMENUPOPUP:
        //handled = webView->onUninitMenuPopup(wParam, lParam);
        break;
    case WM_XP_THEMECHANGED:
        // if (Frame* coreFrame = core(mainFrameImpl)) {
        //     webView->deleteBackingStore();
        //     coreFrame->page()->theme().themeChanged();
        //     ScrollbarTheme::theme()->themeChanged();
        //     RECT windowRect;
        //     ::GetClientRect(hWnd, &windowRect);
        //     ::InvalidateRect(hWnd, &windowRect, false);
        //     if (webView->isAcceleratedCompositing())
        //         webView->m_backingLayer->setNeedsDisplay();
        //}
        break;
    case WM_MOUSEACTIVATE:
        //webView->setMouseActivated(true);
        handled = false;
        break;
    case WM_GETDLGCODE: {
        //COMPtr<IWebUIDelegate> uiDelegate;
        //COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate;
        //LONG_PTR dlgCode = 0;
        //UINT keyCode = 0;
        //if (lParam) {
        //    LPMSG lpMsg = (LPMSG)lParam;
        //    if (lpMsg->message == WM_KEYDOWN)
        //        keyCode = (UINT) lpMsg->wParam;
        //}
        //if (SUCCEEDED(webView->uiDelegate(&uiDelegate)) && uiDelegate
        //    && SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**) &uiDelegatePrivate)) && uiDelegatePrivate
        //    && SUCCEEDED(uiDelegatePrivate->webViewGetDlgCode(webView, keyCode, &dlgCode)))
        //    return dlgCode;
        handled = false;
        break;
    }
    case WM_GETOBJECT:
        //handled = webView->onGetObject(wParam, lParam, lResult);
        break;
    case WM_IME_STARTCOMPOSITION:
        //handled = webView->onIMEStartComposition();
        break;
    case WM_IME_REQUEST:
        //lResult = webView->onIMERequest(wParam, lParam);
        break;
    case WM_IME_COMPOSITION:
        //handled = webView->onIMEComposition(lParam);
        break;
    case WM_IME_ENDCOMPOSITION:
        //handled = webView->onIMEEndComposition();
        break;
    case WM_IME_CHAR:
        //handled = webView->onIMEChar(wParam, lParam);
        break;
    case WM_IME_NOTIFY:
        //handled = webView->onIMENotify(wParam, lParam, &lResult);
        break;
    case WM_IME_SELECT:
        //handled = webView->onIMESelect(wParam, lParam);
        break;
    case WM_IME_SETCONTEXT:
        //handled = webView->onIMESetContext(wParam, lParam);
        break;
    case WM_TIMER:
        //switch (wParam) {
        //    case UpdateActiveStateTimer:
        //        KillTimer(hWnd, UpdateActiveStateTimer);
        //        webView->updateActiveState();
        //        break;
        //    case DeleteBackingStoreTimer:
        //        webView->deleteBackingStore();
        //        break;
        //}
        break;
    case WM_SETCURSOR:
        //handled = ::SetCursor(webView->m_lastSetCursor);
        break;
    case WM_VSCROLL:
        //handled = webView->verticalScroll(wParam, lParam);
        break;
    case WM_HSCROLL:
        //handled = webView->horizontalScroll(wParam, lParam);
        break;
    default:
        handled = false;
        break;
    }

    //if (webView->needsDisplay() && message != WM_PAINT)
    //    ::UpdateWindow(hWnd);

    if (!handled)
        lResult = DefWindowProc(hWnd, message, wParam, lParam);
    
    // Let the client know whether we consider this message handled.
    return (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP) ? !handled : lResult;
}
