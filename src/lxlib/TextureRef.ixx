module;
#include "precompiled.h"
#include "stb_image.h"

export module lxlib.TextureRef;

import lxlib.GlslProg;
import lxlib.VaoVbo;
import lxlib.Rect;

extern void drawRect();
extern glm::ivec2 windowSize;

export namespace lx {
	class Texture
	{
	public:
		class Format {
		public:
			GLint mInternalFormat = -1;
			bool mImmutableStorage = false;
			bool mMipmapping = false;
			bool mLoadTopDown = false;
			GLenum mMinFilter, mMagFilter;
			GLenum mWrapS, mWrapT, mWrapR;
		public:
			void	setInternalFormat(GLint internalFormat) { mInternalFormat = internalFormat; }
			void	setImmutableStorage(bool immutable = true) { mImmutableStorage = immutable; }
			void	enableMipmapping(bool enableMipmapping = true) { mMipmapping = enableMipmapping; }
			Format& mipmap(bool enableMipmapping = true) { mMipmapping = enableMipmapping; return *this; }
			Format& minFilter(GLenum minFilter) { mMinFilter = minFilter; return *this; }
			Format& magFilter(GLenum magFilter) { mMagFilter = magFilter; return *this; }
			Format& loadTopDown(bool loadTopDown = true) { mLoadTopDown = loadTopDown; return *this; }
			Format& wrap(GLenum wrap) { mWrapS = mWrapT = mWrapR = wrap; return *this; }
			Format& internalFormat(GLenum internalFormat) { mInternalFormat = internalFormat; return *this; }
		};
	private:
		GLuint mId;
		Format mFormat;
		int mWidth, mHeight;
		bool mTopDown = false;
	public:
		Texture(int width, int height, lx::Texture::Format const& format) {
			init(width, height, format);
		}
		Texture(std::string filePath, lx::Texture::Format const& format) {
			int width, height, n;
			filePath = "../../assets/" + filePath;
			unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &n, 4);
			if (data == nullptr)
				throw std::runtime_error("Couldn't load image");

			init(width, height, format);
			bind();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
			if (format.mMipmapping)
				glGenerateMipmap(GL_TEXTURE_2D);
		}
		~Texture() {
			glDeleteTextures(1, &mId);
		}
		void bind() {
			glBindTexture(GL_TEXTURE_2D, mId);
		}
		void bind(GLenum textureUnit)
		{
			glActiveTexture(textureUnit);
			bind();
			glActiveTexture(GL_TEXTURE0); // todo: is this necessary?
		}
		void sendParamsToGPU() {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mFormat.mMinFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mFormat.mMagFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mFormat.mWrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mFormat.mWrapT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, mFormat.mWrapR);
		}
		GLint getId() {
			return mId;
		}
		GLenum getInternalFormat() const {
			return mFormat.mInternalFormat;
		}
		int getWidth() {
			return mWidth;
		}
		int getHeight() {
			return mHeight;
		}
		glm::ivec2 getSize() {
			return glm::ivec2(mWidth, mHeight);
		}
		void setTopDown(bool topDown) {
			mTopDown = topDown;
		}
		void setMinFilter(GLenum minFilter) {
			mFormat.minFilter(minFilter);
		}
		void setMagFilter(GLenum magFilter) {
			mFormat.magFilter(magFilter);
		}
		void setWrap(GLenum wrap) {
			mFormat.wrap(wrap);
		}
		void setWrap(GLenum wrapS, GLenum wrapT) {
			mFormat.mWrapS = wrapS;
			mFormat.mWrapT = wrapT;
		}
		Format const& format() const {
			return mFormat;
		}
		GLenum getTarget() const {
			return GL_TEXTURE_2D;
		}
		static std::shared_ptr<lx::Texture> create(int width, int height, lx::Texture::Format const& format) {
			return std::make_shared<lx::Texture>(width, height, format);
		}
		static std::shared_ptr<lx::Texture> create(std::string filepath, lx::Texture::Format const& format) {
			return std::make_shared<lx::Texture>(filepath, format);
		}

	private:
		void init(int width, int height, lx::Texture::Format const& format) {
			mFormat = format;
			mWidth = width;
			mHeight = height;

			glGenTextures(1, &mId);
			bind();
			int levels;
			if (format.mMipmapping)
				levels = std::floor(std::log2(std::max(width, height))) + 1;
			else
				levels = 1;
			glTexStorage2D(GL_TEXTURE_2D, levels, format.mInternalFormat, width, height);
		}
	};

	typedef std::shared_ptr<lx::Texture> TextureRef;

	inline const std::string genericVertexShaderSource =
		"#version 150\n"
		"in vec4 ciPosition;"
		"in vec2 ciTexCoord0;"
		"out highp vec2 texCoord;"
		"uniform vec2 uTexCoordOffset, uTexCoordScale;"
		"uniform vec2 uPositionOffset, uPositionScale;"
		"void main()"
		"{"
		"\tgl_Position.xy = uPositionOffset + uPositionScale * ciPosition.xy;"
		"\tgl_Position.xy = gl_Position.xy * 2 - 1;"
		"\tgl_Position.zw = vec2(0, 1);"
		"\ttexCoord = ciTexCoord0;"
		"\ttexCoord = uTexCoordOffset + uTexCoordScale * texCoord;"
		"}";

	inline const std::string genericFragmentShaderSource =
		"#version 150\n"
		"in highp vec2 texCoord;"
		"uniform sampler2D uTex;"
		"void main()"
		"{"
		"\tgl_FragColor = texture2D(uTex, texCoord);"
		"}";

	void lxDraw(lx::TextureRef const& tex, lx::Rect<float> const& destRect) {
		glActiveTexture(GL_TEXTURE0);
		tex->bind();
		tex->sendParamsToGPU();
		static const auto glsl = std::make_shared<lx::GlslProg>(lx::genericFragmentShaderSource, lx::genericVertexShaderSource);
		glsl->bind();
		glsl->uniform("uTex", 0);
		glsl->uniform("uPositionOffset", destRect.topLeft());
		glsl->uniform("uPositionScale", destRect.size());
		glsl->uniform("uTexCoordOffset", glm::vec2(0.0f, 1.0f));
		glsl->uniform("uTexCoordScale", glm::vec2(1.0f, -1.0f));

		static std::shared_ptr<lx::QuadGpu> quad = lx::createQuadVAO_VBOs();
		quad->vao.bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		lx::VAO::unbind();
	}

	void lxDraw(lx::TextureRef const& tex) {
		auto upperLeft = glm::vec2(0, 0);
		auto bottomRight = tex->getSize();
		lx::lxDraw(tex, lx::Rect<float>::fromBounds(upperLeft, bottomRight));
	}

	void lxClear() {
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	namespace gl {
		typedef lx::TextureRef TextureRef;
		typedef lx::Texture Texture;
	}
}
