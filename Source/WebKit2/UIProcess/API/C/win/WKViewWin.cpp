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
#include "WKViewWin.h"

#include "WKAPICast.h"
#include "WKSharedAPICast.h"
#include "WebViewWin.h"
#include <cairo/cairo.h>

using namespace WebKit;

void WKViewPaintToCairoSurface(WKViewRef viewRef, cairo_surface_t* surface)
{
    static_cast<WebViewWin*>(toImpl(viewRef))->paintToCairoSurface(surface);
}

void WKViewScrollBy(WKViewRef viewRef, WKPoint position, WKPoint offset)
{
    static_cast<WebViewWin*>(toImpl(viewRef))->scrollBy(toIntPoint(position), toIntPoint(offset));
}

void WKViewSendMouseEvent(WKViewRef viewRef, const MSG& event)
{
    static_cast<WebViewWin*>(toImpl(viewRef))->sendMouseEvent(event);
}

void WKViewSendWheelEvent(WKViewRef viewRef, const MSG& event)
{
    static_cast<WebViewWin*>(toImpl(viewRef))->sendWheelEvent(event);
}
