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
#include "ProcessLauncher.h"

#include "ProcessExecutablePath.h"
#include "WSASocketPair.h"
#include <WebCore/NotImplemented.h>
#include <wtf/RunLoop.h>

namespace WebKit {

static void handleLastError()
{
    DWORD lastError = ::GetLastError();
    // FIXME: What should we do here?
    ASSERT_NOT_REACHED();
}

void ProcessLauncher::launchProcess()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        handleLastError();
        return;
    }

    SOCKET sockets[2];
    if (WSASocketPair(AF_INET, SOCK_STREAM, 0, sockets) < 0) {
        handleLastError();
        return;
    }

    String executablePath;
    switch (m_launchOptions.processType) {
    case WebProcess:
        executablePath = executablePathOfWebProcess();
        break;
#if ENABLE(PLUGIN_PROCESS)
    case PluginProcess:
        executablePath = executablePathOfPluginProcess();
        pluginPath = m_launchOptions.extraInitializationData.get("plugin-path");
        break;
#endif
#if ENABLE(NETWORK_PROCESS)
    case NetworkProcess:
        executablePath = executablePathOfNetworkProcess();
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    String argv;
    argv.append((LChar)'\"');
    argv.append((LPCWSTR)executablePath.charactersWithNullTermination().data());
    argv.append("\" ");
    argv.append(processTypeAsString(m_launchOptions.processType));
    argv.append((LChar)' ');
    argv.append(String::number(sockets[0]));

    STARTUPINFO startupInfo = { sizeof(STARTUPINFO), 0, };
    PROCESS_INFORMATION processInformation = { 0, };

    if (!::SetHandleInformation((HANDLE)sockets[0], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
        handleLastError();
        return;
    }

    BOOL result = ::CreateProcessW(0, (LPWSTR)argv.charactersWithNullTermination().data(), 0, 0, true, 0, 0, 0, &startupInfo, &processInformation);

    ::closesocket(sockets[0]);

    if (!result) {
        handleLastError();
        return;
    }

    ::CloseHandle(processInformation.hThread);

    RunLoop::main().dispatch(WTF::bind(&ProcessLauncher::didFinishLaunchingProcess, this, processInformation.hProcess, (HANDLE)sockets[1]));
}

void ProcessLauncher::terminateProcess()
{
    notImplemented();
}

void ProcessLauncher::platformInvalidate()
{
    notImplemented();
}

} // namespace WebKit
