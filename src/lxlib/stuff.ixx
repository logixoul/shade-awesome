module;
#include "precompiled.h"

#if defined(_WIN32)
#include <float.h>
#elif defined(__SSE__)
#include <xmmintrin.h>
#endif

export module lxlib.stuff;

//import lxlib.shade;

import lxlib.TextureRef;
import lxlib.Array2D;
import lxlib.TextureCache;

export namespace lx {
  lx::gl::TextureRef uploadTex(lx::Array2D<float> a);
	lx::gl::TextureRef uploadTex(lx::Array2D<vec2> a);
	lx::gl::TextureRef uploadTex(lx::Array2D<vec3> a);
	lx::gl::TextureRef uploadTex(lx::Array2D<lx::bytevec3> a);
	lx::gl::TextureRef uploadTex(lx::Array2D<vec4> a);
	lx::gl::TextureRef uploadTex(lx::Array2D<uvec4> a);
	lx::gl::TextureRef uploadTex(glm::ivec2 size, GLint internalFormat, GLenum format, GLenum type, void* data);

	int sign(float f);
	float expRange(float x, float min, float max);

	template<class T>
  lx::Array2D<T> downloadTex(lx::gl::TextureRef tex) {
		return lx::Array2D<T>();
	}

	template<class T>
  lx::Array2D<T> gettexdata(lx::gl::TextureRef tex, GLenum format, GLenum type) {
		lx::Array2D<T> data(tex->getSize());

		tex->bind();
		glGetTexImage(GL_TEXTURE_2D, 0, format, type, data.data());

		return data;
	}

  template<> lx::Array2D<lx::bytevec3> downloadTex<lx::bytevec3>(lx::gl::TextureRef tex);
	template<> lx::Array2D<float> downloadTex<float>(lx::gl::TextureRef tex);
	template<> lx::Array2D<vec2> downloadTex<vec2>(lx::gl::TextureRef tex);
	template<> lx::Array2D<vec3> downloadTex<vec3>(lx::gl::TextureRef tex);
	template<> lx::Array2D<vec4> downloadTex<vec4>(lx::gl::TextureRef tex);

	float sq(float f) {
		return f * f;
	}

  void setWrapBlack(lx::gl::TextureRef tex);
	void setWrap(lx::gl::TextureRef tex, GLenum wrap);

	class FileCache {
	public:
		static string get(string filename);
	};

	void disableGLReadClamp();
	void enableDenormalFlushToZero();

	template<class TVec>
	TVec safeNormalized(TVec const& vec) {
		typename TVec::value_type len = length(vec);
		if (len == 0.0f) {
			return vec;
		}
		return vec / len;
	}

	unsigned int ilog2(unsigned int val);
	vec2 compdiv(vec2 const& v1, vec2 const& v2);
	void enableGlDebugOutput();
  void beginRTT(lx::gl::TextureRef fbotex);
	void beginRTT(vector<lx::gl::TextureRef> fbotexs);
	void endRTT();

	const float pi = 3.14159265f;

	float randFloat();
	float randFloat(float min, float max);
}

// --- Implementations ---

// tried to have this as a static member (with thread_local) but I got errors. todo.
   /*thread_local*/ static std::map<string,string> FileCache_db;

static std::string readFile(const std::string& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Failed to open file");
	}

	file.seekg(0, std::ios::end);
	std::size_t size = file.tellg();
	file.seekg(0);

	std::string buffer(size, '\0');
	file.read((char*)buffer.data(), size);

  return buffer;
}

string lx::FileCache::get(string filename) {
	if(FileCache_db.find(filename)== FileCache_db.end()) {
		FileCache_db[filename]=readFile("../../assets/"+filename);
	}
	return FileCache_db[filename];
}

static void APIENTRY messageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	if (type == GL_DEBUG_TYPE_PUSH_GROUP || type == GL_DEBUG_TYPE_POP_GROUP)
		return;

	cout << "GL CALLBACK. Msg: " << message << endl;

  if (type == GL_DEBUG_TYPE_ERROR) {
		cout << endl;
	}
}

void lx::enableGlDebugOutput() {
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(messageCallback, 0);
}

lx::gl::TextureRef lx::uploadTex(glm::ivec2 size, GLint internalFormat, GLenum format, GLenum type, void* data) {
	lx::TextureCacheKey key;
	key.ifmt = internalFormat;
	key.size = size;
	key.allocateMipmaps = false;
    lx::gl::TextureRef tex = lx::TextureCache::instance()->get(key);

	tex->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.x, size.y, format, type, data);
	return tex;
}

