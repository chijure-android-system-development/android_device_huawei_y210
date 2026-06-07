/*
 * Y210CameraInterface.h — GB-era CameraHardwareInterface matching the Y210 blob's vtable.
 *
 * The proprietary libcamera.y210.so was compiled against a Qualcomm-modified
 * CameraHardwareInterface that inserts two extra virtual methods between
 * cancelAutoFocus (slot 18) and takePicture.  Declaring those extras here
 * lets the compiler generate correct slot offsets for all subsequent methods,
 * so we can call them via normal C++ virtual dispatch instead of raw offsets.
 *
 * Vtable layout of the Y210 blob:
 *
 *  slot  0  ~CameraHardwareInterface [non-deleting dtor]
 *  slot  1  ~CameraHardwareInterface [deleting dtor]
 *  slot  2  getPreviewHeap
 *  slot  3  getRawHeap
 *  slot  4  setCallbacks
 *  slot  5  enableMsgType
 *  slot  6  disableMsgType
 *  slot  7  msgTypeEnabled
 *  slot  8  startPreview
 *  slot  9  useOverlay
 *  slot 10  setOverlay
 *  slot 11  stopPreview
 *  slot 12  previewEnabled
 *  slot 13  startRecording
 *  slot 14  stopRecording
 *  slot 15  recordingEnabled
 *  slot 16  releaseRecordingFrame
 *  slot 17  autoFocus
 *  slot 18  cancelAutoFocus
 *  slot 19  [Qualcomm extra #1]       ← not called by us
 *  slot 20  [Qualcomm extra #2]       ← not called by us
 *  slot 21  takePicture(sp<ISurface>) ← different signature; use manual dispatch
 *  slot 22  cancelPicture
 *  slot 23  setParameters
 *  slot 24  getParameters
 *  slot 25  sendCommand
 *  slot 26  release
 *  slot 27  dump
 */

#ifndef Y210_CAMERA_INTERFACE_H
#define Y210_CAMERA_INTERFACE_H

#include <binder/IMemory.h>
#include <camera/CameraParameters.h>
#include <surfaceflinger/ISurface.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/Vector.h>

namespace android {

class Overlay;

typedef void (*notify_callback)(int32_t msgType, int32_t ext1,
                                int32_t ext2, void* user);
typedef void (*data_callback)(int32_t msgType,
                              const sp<IMemory>& dataPtr, void* user);
typedef void (*data_callback_timestamp)(nsecs_t timestamp, int32_t msgType,
                                        const sp<IMemory>& dataPtr, void* user);

class CameraHardwareInterface : public virtual RefBase {
public:
    virtual ~CameraHardwareInterface() {}

    virtual sp<IMemoryHeap> getPreviewHeap() const = 0;           /* slot  2 */
    virtual sp<IMemoryHeap> getRawHeap() const = 0;               /* slot  3 */

    virtual void setCallbacks(notify_callback notify_cb,           /* slot  4 */
                              data_callback data_cb,
                              data_callback_timestamp data_cb_timestamp,
                              void* user) = 0;

    virtual void enableMsgType(int32_t msgType) = 0;              /* slot  5 */
    virtual void disableMsgType(int32_t msgType) = 0;             /* slot  6 */
    virtual bool msgTypeEnabled(int32_t msgType) = 0;             /* slot  7 */

    virtual status_t startPreview() = 0;                          /* slot  8 */

    virtual bool useOverlay() { return false; }                   /* slot  9 */
    virtual status_t setOverlay(const sp<Overlay>&) {             /* slot 10 */
        return (status_t)INVALID_OPERATION;
    }

    virtual void stopPreview() = 0;                               /* slot 11 */
    virtual bool previewEnabled() = 0;                            /* slot 12 */
    virtual status_t startRecording() = 0;                        /* slot 13 */
    virtual void stopRecording() = 0;                             /* slot 14 */
    virtual bool recordingEnabled() = 0;                          /* slot 15 */
    virtual void releaseRecordingFrame(const sp<IMemory>& mem) = 0; /* slot 16 */

    virtual status_t autoFocus() = 0;                             /* slot 17 */
    virtual status_t cancelAutoFocus() = 0;                       /* slot 18 */

    /* Qualcomm extras — present in blob, not used by us */
    virtual status_t qcomExtra1() { return 0; }                   /* slot 19 */
    virtual status_t qcomExtra2() { return 0; }                   /* slot 20 */

    /* takePicture is at blob slot 21 but has a DIFFERENT signature
     * (takes const sp<ISurface>&).  Declare with standard signature to keep
     * the slots below correct, but dispatch this one manually. */
    virtual status_t takePicture() = 0;                           /* slot 21 */
    virtual status_t cancelPicture() = 0;                         /* slot 22 */
    virtual status_t setParameters(const CameraParameters& p) = 0; /* slot 23 */
    virtual CameraParameters getParameters() const = 0;           /* slot 24 */
    virtual status_t sendCommand(int32_t cmd,                      /* slot 25 */
                                 int32_t arg1, int32_t arg2) = 0;
    virtual void release() = 0;                                   /* slot 26 */
    virtual status_t dump(int fd,                                  /* slot 27 */
                          const Vector<String16>& args) const = 0;
};

/* GB-era camera_info (same layout as ICS camera_info from hardware/camera.h) */
struct CameraInfo { int facing; int orientation; };

} /* namespace android */

#endif /* Y210_CAMERA_INTERFACE_H */
