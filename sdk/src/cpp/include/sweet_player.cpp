#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"

#include "GLES2/gl2.h"

// FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/avstring.h>
#include <libavutil/mathematics.h>
}

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

// C Includes
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#define AVIO_BUFFER_SIZE 8192

#define SWEET_ASSERT_THROW(expression, message)                  \
    if (!(expression))                                           \
    {                                                            \
        std::stringstream ss;                                    \
        ss << message << " : " << __FILE__ << " : " << __LINE__; \
        throw std::runtime_error(ss.str());                      \
    }                                                    

#define SWEET_ASSERT_PPGL_THROW(ppOpenGLES2Ptr, ppGraphics3DPtr)        \
    if (ppOpenGLES2Ptr->GetError(ppGraphics3DPtr->pp_resource()))       \
    {                                                                   \
        std::stringstream ss;                                           \
        ss << "PP_OpenGLES2 Error : " << __FILE__ << " : " << __LINE__; \
        throw std::runtime_error(ss.str());                             \
    }  

#define SWEET_ASSERT_GL_THROW()                                    \
    if (glGetError())                                             \
    {                                                              \
        std::stringstream ss;                                      \
        ss << "!glGetError() : " << __FILE__ << " : " << __LINE__; \
        throw std::runtime_error(ss.str());                        \
    }                                                                   

namespace {

class SweetPepperInstance : public pp::Instance,
                            public pp::Graphics3DClient 
{
public:
             SweetPepperInstance(PP_Instance instance, pp::Module* module);
    virtual ~SweetPepperInstance();

    // Override pp::Instance implementation //
    virtual void DidChangeView(const pp::Rect& position,
                               const pp::Rect& clip_ignored);

    // Override pp::Graphics3DClient //
    virtual void Graphics3DContextLost();
    virtual bool HandleInputEvent(const pp::InputEvent& event);

private:

    void   blank              (int32_t result);
    GLuint createGlProgramYuv ();
    GLuint createGlShader     (GLuint program, GLenum type, const char* source);
    GLuint createGlTexture    (int32_t width, int32_t height, int unit);
    void   initFFmpeg         ();
    void   initGL             (int32_t result);
    void   paint              (int32_t result, bool paint_blue);
    void   paintYuv           (int32_t result);
    void   printStats         ();

    pp::CompletionCallbackFactory<SweetPepperInstance> mCompletionCallbackFactory;

    // FFmpeg Methods
    static int     readFunction (void* opaque, uint8_t* buffer, int bufferSize);
    static int64_t seekFunction (void* opaque, int64_t pos    , int whence);


    bool                 mFullscreen;
    pp::Size             mPluginSize;


    // Pepper
    const PPB_OpenGLES2 *mPPBOpenGLES2Ptr;
    pp::Graphics3D      *mPPGraphics3DPrt;
    pp::Module          *mPPModulePrt;

    // GLES2
    GLuint mGlBuffer;
    GLuint mGlProgramYuv;
    GLuint mGlTextureY;
    GLuint mGlTextureU;
    GLuint mGlTextureV;

    // FFmpeg
    AVFormatContext*   mpFormatCtx;
    AVCodecContext*    mpCodecCtxOrig;
    AVCodecContext*    mpCodecCtx;
    AVCodec*           mpCodec;
    AVFrame*           mpFrame;
    int                mVideoStream;

    // FFmpeg Custom AVIO
    std::ifstream                mIfstream;
    std::shared_ptr<AVIOContext> mspAvioContext;
    std::shared_ptr<uint8_t>     mspBuffer;

    //unsigned           mI;
    AVPacket           mPacket;
    int                mFrameFinished;
    uint8_t*           mpYPlane;
    uint8_t*           mpUPlane;
    uint8_t*           mpVPlane;
    size_t             mYPlaneSz;
    size_t             mUvPlaneSz;
    int                mUvPitch;
    struct SwsContext* mpSwsCtx;
};

SweetPepperInstance::SweetPepperInstance (PP_Instance instance, pp::Module* module)
    : pp::Instance              (instance),
      pp::Graphics3DClient      (this),
      mCompletionCallbackFactory(this),
      mFullscreen               (false),
      mPPBOpenGLES2Ptr          (static_cast<const PPB_OpenGLES2*>(module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE))),
      mPPGraphics3DPrt          (NULL),
      mPPModulePrt              (module),
      mGlBuffer                 (0),
      mGlProgramYuv             (0),
      mGlTextureY               (0),
      mGlTextureU               (0),
      mGlTextureV               (0),
      mpFormatCtx               (NULL),
      mpCodecCtxOrig            (NULL),
      mpCodec                   (NULL),
      mpFrame                   (NULL),
      mpSwsCtx                  (NULL)
{
    SWEET_ASSERT_THROW(glInitializePPAPI(pp::Module::Get()->get_browser_interface()), "!glInitializePPAPI");

    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_KEYBOARD);
}

