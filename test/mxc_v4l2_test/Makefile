DIR = V4L2
BUILD = mxc_v4l2_output.out mxc_v4l2_still.out mxc_v4l2_tvin.out \
	mxc_v4l2_overlay.out mxc_v4l2_capture.out mx6s_v4l2_capture.out \
	mx6s_v4l2_cap_drm.out mx8_v4l2_cap_drm.out mx8_v4l2_m2m_test.out
LDFLAGS += -lpthread -ldrm
CFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/libdrm
COPY = autorun-v4l2.sh README
