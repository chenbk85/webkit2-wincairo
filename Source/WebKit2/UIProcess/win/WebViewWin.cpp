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
#include "WebViewWin.h"

#include "CoordinatedDrawingAreaProxy.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include <WebCore/CoordinatedGraphicsScene.h>
#include <WebCore/PlatformContextCairo.h>
#include <WebCore/NotImplemented.h>
#include <cairo/cairo.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebView> WebView::create(WebContext* context, WebPageGroup* pageGroup)
{
    return adoptRef(new WebViewWin(context, pageGroup));
}

WebViewWin::WebViewWin(WebContext* context, WebPageGroup* pageGroup)
    : WebView(context, pageGroup)
{
}

void WebViewWin::paintToCairoSurface(cairo_surface_t* surface)
{
    CoordinatedGraphicsScene* scene = coordinatedGraphicsScene();
    if (!scene)
        return;

    PlatformContextCairo context(cairo_create(surface));

    const FloatPoint& position = contentPosition();
    double effectiveScale = m_page->deviceScaleFactor() * contentScaleFactor();

    cairo_matrix_t transform = { effectiveScale, 0, 0, effectiveScale, - position.x() * m_page->deviceScaleFactor(), - position.y() * m_page->deviceScaleFactor() };
    cairo_set_matrix(context.cr(), &transform);
    scene->paintToGraphicsContext(&context);
}

void WebViewWin::scrollBy(const FloatPoint& scrollPosition, const FloatPoint& scrollOffset)
{
    if (CoordinatedDrawingAreaProxy* drawingArea = static_cast<CoordinatedDrawingAreaProxy*>(page()->drawingArea())) {
        // Web Process expects sizes in UI units, and not raw device units.
        drawingArea->setSize(roundedIntSize(dipSize()), IntSize(), IntSize(-scrollOffset.x(), -scrollOffset.y()));
    }
}

void WebViewWin::sendMouseEvent(const MSG& event)
{
    m_page->handleMouseEvent(NativeWebMouseEvent(event));
}

void WebViewWin::sendWheelEvent(const MSG& event)
{
    m_page->handleWheelEvent(NativeWebWheelEvent(event));
}

void WebViewWin::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&)
{
    notImplemented();
}

void WebViewWin::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    notImplemented();
}

void WebViewWin::didFinishLoadForMainFrame()
{
    notImplemented();
}

void WebViewWin::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
    notImplemented();
}

} // namespace WebKit
