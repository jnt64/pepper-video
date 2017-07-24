FFMPEG_LIBS="libavdevice libavformat libavfilter libavcodec libswresample libswscale libavutil"

echo $(pkg-config --cflags $FFMPEG_LIBS) 
echo $(pkg-config --libs $FFMPEG_LIBS)

clang++ ./sweet_player.cpp -shared -o ppapi_sweet.so \
-I/Users/james/Technology/nacl/nacl_sdk/pepper_49/include \
$(pkg-config --cflags $FFMPEG_LIBS) \
-L/Users/james/Technology/nacl/nacl_sdk/pepper_49/lib/mac_host/Release \
-lppapi_cpp -lppapi -lppapi_gles2 -lpthread \
$(pkg-config --libs $FFMPEG_LIBS) -Wall -fPIC
