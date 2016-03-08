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
#include "ProcessExecutablePath.h"

#include <wtf/NeverDestroyed.h>
#include <Windows.h>
#include <Shlwapi.h>

namespace WebKit {

static const UChar webKit2DLLName[] = L"WebKit2.dll";
static const UChar webKit2WebProcessEXEName[] = L"WebKit2WebProcess.exe";
static const UChar webKit2PluginProcessEXEName[] = L"WebKit2PluginProcess.exe";
static const UChar webKit2NetworkProcessEXEName[] = L"WebKit2NetworkProcess.exe";

static String executablePath()
{
    HMODULE webKit2Module = ::GetModuleHandleW(webKit2DLLName);
    ASSERT(webKit2Module);
    if (!webKit2Module) {
        ASSERT_NOT_REACHED();
        return String();
    }

    UChar webKit2ModulePath[MAX_PATH];
    if (!::GetModuleFileNameW(webKit2Module, webKit2ModulePath, WTF_ARRAY_LENGTH(webKit2ModulePath))) {
        ASSERT_NOT_REACHED();
        return String();
    }

    ::PathRemoveFileSpecW(webKit2ModulePath);

    return String(webKit2ModulePath);
}

static String executablePathOfProcess(const UChar* processName)
{
    String webKit2ModulePath(executablePath());
    webKit2ModulePath.append('\\');
    webKit2ModulePath.append(processName);
    return webKit2ModulePath;
}

String executablePathOfWebProcess()
{
    static NeverDestroyed<const String> path(executablePathOfProcess(webKit2WebProcessEXEName));
    return path;
}

String executablePathOfPluginProcess()
{
    static NeverDestroyed<const String> path(executablePathOfProcess(webKit2PluginProcessEXEName));
    return path;
}

#if ENABLE(NETWORK_PROCESS)
String executablePathOfNetworkProcess()
{
    static NeverDestroyed<const String> path(executablePathOfProcess(webKit2NetworkProcessEXEName));
    return path;
}
#endif

} // namespace WebKit
