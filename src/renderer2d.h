#pragma once

#include <chrono>

#include "context.h"
#include "render_target.h"
#include "texture/texture2d.h"
#include "tools.h"

#ifdef ANDROID
#include "texture/texture_buffer_android.h"
#endif

namespace gl {

#ifdef ANDROID
	template<typename T, int C>
	using texture_dma = texture_buffer_android<T, C>;
#else
	//template<typename T, int C>
	//using texture_dma = texture2d<T, C>;

	template<typename T, int C>
	class texture_dma : public texture2d<T, C> {
	public:
		texture_dma(const std::string name, int width, int height, gl::buf_use use)
				: texture2d<T, C>(name, width, height) {}
	};
#endif

	template<typename T, int C>
	class renderer2d {
	public:
		struct texture_bind {
			texture_bind(const std::string &uName, texture_base &tex) : tex(tex), uName(uName) {}

			texture_base &tex;
			std::string uName;
			int uLoc = -1;
		};

		gl::context gl;


	private:
		// render thread & sync
		std::thread renderThread;
		std::atomic<bool> alive;
		bool ready = false, rendered = false;
		std::condition_variable cv;
		std::mutex mtx;
		uint32_t frameIndex = -1;

		// shader program
		GLuint prog = 0;
		std::string fragShaderSrc;
		std::string watchedFragFileName;
		bool usingFallbackFragment = false;

		// input
		std::vector<texture_bind> inputBindings;
		std::function<void(void)> updateTexturesExternal;

		// output
		bool accumulate = false;
		texture_bind *feedbackInput = nullptr;
		render_target<T,C> target, target2;
		std::atomic<T *> readBuffer;
		std::function<void(const T *src)> *reader = nullptr;

		const char *vShaderSrc = R"GLSL(#version 300 es
layout(location = 0) in mediump vec2 position;
void main() {gl_Position = vec4(position, 0.0, 1.0);}
)GLSL";