SweetPepperInstance::~SweetPepperInstance () 
{
    delete mPPGraphics3DPrt;
}

void SweetPepperInstance::DidChangeView (const pp::Rect& position, const pp::Rect& clip_ignored)
{
    if (position.width() == 0 || position.height() == 0) 
    {
        return;
    }

    this->mPluginSize = position.size();

    std::cout << "width: " << this->mPluginSize.width() << " height: " << this->mPluginSize.height() << std::endl;
  
    // Initialize graphics.
    this->initFFmpeg();
    this->initGL(0);
}

//-- Overrides --------------------------------------------------------------------------------------------//

void SweetPepperInstance::Graphics3DContextLost () 
{
    std::cout << "Graphics3DContextLost" << std::endl;

    delete mPPGraphics3DPrt; mPPGraphics3DPrt = NULL;

    pp::CompletionCallback callback = mCompletionCallbackFactory.NewCallback(&SweetPepperInstance::initGL);
    
    mPPModulePrt->core()->CallOnMainThread(0, callback, 0);
}

bool SweetPepperInstance::HandleInputEvent (const pp::InputEvent& event)
{    
    if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEUP) 
    {
        mFullscreen = !mFullscreen;
        // pp::Fullscreen(this).SetFullscreen(mFullscreen);

        this->paintYuv(0);
    }

    if (event.GetType() == PP_INPUTEVENT_TYPE_KEYUP) 
    {
        mFullscreen = !mFullscreen;
        // pp::Fullscreen(this).SetFullscreen(mFullscreen);

        this->paintYuv(0);
    }

    return true;
}

//-- Private Instance Members --------------------------------------------------------------------------------//

GLuint SweetPepperInstance::createGlProgramYuv ()
{
    // Code and constants for shader.
    static const char kVertexShader[] =
        "varying vec2 v_texCoord;            \n"
        "attribute vec4 a_position;          \n"
        "attribute vec2 a_texCoord;          \n"
        "void main()                         \n"
        "{                                   \n"
        "    v_texCoord = a_texCoord;        \n"
        "    gl_Position = a_position;       \n"
        "}";

    static const char kFragmentShaderYUV[] =
        "precision mediump float;                                   \n"
        "varying vec2 v_texCoord;                                   \n"
        "uniform sampler2D y_texture;                               \n"
        "uniform sampler2D u_texture;                               \n"
        "uniform sampler2D v_texture;                               \n"
        "uniform mat3 color_matrix;                                 \n"
        "void main()                                                \n"
        "{                                                          \n"
        "  vec3 yuv;                                                \n"
        "  yuv.x = texture2D(y_texture, v_texCoord).r;              \n"
        "  yuv.y = texture2D(u_texture, v_texCoord).r;              \n"
        "  yuv.z = texture2D(v_texture, v_texCoord).r;              \n"
        "  vec3 rgb = color_matrix * (yuv - vec3(0.0625, 0.5, 0.5));\n"
        "  gl_FragColor = vec4(rgb, 1.0);                           \n"
        "}";


    GLuint glProgramYuv = glCreateProgram();
    
    this->createGlShader(glProgramYuv, GL_VERTEX_SHADER  , kVertexShader);
    this->createGlShader(glProgramYuv, GL_FRAGMENT_SHADER, kFragmentShaderYUV);

    glLinkProgram(glProgramYuv);

    SWEET_ASSERT_GL_THROW();

    return glProgramYuv;
}

GLuint SweetPepperInstance::createGlShader (GLuint program, GLenum type, const char* source) 
{
    GLuint shader = glCreateShader(type);
    GLint  length = static_cast<GLint>(strlen(source) + 1);
    
    glShaderSource (shader, 1, &source, &length);
    glCompileShader(shader);
    glAttachShader (program, shader);
    glDeleteShader (shader);

    return shader;
}