lx::gl::TextureRef lx::uploadTex(lx::Array2D<float> a)
{
	return uploadTex(a.size(), GL_R16F, GL_RED, GL_FLOAT, a.data());
}

lx::gl::TextureRef lx::uploadTex(lx::Array2D<vec2> a)
{
	return uploadTex(a.size(), GL_RG16F, GL_RG, GL_FLOAT, a.data());
}

lx::gl::TextureRef lx::uploadTex(lx::Array2D<vec3> a)
{
	return uploadTex(a.size(), GL_RGB16F, GL_RGB, GL_FLOAT, a.data());
}
lx::gl::TextureRef lx::uploadTex(lx::Array2D<lx::bytevec3> a)
{
	return uploadTex(a.size(), GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, a.data());
}

lx::gl::TextureRef lx::uploadTex(lx::Array2D<vec4> a)
{
	return uploadTex(a.size(), GL_RGBA16F, GL_RGBA, GL_FLOAT, a.data());
}

lx::gl::TextureRef lx::uploadTex(lx::Array2D<uvec4> a)
{
	return uploadTex(a.size(), GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, a.data());
}

int lx::sign(float f)
{
	if (f < 0)
		return -1;
	if (f > 0)
       return 1;
	return 0;
}

float lx::expRange(float x, float min, float max) {
	return exp(mix(std::log(min), std::log(max), x));
}

void lx::setWrapBlack(lx::gl::TextureRef tex) {
	// I think the border color is transparent black by default. It doesn't hurt that it is transparent.
	tex->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black);
}

void lx::setWrap(lx::gl::TextureRef tex, GLenum wrap) {
	tex->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
}

/*thread_local*/ bool fboBound = false;

void lx::beginRTT(lx::gl::TextureRef fbotex)
{
	/*thread_local*/ static unsigned int fboid = 0;

	if (fboid == 0)
	{
		glGenFramebuffers(1, &fboid);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	if (fbotex != nullptr)
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbotex->getId(), 0);
	fboBound = true;
}
void lx::beginRTT(vector<lx::gl::TextureRef> fbotexs)
{
	if (fbotexs.size() != 1)
		throw runtime_error("not implemented");

	/*thread_local*/ static unsigned int fboid = 0; // hack: a separate fbo for this case. this is brittle. todo.

	if (fboid == 0)
	{
		glGenFramebuffers(1, &fboid);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fboid);
	int i = 0;
	for (auto& fbotex : fbotexs) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, fbotex->getId(), 0);
		i++;
	}
	fboBound = true;
}
void lx::endRTT()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	fboBound = false;
}

void lx::disableGLReadClamp() {
	glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
}

void lx::enableDenormalFlushToZero() {
#if defined(_WIN32)
	_controlfp(_DN_FLUSH, _MCW_DN);
#elif defined(__SSE__)
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
}

unsigned int lx::ilog2(unsigned int val) {
	unsigned int ret = -1;
	while (val != 0) {
		val >>= 1;
		ret++;
	}
	return ret;
}

template<> lx::Array2D<lx::bytevec3> lx::downloadTex<lx::bytevec3>(lx::gl::TextureRef tex) {
	return lx::gettexdata<lx::bytevec3>(tex, GL_RGB, GL_UNSIGNED_BYTE);
}

template<> lx::Array2D<float> lx::downloadTex<float>(lx::gl::TextureRef tex) {
	return lx::gettexdata<float>(tex, GL_RED, GL_FLOAT);
}

template<> lx::Array2D<vec2> lx::downloadTex<vec2>(lx::gl::TextureRef tex) {
	return lx::gettexdata<vec2>(tex, GL_RG, GL_FLOAT);
}

template<> lx::Array2D<vec3> lx::downloadTex<vec3>(lx::gl::TextureRef tex) {
	return lx::gettexdata<vec3>(tex, GL_RGB, GL_FLOAT);
}

template<> lx::Array2D<vec4> lx::downloadTex<vec4>(lx::gl::TextureRef tex) {
	return lx::gettexdata<vec4>(tex, GL_RGBA, GL_FLOAT);
}

float lx::randFloat()
{
	return rand() / (float)RAND_MAX;
}

float lx::randFloat(float min, float max)
{
	return randFloat() * (max - min) + min;
}