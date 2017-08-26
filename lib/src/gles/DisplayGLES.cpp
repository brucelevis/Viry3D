/*
* Viry3D
* Copyright 2014-2017 by Stack - stackos@qq.com
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#if VR_GLES

#include "DisplayGLES.h"
#include "gles_include.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Shader.h"
#include "graphics/XMLShader.h"
#include "graphics/Graphics.h"
#include "memory/ByteBuffer.h"
#include "memory/Memory.h"
#include "io/File.h"
#include "thread/Thread.h"
#include "time/Time.h"
#include "Debug.h"

#if VR_WINDOWS
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}
#endif

#if VR_ANDROID
#include "android/jni.h"
#endif

namespace Viry3D
{
	class DisplayGLESPrivate
	{
	public:
		DisplayGLESPrivate()
		{
#if VR_WINDOWS
			video_out_context = NULL;
			video_out_stream = NULL;
			video_codec_context = NULL;
			video_frame = NULL;
			input_frame = NULL;
			yuv_convert_context = NULL;
			record_begin_frame = -1;
#endif
		}

#if VR_WINDOWS
		Ref<Thread> record_thread;
		AVFormatContext* video_out_context;
		AVStream* video_out_stream;
		AVCodecContext* video_codec_context;
		AVFrame* video_frame;
		AVFrame* input_frame;
		SwsContext* yuv_convert_context;
		int record_begin_frame;
#endif
	};

	DisplayGLES::DisplayGLES()
	{
		m_private = RefMake<DisplayGLESPrivate>();
	}

	void DisplayGLES::Init(int width, int height, int fps)
	{
#if VR_IOS
		DisplayIOS::Init(width, height, fps);
#elif VR_ANDROID
		EGLInit(width, height);

		DisplayAndroid::Init(width, height, fps);

		m_display = eglGetCurrentDisplay();
		m_context = eglGetCurrentContext();
		m_default_depth_render_buffer = 0;

		Log("current display: %d context: %d", m_display, m_context);
#elif VR_WINDOWS
		DisplayWindows::Init(width, height, fps);

		m_default_depth_render_buffer = 0;

		glewInit();
#endif

		glClearDepthf(1.0f);
		glClearStencil(0);
		glFrontFace(GL_CCW);
		glEnable(GL_DEPTH_TEST);

		auto vender = (char *) glGetString(GL_VENDOR);
		auto renderer = (char *) glGetString(GL_RENDERER);
		auto version = (char *) glGetString(GL_VERSION);
		m_device_name = String::Format("%s/%s/%s", vender, renderer, version);
		m_extensions = (char *) glGetString(GL_EXTENSIONS);

		GLint max_vertex_uniform_vectors;
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vertex_uniform_vectors);
		GLint max_uniform_block_size;
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_block_size);
		GLint uniform_buffer_offset_alignment;
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniform_buffer_offset_alignment);

		m_uniform_buffer_offset_alignment = (int) uniform_buffer_offset_alignment;

		Log("device_name: %s", m_device_name.CString());
		Log("extensions: %s", m_extensions.CString());
		Log("max_vertex_uniform_vectors:%d", max_vertex_uniform_vectors);
		Log("max_uniform_block_size:%d", max_uniform_block_size);
		Log("uniform_buffer_offset_alignment:%d", uniform_buffer_offset_alignment);

		LogGLError();
	}

	void DisplayGLES::OnResize(int width, int height)
	{
#if VR_ANDROID
		EGLPause();
		EGLResume();
#endif

		m_width = width;
		m_height = height;
	}

	void DisplayGLES::OnPause()
	{
#if VR_ANDROID
		EGLPause();
#endif
	}

	void DisplayGLES::OnResume()
	{
#if VR_ANDROID
		EGLResume();
#endif
	}

#if VR_ANDROID
	void DisplayGLES::EGLInit(int& width, int& height)
	{
		auto window = (EGLNativeWindowType) get_native_window();

		EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		eglInitialize(display, NULL, NULL);

		const EGLint configAttribs[] = {
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_DEPTH_SIZE, 24,
			EGL_NONE
		};
		EGLint configCount;
		eglChooseConfig(display, configAttribs, NULL, 0, &configCount);

		int configIndex = -1;
		EGLConfig* configs = new EGLConfig[configCount];
		eglChooseConfig(display, configAttribs, configs, configCount, &configCount);

		for (int i = 0; i < configCount; ++i)
		{
			auto& cfg = configs[i];
			EGLint r, g, b, d;
			if (eglGetConfigAttrib(display, cfg, EGL_RED_SIZE, &r) &&
				eglGetConfigAttrib(display, cfg, EGL_GREEN_SIZE, &g) &&
				eglGetConfigAttrib(display, cfg, EGL_BLUE_SIZE, &b) &&
				eglGetConfigAttrib(display, cfg, EGL_DEPTH_SIZE, &d) &&
				r == 8 && g == 8 && b == 8 && d == 24)
			{

				configIndex = i;
				break;
			}
		}
		m_config = configs[configIndex];
		delete[] configs;

		EGLint contextAttribs[] = {
			EGL_CONTEXT_CLIENT_VERSION, 3,
			EGL_NONE
		};
		EGLContext context = eglCreateContext(display, m_config, NULL, contextAttribs);
		EGLSurface surface = eglCreateWindowSurface(display, m_config, window, NULL);

		eglMakeCurrent(display, surface, surface, context);

		eglQuerySurface(display, surface, EGL_WIDTH, &width);
		eglQuerySurface(display, surface, EGL_HEIGHT, &height);

		m_display = display;
		m_surface = surface;
		m_context = context;

		Log("EGL Surface Width: %d Height:%d", width, height);
	}

	void DisplayGLES::EGLDeinit()
	{
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(m_display, m_context);
		if (m_surface != EGL_NO_SURFACE)
		{
			eglDestroySurface(m_display, m_surface);
		}
		eglTerminate(m_display);
	}

	void DisplayGLES::EGLPause()
	{
		eglDestroySurface(m_display, m_surface);
		m_surface = EGL_NO_SURFACE;
	}

	void DisplayGLES::EGLResume()
	{
		auto window = (EGLNativeWindowType) get_native_window();
		m_surface = eglCreateWindowSurface(m_display, m_config, window, NULL);
		eglMakeCurrent(m_display, m_surface, m_surface, m_context);
	}

	void DisplayGLES::CreateSharedContext()
	{
		const EGLint attrib_list[] = {
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_DEPTH_SIZE, 24,
			EGL_NONE
		};
		EGLConfig config;
		EGLint num_config;
		auto success = eglChooseConfig(m_display, attrib_list, &config, 1, &num_config);
		if (success)
		{
			const EGLint context_attrib_list[] = {
				EGL_CONTEXT_CLIENT_VERSION, 3,
				EGL_NONE
			};
			auto context = eglCreateContext(m_display, config, m_context, context_attrib_list);
			auto error = eglGetError();

			Log("CreateSharedContext: %d %d %d", m_context, context, error);

			success = eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
			Log("CurrentSharedContext: %d", success);
		}
	}

	void DisplayGLES::DestroySharedContext()
	{
		auto context = eglGetCurrentContext();
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(m_display, context);

		Log("DestroySharedContext: %d", context);
	}
#endif

#if VR_WINDOWS
	void DisplayGLES::CreateSharedContext()
	{
		wglMakeCurrent(m_hdc, m_shared_context);
	}

	void DisplayGLES::DestroySharedContext()
	{
		wglMakeCurrent(NULL, NULL);
	}
#endif

#if VR_ANDROID || VR_WINDOWS
	int DisplayGLES::GetDefualtDepthRenderBuffer()
	{
		if (m_default_depth_render_buffer == 0)
		{
			glGenRenderbuffers(1, &m_default_depth_render_buffer);
			glBindRenderbuffer(GL_RENDERBUFFER, m_default_depth_render_buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}

		return m_default_depth_render_buffer;
	}
#endif

	void DisplayGLES::FlushContext()
	{
		LogGLError();
		glFlush();
		LogGLError();
	}

	void DisplayGLES::SwapBuffers()
	{
		if (IsRecording())
		{
			RecordBuffer();
		}

#if VR_ANDROID
		eglSwapBuffers(m_display, m_surface);
#elif VR_WINDOWS
		::SwapBuffers(m_hdc);
#endif
	}

	void DisplayGLES::Deinit()
	{
#if VR_ANDROID || VR_WINDOWS
		if (m_default_depth_render_buffer == 0)
		{
			glDeleteRenderbuffers(1, &m_default_depth_render_buffer);
		}
#endif

#if VR_IOS
		DisplayIOS::Deinit();
#elif VR_ANDROID
		EGLDeinit();
		DisplayAndroid::Deinit();
#elif VR_WINDOWS
		DisplayWindows::Deinit();
#endif
	}

	void DisplayGLES::BindVertexBuffer(const VertexBuffer* buffer)
	{
		LogGLError();

		glBindBuffer(GL_ARRAY_BUFFER, buffer->GetBuffer());

		LogGLError();
	}

	void DisplayGLES::BindIndexBuffer(const IndexBuffer* buffer, IndexType::Enum index_type)
	{
		LogGLError();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->GetBuffer());

		LogGLError();
	}

	void DisplayGLES::BindVertexArray(const Ref<Shader>& shader, int pass_index)
	{
		LogGLError();

		auto vs = shader->GetVertexShaderInfo(pass_index);
		for (const auto& i : vs->attrs)
		{
			glEnableVertexAttribArray(i.location);
			glVertexAttribPointer(i.location, i.size / 4, GL_FLOAT, GL_FALSE, vs->stride, (const GLvoid*) i.offset);
		}

		LogGLError();
	}

	void DisplayGLES::DrawIndexed(int start, int count, IndexType::Enum index_type)
	{
		LogGLError();

		GLenum type;
		int type_size;
		if (index_type == IndexType::UnsignedShort)
		{
			type = GL_UNSIGNED_SHORT;
			type_size = 2;
		}
		else
		{
			type = GL_UNSIGNED_INT;
			type_size = 4;
		}

		glDrawElements(GL_TRIANGLES, count, type, (const GLvoid*) (start * type_size));

		Graphics::draw_call++;

		LogGLError();
	}

	void DisplayGLES::BeginRecord(const String& file)
	{
		if (IsRecording())
		{
			return;
		}

		DisplayBase::BeginRecord(file);

#if VR_WINDOWS
		m_fps = 30;

		m_private->record_begin_frame = Time::GetFrameCount();

		m_private->record_thread = RefMake<Thread>(0, ThreadInfo({
			[=]() {
			av_register_all();

			AVFormatContext* oc;
			auto ret = avformat_alloc_output_context2(&oc, NULL, NULL, file.CString());
			auto codec_id = oc->oformat->video_codec;

			auto stream = avformat_new_stream(oc, NULL);
			stream->id = oc->nb_streams - 1;
			stream->time_base = { 1, m_fps };

			auto c = stream->codec;
			c->qmin = 1;
			c->qmax = 50;
			c->qcompress = 1;
			c->gop_size = 12; /* emit one intra frame every twelve frames at most */
			c->bit_rate = 4000 * m_fps / 30 * 1000;
			c->pix_fmt = AV_PIX_FMT_YUV420P;
			c->codec_type = AVMEDIA_TYPE_VIDEO;
			c->codec_id = codec_id;
			c->width = m_width;
			c->height = m_height;
			c->time_base = stream->time_base;

			auto codec = avcodec_find_encoder(codec_id);
			ret = avcodec_open2(c, codec, NULL);

			auto frame = av_frame_alloc();
			frame->format = c->pix_fmt;
			frame->width = c->width;
			frame->height = c->height;
			ret = av_frame_get_buffer(frame, 32);

			auto input_frame = av_frame_alloc();
			input_frame->format = AV_PIX_FMT_RGB24;
			input_frame->width = c->width;
			input_frame->height = c->height;
			ret = av_frame_get_buffer(input_frame, 32);

			auto yuv_convert_context = sws_getContext(
				c->width, c->height,
				AV_PIX_FMT_RGB24,
				c->width, c->height,
				c->pix_fmt,
				AV_PIX_FMT_YUV420P, NULL, NULL, NULL);

			ret = avio_open(&oc->pb, file.CString(), AVIO_FLAG_WRITE);
			ret = avformat_write_header(oc, NULL);

			m_private->video_out_context = oc;
			m_private->video_out_stream = stream;
			m_private->video_codec_context = c;
			m_private->video_frame = frame;
			m_private->input_frame = input_frame;
			m_private->yuv_convert_context = yuv_convert_context;
		},
			[=]() {
			av_write_trailer(m_private->video_out_context);

			avcodec_close(m_private->video_codec_context);
			av_frame_free(&m_private->video_frame);
			avio_closep(&m_private->video_out_context->pb);
			avformat_free_context(m_private->video_out_context);
			av_frame_free(&m_private->input_frame);
			sws_freeContext(m_private->yuv_convert_context);

			m_private->video_out_context = NULL;
			m_private->video_out_stream = NULL;
			m_private->video_codec_context = NULL;
			m_private->video_frame = NULL;
			m_private->input_frame = NULL;
			m_private->yuv_convert_context = NULL;
		}
		}));