GLuint SweetPepperInstance::createGlTexture (int32_t width, int32_t height, int unit)
{
    GLuint textureId;
    glGenTextures(1, &textureId); // Since we need three textures we should create 3 here.

    SWEET_ASSERT_GL_THROW();

    // Assign parameters.
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture  (GL_TEXTURE_2D, textureId);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S    , GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T    , GL_CLAMP_TO_EDGE);
    
    // Allocate texture.
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 width,
                 height,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, 
                 NULL);

    SWEET_ASSERT_GL_THROW();

    return textureId;
}

void SweetPepperInstance::initGL (int32_t result) 
{
    SWEET_ASSERT_THROW(mPluginSize.width() && mPluginSize.height(), "Need non zero width and height.");

    delete mPPGraphics3DPrt;

    int32_t attributes[] = {
        PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 0,
        PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
        PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
        PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
        PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
        PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
        PP_GRAPHICS3DATTRIB_SAMPLES, 0,
        PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
        PP_GRAPHICS3DATTRIB_WIDTH, mPluginSize.width(),
        PP_GRAPHICS3DATTRIB_HEIGHT, mPluginSize.height(),
        PP_GRAPHICS3DATTRIB_NONE,
    };

    mPPGraphics3DPrt = new pp::Graphics3D(this, attributes);
    
    SWEET_ASSERT_THROW(!mPPGraphics3DPrt->is_null(), "mPPGraphics3DPrt is_null().");

    glSetCurrentContextPPAPI(mPPGraphics3DPrt->pp_resource());

    // Set viewport window size and clear color bit.
    glClearColor(0.2, 0.2, 0.2, 0);
    glClear     (GL_COLOR_BUFFER_BIT);
    glViewport  (0, 0, mPluginSize.width(), mPluginSize.height()); // Here we need to get the ratio of plugin size to video and set the viewport.

    BindGraphics(*mPPGraphics3DPrt);
    SWEET_ASSERT_GL_THROW();

    this->mGlProgramYuv = this->createGlProgramYuv();

    // this->mGlTextureY = this->createGlTexture(mPluginSize.width()    , mPluginSize.height()    , 0);
    // this->mGlTextureU = this->createGlTexture(mPluginSize.width() / 2, mPluginSize.height() / 2, 1);
    // this->mGlTextureV = this->createGlTexture(mPluginSize.width() / 2, mPluginSize.height() / 2, 2);

    this->mGlTextureY = this->createGlTexture(mpCodecCtx->width    , mpCodecCtx->height    , 0);
    this->mGlTextureU = this->createGlTexture(mpCodecCtx->width / 2, mpCodecCtx->height / 2, 1);
    this->mGlTextureV = this->createGlTexture(mpCodecCtx->width / 2, mpCodecCtx->height / 2, 2);


  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 1, 1, 1, -1,  // Position coordinates.
    //0, 1, 0, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
    //0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
  };

    // // Assign vertex positions and texture coordinates to buffers for use in
    // // shader program.
    // static const float kVertices[] = {
    //     -1, 1, -1, -1, 0, 1, 0, -1,  // Position coordinates.
    //      0, 1,  0, -1, 1, 1, 1, -1,  // Position coordinates.
    //      0, 0,  0,  1, 1, 0, 1,  1,  // Texture coordinates.
    //      0, 0,  0,  1, 1, 0, 1,  1,  // Texture coordinates.
    // };

    glGenBuffers(1, &this->mGlBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, this->mGlBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
    
    SWEET_ASSERT_GL_THROW();

    this->printStats();
}

// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE

int64_t SweetPepperInstance::seekFunction (void* opaque, int64_t pos, int whence)
{
    std::ifstream *stream = reinterpret_cast<std::ifstream *>(opaque);

    switch (whence) {
        
        case AVSEEK_SIZE:
            std::cout << "seekFunction  pos: " << pos << "  whence: " << "AVSEEK_SIZE  return: -1" << std::endl;
            return -1;

        case SEEK_SET:
            stream->seekg(pos, std::ios::beg);
            std::cout << "seekFunction  pos: " << pos << "  whence: " << "SEEK_SET"; 
            break;
            
        case SEEK_CUR:
            stream->seekg(pos, std::ios::cur);
            std::cout << "seekFunction  pos: " << pos << "  whence: " << "SEEK_CUR"; 
            break;

        case SEEK_END:
            stream->seekg(pos, std::ios::end);
            std::cout << "seekFunction  pos: " << pos << "  whence: " << "SEEK_END"; 
            break;
    }
    
    std::cout << "  return: " << stream->tellg() << std::endl;

    return stream->tellg();
}

