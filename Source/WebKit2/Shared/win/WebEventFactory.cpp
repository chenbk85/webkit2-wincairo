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

#include "config.h"
#include "WebEventFactory.h"

#include <WebCore/Scrollbar.h>

namespace WebCore {
// WebCore/platform/win/KeyEventWin.cpp
String keyIdentifierForWindowsKeyCode(unsigned short keyCode);
}

using namespace WebCore;

namespace WebKit {

static inline WebEvent::Type typeForEvent(UINT message)
{
    switch (message) {
    case WM_MOUSEMOVE:
        return WebEvent::MouseMove;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        return WebEvent::MouseDown;
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        return WebEvent::MouseUp;
    case WM_MOUSEWHEEL:
        return WebEvent::Wheel;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        return WebEvent::KeyDown;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        return WebEvent::KeyUp;
    case WM_SYSCHAR:
    case WM_CHAR:
        return WebEvent::Char;
    default:
        return WebEvent::NoType;
    }
}

static inline WebEvent::Modifiers modifiersForEvent(WPARAM wparam)
{
    unsigned result = 0;

    if (wparam & MK_SHIFT)
        result |= WebEvent::ShiftKey;
    if (wparam & MK_CONTROL)
        result |= WebEvent::ControlKey;

    return static_cast<WebEvent::Modifiers>(result);
}

static inline WebMouseEvent::Button buttonForMouseEvent(const MSG& event)
{
    switch (event.message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
        return WebMouseEvent::LeftButton;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONUP:
        return WebMouseEvent::MiddleButton;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONUP:
        return WebMouseEvent::RightButton;
    }

    return WebMouseEvent::NoButton;
}

static inline WebMouseEvent::Button buttonForKeyboardEvent(WPARAM wparam)
{
    if (wparam & MK_LBUTTON)
        return WebMouseEvent::LeftButton;
    if (wparam & MK_MBUTTON)
        return WebMouseEvent::MiddleButton;
    if (wparam & MK_RBUTTON)
        return WebMouseEvent::RightButton;

    return WebMouseEvent::NoButton;
}

static inline int32_t clickCountForEvent(UINT message)
{
    switch (message) {
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
        return 2;
    default:
        return 1;
    }
}

static inline String textFromEvent(const MSG& event)
{
    if (event.message != WM_CHAR && event.message != WM_SYSCHAR)
        return emptyString();
    UChar uchar = static_cast<wchar_t>(event.wParam);
    return String(&uchar, 1);
}

static inline String unmodifiedTextFromEvent(const MSG& event)
{
    if (event.message != WM_CHAR && event.message != WM_SYSCHAR)
        return emptyString();
    UChar uchar = static_cast<wchar_t>(event.wParam);
    return String(&uchar, 1);
}

static inline bool isKeypadEvent(WPARAM wparam)
{
    return (wparam >= VK_NUMPAD0) && (wparam <= VK_DIVIDE);
}

static inline bool isSystemKeyEvent(WPARAM wparam)
{
    return false;
}

static inline double convertMillisecondToSecond(DWORD timestamp)
{
    return static_cast<double>(timestamp) / 1000;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const MSG& event)
{
    IntPoint position(LOWORD(event.lParam), HIWORD(event.lParam));
    return WebMouseEvent(typeForEvent(event.message),
        buttonForMouseEvent(event),
        position,
        position,
        0 /* deltaX */,
        0 /* deltaY */,
        0 /* deltaZ */,
        clickCountForEvent(event.message),
        modifiersForEvent(event.wParam),
        convertMillisecondToSecond(event.time));
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const MSG& event)
{
    IntPoint position(LOWORD(event.lParam), HIWORD(event.lParam));
    return WebWheelEvent(typeForEvent(event.message),
        position,
        position,
        FloatSize(0.0f, GET_WHEEL_DELTA_WPARAM(event.wParam)),
        FloatSize(0.0f, GET_WHEEL_DELTA_WPARAM(event.wParam) / static_cast<float>(Scrollbar::pixelsPerLineStep())),
        WebWheelEvent::ScrollByPixelWheelEvent,
        modifiersForEvent(event.wParam),
        convertMillisecondToSecond(event.time));
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const MSG& event)
{
    return WebKeyboardEvent(typeForEvent(event.message),
        textFromEvent(event),
        unmodifiedTextFromEvent(event),
        keyIdentifierForWindowsKeyCode(event.wParam),
        event.wParam,
        event.wParam,
        0 /* macCharCode */,
        false /* FIXME: isAutoRepeat */,
        isKeypadEvent(event.message),
        isSystemKeyEvent(event.message),
        modifiersForEvent(event.wParam),
        convertMillisecondToSecond(event.time));
}

} // namespace WebKit
