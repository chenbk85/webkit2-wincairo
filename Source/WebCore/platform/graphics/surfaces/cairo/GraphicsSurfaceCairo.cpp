/*
 * Copyright (C) 2014 Daewoong Jang (daewoong.jang@navercorp.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include "GraphicsSurface.h"

#if USE(GRAPHICS_SURFACE)

#include "GLPlatformContext.h"
#include "GLTransportSurface.h"
#include "NotImplemented.h"
#include "TextureMapperGL.h"

namespace WebCore {

GraphicsSurfaceToken GraphicsSurface::platformExport()
{
    notImplemented();
    return 0;
}

uint32_t GraphicsSurface::platformGetTextureID()
{
    notImplemented();
    return 0;
}

void GraphicsSurface::platformCopyToGLTexture(uint32_t /*target*/, uint32_t /*id*/, const IntRect& /*targetRect*/, const IntPoint& /*offset*/)
{
    notImplemented();
}

void GraphicsSurface::platformCopyFromTexture(uint32_t textureId, const IntRect&)
{
    notImplemented();
}

void GraphicsSurface::platformPaintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity)
{
    notImplemented();
}

uint32_t GraphicsSurface::platformFrontBuffer() const
{
    notImplemented();
    return 0;
}

uint32_t GraphicsSurface::platformSwapBuffers()
{
    notImplemented();
    return 0;
}

IntSize GraphicsSurface::platformSize() const
{
    notImplemented();
    return IntSize();
}

PassRefPtr<GraphicsSurface> GraphicsSurface::platformCreate(const IntSize& size, Flags flags, const PlatformGraphicsContext3D shareContext)
{
    notImplemented();
    return 0;
}

PassRefPtr<GraphicsSurface> GraphicsSurface::platformImport(const IntSize& size, Flags flags, const GraphicsSurfaceToken& token)
{
    notImplemented();
    return 0;
}

char* GraphicsSurface::platformLock(const IntRect&, int* /*outputStride*/, LockOptions)
{
    notImplemented();
    return 0;
}

void GraphicsSurface::platformUnlock()
{
    notImplemented();
}

void GraphicsSurface::platformDestroy()
{
    notImplemented();
}

std::unique_ptr<GraphicsContext> GraphicsSurface::platformBeginPaint(const IntSize&, char*, int)
{
    notImplemented();
    return nullptr;
}

PassRefPtr<Image> GraphicsSurface::createReadOnlyImage(const IntRect&)
{
    notImplemented();
    return 0;
}

}

#endif // USE(GRAPHICS_SURFACE)
