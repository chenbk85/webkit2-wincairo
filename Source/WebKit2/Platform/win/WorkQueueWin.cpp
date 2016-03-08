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
#include "WorkQueue.h"

#include <WebCore/NotImplemented.h>
#include <functional>
#include <future>

void WorkQueue::platformInitialize(const char*, QOS)
{
    m_preventAsyncWorkerThreadLaunch = false;
}

void WorkQueue::platformInvalidate()
{
    notImplemented();
}

static std::chrono::nanoseconds now()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
}

static bool compareWorkItem(const std::pair<std::chrono::nanoseconds, std::function<void ()>>& a, const std::pair<std::chrono::nanoseconds, std::function<void ()>>& b)
{
    return a.first < b.first;
}

void WorkQueue::asyncWorkerThreadMain()
{
    MutexLocker lock(m_workItemQueueLock);

    while (!m_workItemQueue.empty() && m_workItemQueue.front().first <= now()) {
        m_workItemQueue.front().second();

        std::pop_heap(m_workItemQueue.begin(), m_workItemQueue.end(), compareWorkItem);
        m_workItemQueue.pop_back();
    }

    m_preventAsyncWorkerThreadLaunch = false;
}

void WorkQueue::dispatch(std::function<void ()> function)
{
    MutexLocker lock(m_workItemQueueLock);

    m_workItemQueue.push_back(std::make_pair(now(), function));
    std::push_heap(m_workItemQueue.begin(), m_workItemQueue.end(), compareWorkItem);

    if (!m_preventAsyncWorkerThreadLaunch) {
        m_preventAsyncWorkerThreadLaunch = true;
        std::async(std::launch::async, [this] { asyncWorkerThreadMain(); });
    }
}

void WorkQueue::dispatchAfter(std::chrono::nanoseconds duration, std::function<void ()> function)
{
    std::async(std::launch::async, [=] {
        std::this_thread::sleep_until(std::chrono::high_resolution_clock::now() + duration);
        dispatch(function);
    });
}