int SweetPepperInstance::readFunction (void* opaque, uint8_t* buffer, int bufferSize) 
{
    std::ifstream *stream = reinterpret_cast<std::ifstream*>(opaque);

    std::cout << "readFunction  " << "  tellg: " << stream->tellg();

    stream->read(reinterpret_cast<char*>(buffer), bufferSize);
    
    std::cout << "  bufferSize: " << bufferSize << "  return: " << stream->gcount() << "  tellg: " << stream->tellg() << std::endl;

    if (stream->eof()) {
        stream->clear();
    }

    return stream->gcount();
}

void SweetPepperInstance::initFFmpeg () 
{
    // Register all formats and codecs
    av_register_all();

    //const char *videoFilePath = "/Users/jamesdoran/Development/synergy/media/frag/26ccb43b-2da0-4ffd-70c7-3bd5adcbd408-asset/26ccb43b-2da0-4ffd-70c7-3bd5adcbd408.1-Video1.2298.frag";
    //const char *videoFilePath = "/Users/james/Synergy/sandbox/media/basketball/150831121.mp4";
    //const char *videoFilePath = "/Users/james/Desktop/Untitled.mov";
    const char *videoFilePath = "/Users/jamesdoran/Desktop/zero-dollar-cart.mov";

    // TEST CUSTOM AVIO
    mIfstream.open(videoFilePath, std::ios::binary);

    mspBuffer = std::shared_ptr<uint8_t>
        (reinterpret_cast<uint8_t*>(av_malloc(AVIO_BUFFER_SIZE)), &av_free);

    mspAvioContext = std::shared_ptr<AVIOContext>
        (avio_alloc_context(
            mspBuffer.get(),
            AVIO_BUFFER_SIZE,
            0,
            reinterpret_cast<void*>(&mIfstream),
            &SweetPepperInstance::readFunction,
            nullptr,
            &SweetPepperInstance::seekFunction
        ), &av_free);

    //const auto avFormat    = std::shared_ptr<AVFormatContext>(avformat_alloc_context(), &avformat_free_context);
    //auto       avFormatPtr = avFormat.get();

    mpFormatCtx     = avformat_alloc_context();
    mpFormatCtx->pb = mspAvioContext.get();

    uint32_t probeBufferSize = AVIO_BUFFER_SIZE;
    //uint8_t*  probeBuffer[40960];

    std::cout << "HERE" << std::endl;

    // Probe the input format
    uint32_t probeReadBytes = 0;
    int64_t  seekPosition   = 0;
    probeReadBytes = SweetPepperInstance::readFunction(&mIfstream, mspBuffer.get(), probeBufferSize);
    seekPosition   = SweetPepperInstance::seekFunction(&mIfstream, 0, SEEK_SET);

    std::cout << "probeReadBytes:" << probeReadBytes << std::endl;
    std::cout << "seekPosition:"   << seekPosition   << std::endl;

    std::cout << "HERE2" << std::endl;

    // Now we set the ProbeData-structure for av_probe_input_format:
    AVProbeData probeData;

    probeData.buf      = mspBuffer.get();
    probeData.buf_size = AVIO_BUFFER_SIZE;
    probeData.filename = "Untitled.mov";

    std::cout << "HERE3" << std::endl;
    
    // Pass in an AVDictionary with hint
    //SWEET_ASSERT_THROW(avformat_open_input(&mpFormatCtx, "", nullptr, nullptr) == 0, "Could not open input.");
    std::cout << "HERE4" << std::endl;

    int probeScore = 0;

    AVInputFormat *pAvInputFormat = NULL;

    probeScore = av_probe_input_buffer2(mspAvioContext.get(), &pAvInputFormat, videoFilePath, NULL, 0, AVIO_BUFFER_SIZE);

    // Determine the input-format:
    mpFormatCtx->flags   = AVFMT_FLAG_CUSTOM_IO;
    //mpFormatCtx->iformat = av_probe_input_format3(&probeData, 0, &probeScore);
    mpFormatCtx->iformat = pAvInputFormat;

    std::cout << "probeScore: " << probeScore << ", AVPROBE_SCORE_MAX / 4: " << (AVPROBE_SCORE_MAX / 4) << std::endl;

    //avformat_find_stream_info
    SWEET_ASSERT_THROW(avformat_open_input(&mpFormatCtx, ""/*videoFilePath*/, NULL/*pAvInputFormat*/, NULL) == 0, "Could not open input.");

    // END TEST CUSTOM AVIO

    // Open video file
    //SWEET_ASSERT_THROW(avformat_open_input(&mpFormatCtx, videoFilePath, NULL, NULL) == 0, "Could not open input.");

    std::cout << "HERE5" << std::endl;

    // AVDictionary *options = NULL; // This is cheating and needs to be fixed. We want to find the pixel format, not set it.
    // av_dict_set(&options, "pixel_format", "yuv420p", 0); // Not sure why this works from read file and not avio.

    // Retrieve stream information
    SWEET_ASSERT_THROW(avformat_find_stream_info(mpFormatCtx, NULL/*&options*/) >= 0, "Could not find stream info.");

    // Try setting the options for frag files.
    AVDictionary *options = NULL; // This is cheating and needs to be fixed. We want to find the pixel format, not set it.
    av_dict_set(&options, "pixel_format", "yuv420p", 0); // Not sure why this works from read file and not avio.
    av_dict_set(&options, "video_size", "1280x720", 0); // Not sure why this works from read file and not avio.

    /// Try setting the options for frag files. ^^^
    SWEET_ASSERT_THROW(avformat_find_stream_info(mpFormatCtx, NULL/*&options*/) >= 0, "Could not find stream info.");

    std::cout << "HERE6" << std::endl;

    // Dump information about file onto standard error
    av_dump_format(mpFormatCtx, 0, videoFilePath, 0);


    // Find the first video stream
    mVideoStream = -1;
    for (unsigned int i = 0; i < mpFormatCtx->nb_streams; i++)
        if (mpFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            mVideoStream = i;
            break;
        }

    SWEET_ASSERT_THROW(mVideoStream != -1, "Could not find video stream.")

    // Get a pointer to the codec context for the video stream
    mpCodecCtxOrig = mpFormatCtx->streams[mVideoStream]->codec;
    
    // Find the decoder for the video stream
    mpCodec = avcodec_find_decoder(mpCodecCtxOrig->codec_id);
    SWEET_ASSERT_THROW(mpCodec != NULL, "Could not find codec.")

    // Copy context
    mpCodecCtx = avcodec_alloc_context3(mpCodec);
    SWEET_ASSERT_THROW(avcodec_copy_context(mpCodecCtx, mpCodecCtxOrig) == 0, "Could not copy codec context");

    // Open codec
    SWEET_ASSERT_THROW(avcodec_open2(mpCodecCtx, mpCodec, NULL) >= 0, "Could not open codec.");

    // Allocate video frame
    mpFrame = av_frame_alloc();  // TODO Recycle the Frame

    std::cout << "HERE 8" << std::endl;

    std::cout << "mpCodecCtx->pix_fmt: " << mpCodecCtx->pix_fmt << std::endl; // This is unknown.

    //initialize SWS context for software scaling (YUV Conversion)
    mpSwsCtx = sws_getContext(
        mpCodecCtx->width,
        mpCodecCtx->height,
        mpCodecCtx->pix_fmt,
        mpCodecCtx->width,
        mpCodecCtx->height,
        AV_PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

    std::cout << "HERE 9" << std::endl;

    // set up YV12 pixel array (12 bits per pixel)
    mYPlaneSz  = mpCodecCtx->width * mpCodecCtx->height;
    mUvPlaneSz = mpCodecCtx->width * mpCodecCtx->height / 4;

    mpYPlane = (uint8_t*)malloc(mYPlaneSz);
    mpUPlane = (uint8_t*)malloc(mUvPlaneSz);
    mpVPlane = (uint8_t*)malloc(mUvPlaneSz);
    
    SWEET_ASSERT_THROW(mpYPlane && mpUPlane && mpVPlane, "Could not allocate pixel buffers");

    mUvPitch = mpCodecCtx->width / 2;
}

void SweetPepperInstance::printStats () 
{
    GLint maxTextureImageUnits;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);

    GLint maxCombinedTextureImageUnits;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTextureImageUnits);

    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    // GLint maxArrayTextureLayers;
    // glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);

    //GL_COMPRESSED_TEXTURE_FORMATS


    GLboolean shaderCompilerAvailable;
    glGetBooleanv(GL_SHADER_COMPILER, &shaderCompilerAvailable);
    
    pid_t pid  = getpid();
    pid_t ppid = getppid();

    std::cout << "Shader Compiler Available: " << (shaderCompilerAvailable ? "y" : "n") << std::endl;
    std::cout << "Texture Units            : " << maxTextureImageUnits                  << std::endl;
    std::cout << "Combined Texture Units   : " << maxCombinedTextureImageUnits          << std::endl;
    std::cout << "Max Texture Size         : " << maxTextureSize                        << std::endl;
    std::cout << "Pid                      : " << pid                                   << std::endl;
    std::cout << "Parent Pid               : " << ppid                                  << std::endl;
}

