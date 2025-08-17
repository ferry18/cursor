#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include "utils.h"

class FrameBuffer {
public:
    struct Frame {
        std::vector<uint8_t> data;
        ToupcamFrameInfoV4 info;
        std::chrono::steady_clock::time_point timestamp;
        
        Frame() : data(CAMERA_WIDTH * CAMERA_HEIGHT) {
            std::memset(&info, 0, sizeof(info));
        }
    };
    
    FrameBuffer(size_t bufferCount = 3);
    ~FrameBuffer();
    
    // Get a frame for writing (camera thread)
    Frame* getWriteFrame();
    void commitWriteFrame();
    
    // Get the latest frame for reading (UI thread)
    const Frame* getReadFrame();
    void releaseReadFrame();
    
    // Stats
    size_t getDroppedFrames() const { return m_droppedFrames; }
    
private:
    std::vector<std::unique_ptr<Frame>> m_frames;
    std::atomic<size_t> m_writeIndex{0};
    std::atomic<size_t> m_readIndex{0};
    std::atomic<size_t> m_droppedFrames{0};
    std::atomic<bool> m_frameReady{false};
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
};

#endif // FRAME_BUFFER_H