		const char *fErrorShaderSrc = R"GLSL(#version 300 es
out mediump vec4 color; void main() {
ivec2 size = ivec3(WIDTH, HEIGHT);
ivec2 pos = ivec2(gl_FragCoord.xy);
color = vec4((pos.x == pos.y) ? 200 : 0, 0,0, (pos.x == (size.y-pos.y)) ? 200 : 0);
}
)GLSL";

		void compileShaders() {
			LOG_V << "compileShaders()... ";
			if (prog != 0) glDeleteProgram(prog);

			glsl_processor processor;
			processor.d("$width", width()), processor.d("$height", height());
			processor.d("WIDTH", width()), processor.d("HEIGHT", height());

			auto vertexShader = gl.LoadShader(GL_VERTEX_SHADER, vShaderSrc);
			auto fragmentShader = gl.LoadShader(GL_FRAGMENT_SHADER, processor.process(fragShaderSrc));
			prog = gl.Link({vertexShader, fragmentShader});
			glDeleteShader(vertexShader), glDeleteShader(fragmentShader);
			gl.checkAndThrowError("compileShaders(): glDeleteShader");

			std::string().swap(fragShaderSrc); // free
		}

	public:
		explicit renderer2d(int width, int height, const std::string &fragShaderSrc, context &gl)
				: fragShaderSrc(fragShaderSrc), gl(gl),
				  alive(false), readBuffer(nullptr), target(width, height), target2(width, height) {
		}

		~renderer2d() {
			{
				std::lock_guard<std::mutex> lk(mtx);
				alive = false,	ready = true;
			}
			cv.notify_one();
			renderThread.join();
		}

		inline int width() { return target.width(); }

		inline int height() { return target.height(); }

		void renderThreadBody(Stopwatch *sw) {
			gl.initForThisThread();

			target.init();
			if (feedbackInput) target2.init();

			compileShadersOrFallback();

			glActiveTexture(GL_TEXTURE0);
			for (texture_bind &tb : inputBindings) {
				tb.tex.init();
			}

			cv.notify_one(); // init done

			bool watchFragFile = watchedFragFileName.length() > 0;
			auto tFragFileStat = std::chrono::steady_clock::now();
			int64_t fragShaderMtime = watchFragFile ? tools::fileModTime(watchedFragFileName)													: 0;

			if (watchFragFile) {
				LOG_I << "Watching fragment shader file (" << watchedFragFileName << ")";
			}

			clear();

			// make sure we dont bring any errors into the render loop
			gl.checkAndThrowError("startBackgroundRenderThread(): post-init error state");

			while (true) {
				std::unique_lock<std::mutex> lk(mtx);
				cv.wait(lk, [this] { return ready; });

				if (!alive)
					break;

				auto now = std::chrono::steady_clock::now();
				using namespace std::chrono_literals;

				if (watchFragFile && (now - tFragFileStat) > 1s) {
					tFragFileStat = now;
					auto m = tools::fileModTime(watchedFragFileName);
					if (m != fragShaderMtime) {
						LOG_I << "Fragement shader file " << watchedFragFileName
							  << " changed. Updating shader program...";
						std::this_thread::sleep_for(10ms);
						fragShaderSrc = tools::readFile(watchedFragFileName);
						while (!compileShadersOrFallback()) {
							std::this_thread::sleep_for(2s);
							fragShaderSrc = tools::readFile(watchedFragFileName);
						}
						fragShaderMtime = m;
						if (sw) sw->clear();
					}
				}

				// render or just read
				if (readBuffer != nullptr) {
					getRenderTarget().read(readBuffer, width() * height() * C);
				} else if (reader != nullptr) {
					getRenderTarget().read(*reader, width() * height() * C);
				} else {
					render_internal(sw);
				}

				ready = false, rendered = true;
				lk.unlock();
				cv.notify_one();
			}

			gl.deinitForThisThread();
			//LOG_D << "renderer2d: render thread ended";
		}


		void startBackgroundRenderThread(Stopwatch *sw = nullptr) {
			if (alive)
				throw std::logic_error("renderer already alive");
			alive = true;

			renderThread = std::thread([this, sw]() {
				try { renderThreadBody(sw); }
				catch (std::runtime_error &e) {
					LOG_E << "renderer2d: exception in render thread: " << e.what();
				}
			});

			{   // block during init
				std::unique_lock<std::mutex> lk(mtx);
				cv.wait(lk);
			}
		}

		void enableAccumulation(bool enable = true) {
			accumulate = enable;
			frameIndex = -1;
		}

		void enableFeedback(const std::string &feedbackUniformName) {
			if (alive)
				throw std::logic_error("renderer already alive");
			inputBindings.push_back({feedbackUniformName, target2.texBuf});
			feedbackInput = &inputBindings.back();
		}

		render_target<T,C> &getRenderTarget(bool alt = false) {
			return feedbackInput && (frameIndex % 2) == (alt ? 0 : 1) ? target2 : target;
		}

		texture_base &getOutputTexture(bool alt = false) {
			return getRenderTarget(alt).texBuf;
		}

		void watchFragmentShaderFile(const std::string &fn) {
			watchedFragFileName = fn;
			if (fragShaderSrc.empty())
				fragShaderSrc = tools::readFile(fn);
		}

		void setFragmentShader(const std::string &glsl) {
			fragShaderSrc = glsl;
			compileShaders();
		}

		void addInput(const std::string &name, texture_base &tex) {
			if (alive)
				throw std::logic_error("cannot add input texture, renderer already alive");

			inputBindings.push_back({name, tex});
		}

		void render() {
			readBuffer = 0;
			notifyRenderThread();
		}

		void getResult(T *buffer) {
			readBuffer = buffer;
			notifyRenderThread();
			readBuffer = nullptr;
		}

		void getResult(std::function<void(const T *src)> &read) {
			reader = &read;
			notifyRenderThread();
			reader = nullptr;
		}

		void getResult(HlBuf<T> &buffer) {
			if (buffer.number_of_elements() != width() * height() * C)
				throw std::runtime_error("getResult(): invalid buffer size");
			getResult(buffer.begin());
		}

		inline bool isAlive() { return alive; }

		void setExternalTextureUpdateCallback(std::function<void(void)> &callback) {
			updateTexturesExternal = callback;
		}

	private:

		bool compileShadersOrFallback() {
			try {
				for (texture_bind &tb : inputBindings)
					tb.uLoc = -1;

				compileShaders();

				bool allFound = true;
				for (texture_bind &ib : inputBindings) {
					ib.uLoc = glGetUniformLocation(prog, ib.uName.c_str());
					gl.checkAndThrowError("glGetUniformLocation " + ib.uName);
					if (ib.uLoc == -1) {
						LOG_W << "shader uniform " << ib.uName
							  << " not found/optimized out, ignore";
						allFound = false;
						if (feedbackInput && &ib == feedbackInput)
							throw std::runtime_error(
									"feedback uniform " + ib.uName + " optimized out");
					}
				}
				if (allFound) {
					LOG_V << "All " << inputBindings.size() << " textures bound to uniforms";
				}
				return true;
			}
			catch (std::runtime_error &ex) {
				if (watchedFragFileName.empty())
					throw ex;
				LOG_E << "Failed to update shader program: " << ex.what()
					  << ", falling back to error fragment";
				fragShaderSrc = std::string(fErrorShaderSrc), compileShaders();
				return false;
			}
		}

		void notifyRenderThread() {
			if (!alive)
				throw std::logic_error("renderer is not alive");

			{
				std::lock_guard<std::mutex> lk(mtx);
				rendered = false;
				ready = true;
			}
			cv.notify_one();
			{
				using namespace std::chrono_literals;
				std::unique_lock<std::mutex> lk(mtx);
				if (!cv.wait_for(lk, 4s, [this] { return rendered; })) {
					LOG_E << "Timed out waiting for render thread after 4s";
					throw std::runtime_error("render thread not responding");
				}
			}
		}


		void clear() {
			auto &target(getRenderTarget(false));
			target.clear();
		}

		void render_internal(Stopwatch *sw) {
			if (sw) sw->start();
			const float l = -1.0f, r = 1.0f;
			static khronos_float_t vPositions[] = {
					l, l, r, l, r, r,
					r, r, l, r, l, l
			};

			if (eglGetCurrentContext() == EGL_NO_CONTEXT)
				throw std::runtime_error("no EGL context");

			++frameIndex;

			unsigned int vertexPosBuffer;
			glGenBuffers(1, &vertexPosBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, vertexPosBuffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vPositions), vPositions, GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glUseProgram(prog);

			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, vertexPosBuffer);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			if (sw) sw->measure("glVBuf");

			if (feedbackInput) {
				feedbackInput->tex = getOutputTexture(true);
				LOG_I << "gl::renderer2d(frame=" << frameIndex << "): feedback input is "
					  << feedbackInput->tex.to_string();
			}

			if (updateTexturesExternal) {
				updateTexturesExternal();
				if (sw) sw->measure("updateTexturesExternal");
			}

			gl.checkAndThrowError("render(): texture_bind");


			gl.checkAndThrowError("render(): glBindFramebuffer");
			auto &target(getRenderTarget(false));

			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			if (accumulate) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glBlendEquation(GL_FUNC_ADD);
				gl.checkAndThrowError("render(): glBlend*");

				if (frameIndex == 0) {
					clear();
				}
			} else {
				glDisable(GL_BLEND);
			}

			// attach the output texture
			target.bind(true);
			if (sw) sw->measure("glFBO");


			glViewport(0, 0, target.width(), target.height());
			if (sw) sw->measure("glPrep");

			int i = 0;
			for (texture_bind &tb : inputBindings) {
				if (tb.uLoc == -1)
					continue;
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(tb.tex.getGlTarget(), tb.tex.getGlId());
				tb.tex._preRender();
				gl.checkAndThrowError("render(): glBindTexture", tb.tex);

				glUniform1i(tb.uLoc, i);
				LOG_V << "bound texture " << tb.tex.to_string() << " to uniform " << tb.uName
					  << " (unit GL_TEXTURE" << i << ", target " << tb.tex.getGlTargetString()
					  << ")";
				++i;

				if (sw) sw->measure("ulTex_" + tb.tex.name);
			}
			if (sw) sw->measure("texBind");

			glDrawArrays(GL_TRIANGLES, 0, 6);
			if (sw) sw->measure("glDrawArrays");
		}
	};
}