void SweetPepperInstance::paintYuv (int32_t result)
{
    // FFmpeg

    std::cout << "beforeSeek " << std::endl;

    int seekResponse = av_seek_frame(mpFormatCtx, mVideoStream, 24400, 0);

    std::cout << "seekResponse " << seekResponse << std::endl;

    bool finished = av_read_frame(mpFormatCtx, &mPacket) < 0;

    static int i = 0;

    std::cout << "frame " << ++i << std::endl;

    std::cout << "packet.pts: "      << mPacket.pts << std::endl;
    std::cout << "packet.pos: "      << mPacket.pos << std::endl;
    std::cout << "packet.duration: " << mPacket.duration << std::endl;


    if (finished) {
        std::cout << "av_read_frame finished" << std::endl;
        return;
    }

    // return this->paintYuv(0);

    // Is this a packet from the video stream?
    if (mPacket.stream_index != mVideoStream) {
        //std::cout << "packet not from desired videoIndex" << std::endl;
        return this->paintYuv(0);
    }    
    // Decode video frame
    avcodec_decode_video2(mpCodecCtx, mpFrame, &mFrameFinished, &mPacket);

    // Did we get a video frame? // First we will software scale it. // But later we need to let OpenGLES2 do it.
    if (!mFrameFinished) {
        //std::cout << "did not get a video frame" << std::endl;
        return this->paintYuv(0);
    }

    AVPicture pict;

    pict.data[0]     = mpYPlane;
    pict.data[1]     = mpUPlane;
    pict.data[2]     = mpVPlane;
    pict.linesize[0] = mpCodecCtx->width;
    pict.linesize[1] = mUvPitch;
    pict.linesize[2] = mUvPitch;

    // Convert the image into YUV format that SDL uses
    // sws_scale(
    //     mpSwsCtx,
    //     (uint8_t const * const *) mpFrame->data,
    //     mpFrame->linesize,
    //     0,
    //     mpCodecCtx->height,
    //     pict.data,
    //     pict.linesize
    // );

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&mPacket);

    // // Free the YUV frame
    // av_frame_free(&pFrame);
    // free(yPlane);
    // free(uPlane);
    // free(vPlane);

    // // Close the codec
    // avcodec_close(pCodecCtx);
    // avcodec_close(pCodecCtxOrig);

    // // Close the video file
    // avformat_close_input(&pFormatCtx);

    // return 0;

    // FFmpeg


    // static int i = 0;

    // if (i == 223) {
    //     i = 0;
    // }

    // std::stringstream ss;
    // ss << "/Users/james/Desktop/yuv/yuv-640x480-" << ++i << ".data";

    // std::ifstream input(ss.str(), std::ios::binary);

    // // copies all data into buffer
    // std::vector<char> buffer((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));
    // input.close();
    
    // char *data = buffer.data();

    // int width  = 640;
    // int height = 480;

    int width  = mpCodecCtx->width;
    int height = mpCodecCtx->height;

    // std::cout << "mpCodecCtx->width :" << mpCodecCtx->width  << std::endl;
    // std::cout << "mpCodecCtx->height:" << mpCodecCtx->height << std::endl;

    glActiveTexture(GL_TEXTURE0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, /*data*//*pict.data[0]*/mpFrame->data[0]);

    glGenerateMipmap(GL_TEXTURE_2D); // Software Scaling can also be used. Profile this.


    //data += width * height;
    width /= 2;
    height /= 2;

    glActiveTexture(GL_TEXTURE1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, /*data*//*pict.data[1]*/mpFrame->data[1]);

    glGenerateMipmap(GL_TEXTURE_2D); // Software Scaling can also be used. Profile this.

    //data += width * height;
    glActiveTexture(GL_TEXTURE2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, /*data*//*pict.data[2]*/mpFrame->data[2]);

    glGenerateMipmap(GL_TEXTURE_2D); // Software Scaling can also be used. Profile this.


    // Perhaps instead of 3 texture units it would be best to convert to rgb in software. Not sure. Needs profiling.


    // Draw YUV Here

    static const float kColorMatrix[9] = {
        1.1643828125f, 1.1643828125f, 1.1643828125f,
        0.0f, -0.39176171875f, 2.017234375f,
        1.59602734375f, -0.81296875f, 0.0f
    };

    glUseProgram(mGlProgramYuv);
    
    glUniform1i(glGetUniformLocation(mGlProgramYuv, "y_texture"), 0);
    glUniform1i(glGetUniformLocation(mGlProgramYuv, "u_texture"), 1);
    glUniform1i(glGetUniformLocation(mGlProgramYuv, "v_texture"), 2);

    glUniformMatrix3fv(
        glGetUniformLocation(mGlProgramYuv, "color_matrix"),
        1,
        GL_FALSE,
        kColorMatrix
    );

    SWEET_ASSERT_GL_THROW();

    GLint pos_location = glGetAttribLocation(mGlProgramYuv, "a_position");
    GLint tc_location  = glGetAttribLocation(mGlProgramYuv, "a_texCoord");

    SWEET_ASSERT_GL_THROW();

    glEnableVertexAttribArray(pos_location);
    glVertexAttribPointer    (pos_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(tc_location);
    glVertexAttribPointer    (tc_location, 2, GL_FLOAT, GL_FALSE, 0, static_cast<float*>(0) + 8);  // Skip position coordinates. kVertices from above.

    SWEET_ASSERT_GL_THROW();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SWEET_ASSERT_GL_THROW();

    pp::CompletionCallback cb = mCompletionCallbackFactory.NewCallback(&SweetPepperInstance::paintYuv);
    
    mPPGraphics3DPrt->SwapBuffers(cb);
}

