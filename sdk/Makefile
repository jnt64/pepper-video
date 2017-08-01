CXX=clang++
PLATFORM=mac

APPS=../apps
ELECTRON := $(APPS)/electron
ELECTRON_LIBS := $(APPS)/electron/native/lib/$(PLATFORM)

# use pkg-config for getting CFLAGS and LDLIBS
FFMPEG_LIBS=    libavdevice   \
                libavformat   \
                libavfilter   \
                libavcodec    \
                libswresample \
                libswscale    \
                libavutil     \

CFLAGS += -Wall
CFLAGS += -I${HOME}/Technology/nacl/nacl_sdk/pepper_49/include 
CFLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(CFLAGS)

LDLIBS += -L${HOME}/Technology/nacl/nacl_sdk/pepper_49/lib/mac_host/Release
LDLIBS += -lppapi_cpp -lppapi -lppapi_gles2 -lpthread
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)

SOURCE=src/cpp/include/sweet_player.cpp
TARGET := build/lib/$(PLATFORM)/ppapi_sweet.so

.phony: all clean ppapi_sweet push

ppapi_sweet:
	$(CXX) $(SOURCE) -shared -o $(TARGET) $(CFLAGS) $(LDLIBS)

push:
	cp $(TARGET) $(ELECTRON_LIBS)/

all: ppapi_sweet push

clean:
	$(RM) $(TARGET)