#endif
	}

	void DisplayGLES::EndRecord()
	{
		DisplayBase::EndRecord();

#if VR_WINDOWS
		m_private->record_thread.reset();
		m_private->record_begin_frame = -1;
#endif
	}

	void DisplayGLES::RecordBuffer()
	{
#if VR_WINDOWS
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		ByteBuffer buffer(m_width * m_height * 3);
		glReadPixels(0, 0, m_width, m_height, GL_RGB, GL_UNSIGNED_BYTE, buffer.Bytes());

		int frame_index = Time::GetFrameCount() - m_private->record_begin_frame;

		const int task_max = 30;
		int task_count;
		while ((task_count = m_private->record_thread->QueueLength()) >= task_max)
		{
			Thread::Sleep(10);
		}

		m_private->record_thread->AddTask({ [=]() {
			Ref<Any> empty;

			byte* input = buffer.Bytes();

			auto oc = m_private->video_out_context;
			auto stream = m_private->video_out_stream;
			auto c = m_private->video_codec_context;
			auto output_frame = m_private->video_frame;
			auto input_frame = m_private->input_frame;
			auto yuv_convert_context = m_private->yuv_convert_context;

			auto ret = av_frame_make_writable(input_frame);

			for (int i = 0; i < m_height; i++)
			{
				for (int j = 0; j < m_width; j++)
				{
					byte r = input[(m_height - i - 1) * m_width * 3 + j * 3];
					byte g = input[(m_height - i - 1) * m_width * 3 + j * 3 + 1];
					byte b = input[(m_height - i - 1) * m_width * 3 + j * 3 + 2];

					input_frame->data[0][i * input_frame->linesize[0] + j * 3 + 0] = r;
					input_frame->data[0][i * input_frame->linesize[0] + j * 3 + 1] = g;
					input_frame->data[0][i * input_frame->linesize[0] + j * 3 + 2] = b;
				}
			}

			sws_scale(yuv_convert_context, input_frame->data, input_frame->linesize,
				0, c->height, output_frame->data, output_frame->linesize);

			output_frame->pts = frame_index;

			AVPacket pkt;
			pkt.data = NULL;
			pkt.size = 0;
			av_init_packet(&pkt);

			int got_packet;
			ret = avcodec_encode_video2(c, &pkt, output_frame, &got_packet);

			if (got_packet)
			{
				av_packet_rescale_ts(&pkt, c->time_base, stream->time_base);
				pkt.stream_index = stream->index;
				ret = av_interleaved_write_frame(oc, &pkt);
			}

			return empty;
		}, NULL });
#endif
	}
}

#endif