void SweetPepperInstance::blank(int32_t result)
{
}
 
void SweetPepperInstance::paint (int32_t result, bool paint_blue) 
{
    if (result != 0 || !mPPGraphics3DPrt)
    {
        return;
    }
  
    float r = paint_blue ? 0 : 1;
    float g = 0;
    float b = paint_blue ? 1 : 0;
    float a = 0.75;
  
    mPPBOpenGLES2Ptr->ClearColor(mPPGraphics3DPrt->pp_resource(), r, g, b, a);
    mPPBOpenGLES2Ptr->Clear(mPPGraphics3DPrt->pp_resource(), GL_COLOR_BUFFER_BIT);
  
    SWEET_ASSERT_PPGL_THROW(mPPBOpenGLES2Ptr, mPPGraphics3DPrt);

    pp::CompletionCallback callback = mCompletionCallbackFactory.NewCallback(&SweetPepperInstance::paint, !paint_blue);
  
    mPPGraphics3DPrt->SwapBuffers(callback);
    
    SWEET_ASSERT_PPGL_THROW(mPPBOpenGLES2Ptr, mPPGraphics3DPrt);
}

// Plugin Global Module
class SweetPepperModule : public pp::Module {

public:
             SweetPepperModule() : pp::Module() {}
    virtual ~SweetPepperModule() {}

    virtual pp::Instance* CreateInstance(PP_Instance instance) {
        return new SweetPepperInstance(instance, this);
    }
};

}  // anonymous namespace

namespace pp {

Module* CreateModule () {
  return new SweetPepperModule();
}

}  // namespace pp
