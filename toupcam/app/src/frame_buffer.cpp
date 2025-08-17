#include "frame_buffer.h"

FrameBuffer::FrameBuffer(size_t bufferCount) {
    m_frames.reserve(bufferCount);
    for (size_t i = 0; i < bufferCount; ++i) {
        m_frames.emplace_back(std::make_unique<Frame>());
    }
}

FrameBuffer::~FrameBuffer() = default;

FrameBuffer::Frame* FrameBuffer::getWriteFrame() {
    size_t nextWrite = (m_writeIndex + 1) % m_frames.size();
    
    // If next write would catch up to read, we're dropping frames
    if (nextWrite == m_readIndex && m_frameReady) {
        m_droppedFrames++;
        return nullptr;
    }
    
    return m_frames[m_writeIndex].get();
}

void FrameBuffer::commitWriteFrame() {
    m_writeIndex = (m_writeIndex + 1) % m_frames.size();
    m_frameReady = true;
    m_cv.notify_one();
}

const FrameBuffer::Frame* FrameBuffer::getReadFrame() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (!m_frameReady) {
        return nullptr;
    }
    
    // Move read index to latest written frame
    size_t latest = m_writeIndex;
    if (latest == 0) {
        latest = m_frames.size() - 1;
    } else {
        latest--;
    }
    
    m_readIndex = latest;
    return m_frames[m_readIndex].get();
}

void FrameBuffer::releaseReadFrame() {
    // No-op for now, but could be used for synchronization
}