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
#include "Connection.h"

#include "DataReference.h"

namespace IPC {

static const size_t initialMessageMaxSize = 4096;

class AsyncFileCallbacks : public NonblockIoHandle::Client {
public:
    void handleDidClose(NonblockIoHandle*) override
    {
        (m_connection->*m_closeCallback)();
    }
    void handleDidRead(NonblockIoHandle*, size_t numberOfBytesTransferred) override
    {
        (m_connection->*m_readCallback)(numberOfBytesTransferred);
    }
    void handleDidWrite(NonblockIoHandle*, size_t numberOfBytesTransferred) override
    {
        (m_connection->*m_writeCallback)(numberOfBytesTransferred);
    }

    typedef void (Connection::*TransferCallback)(size_t);
    typedef void (Connection::*StateChangeCallback)();

    AsyncFileCallbacks(Connection* connection, TransferCallback readCallback, TransferCallback writeCallback, StateChangeCallback closeCallback)
        : m_connection(connection)
        , m_readCallback(readCallback)
        , m_writeCallback(writeCallback)
        , m_closeCallback(closeCallback)
    {
    }

private:
    Connection* m_connection;
    TransferCallback m_readCallback;
    TransferCallback m_writeCallback;
    StateChangeCallback m_closeCallback;
};

std::shared_ptr<CompletionPort> sharedCompletionPort()
{
    static std::shared_ptr<CompletionPort> completionPort = CompletionPort::create();
    return completionPort;
}

void Connection::platformInitialize(Identifier identifier)
{
    m_readBuffer.resize(initialMessageMaxSize);
    m_readBufferSize = 0;
    m_pendingWriteMessage = false;
    m_completionCallbacks = std::unique_ptr<AsyncFileCallbacks>(new AsyncFileCallbacks(this, &Connection::completeReadHandler, &Connection::completeWriteHandler, &Connection::completeCloseHandler));
    m_ioHandle = NonblockIoHandle::create((SOCKET)identifier, sharedCompletionPort(), m_completionCallbacks.get());
}

void Connection::platformInvalidate()
{
    m_isConnected = false;

    m_ioHandle->close();
}

bool Connection::open()
{
    m_isConnected = true;

    m_connectionQueue->dispatch([this] () {
        m_ioHandle->read(m_readBuffer.data(), m_readBuffer.size());
    });

    return true;
}

bool Connection::processMessage()
{
    if (m_readBufferSize == 0)
        return false;

    size_t messageSize = *reinterpret_cast<size_t*>(m_readBuffer.data()) + sizeof(size_t);
    if (m_readBufferSize < messageSize) {
        if (m_readBuffer.size() < messageSize)
            m_readBuffer.resize(messageSize);
        return false;
    }

    Vector<Attachment> attachments;
    auto decoder = std::make_unique<MessageDecoder>(DataReference(m_readBuffer.data() + sizeof(size_t), messageSize - sizeof(size_t)), std::move(attachments));

    processIncomingMessage(std::move(decoder));

    m_readBufferSize -= messageSize;
    m_readBuffer.remove(0, messageSize);
    m_readBuffer.resizeToFit(std::max(initialMessageMaxSize, m_readBufferSize));

    return true;
}

void Connection::completeReadHandler(size_t numberOfBytesTransferred)
{
    if (numberOfBytesTransferred == 0) {
        connectionDidClose();
        return;
    }

    m_readBufferSize += numberOfBytesTransferred;

    if (m_readBufferSize < sizeof(size_t)) {
        ASSERT_NOT_REACHED();
        return;
    }

    while (processMessage()) { }

    auto result = m_ioHandle->read(m_readBuffer.data() + m_readBufferSize, m_readBuffer.size() - m_readBufferSize);
    if (result.first == NonblockIoHandle::Shutdown)
        connectionDidClose();
}

void Connection::completeWriteHandler(size_t numberOfBytesTransferred)
{
    m_pendingWriteMessage.release();

    m_connectionQueue->dispatch(WTF::bind(&Connection::sendOutgoingMessages, this));
}

void Connection::completeCloseHandler()
{
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return !m_pendingWriteMessage;
}

bool Connection::sendOutgoingMessage(std::unique_ptr<MessageEncoder> encoder)
{
    ASSERT(!m_pendingWriteMessage);

    std::unique_ptr<ArgumentEncoder> message(new ArgumentEncoder);

    *message << encoder->bufferSize();
    message->encodeFixedLengthData(encoder->buffer(), encoder->bufferSize(), alignof(uint32_t));

    auto result = m_ioHandle->write(message->buffer(), message->bufferSize());
    if (result.first == NonblockIoHandle::Complete)
        return true;

    if (result.first == NonblockIoHandle::Shutdown) {
        connectionDidClose();
        return false;
    }

    m_pendingWriteMessage = std::move(message);

    return false;
}

void Connection::willSendSyncMessage(unsigned flags)
{
    UNUSED_PARAM(flags);
}
    
void Connection::didReceiveSyncReply(unsigned flags)
{
    UNUSED_PARAM(flags);    
}

} // namespace IPC
