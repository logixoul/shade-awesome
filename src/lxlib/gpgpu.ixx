module;
#include "precompiled.h"
#include "macros.h"

export module lxlib.gpgpu;

import lxlib.shade;

import lxlib.TextureRef;
import lxlib.stuff;

export namespace lx {
	lx::gl::TextureRef getGradients(lx::gl::TextureRef src, GLuint wrap = GL_REPEAT);
	lx::gl::TextureRef gaussianBlur3x3(lx::gl::TextureRef src);
	lx::gl::TextureRef getLaplace(lx::gl::TextureRef src, GLuint wrap);

	struct Operable {
		explicit Operable(lx::gl::TextureRef aTex);
		lx::Operable operator+(lx::gl::TextureRef other);
		lx::Operable operator-(lx::gl::TextureRef other);
		lx::Operable operator*(lx::gl::TextureRef other);
		lx::Operable operator/(lx::gl::TextureRef other);
		lx::Operable operator+(float scalar) {
			return lx::Operable(lx::shade(tex, "_out = lxTexture() + scalar;", lx::ShadeOpts().uniform("scalar", scalar)));
		}
		lx::Operable operator-(float scalar) {
			return lx::Operable(lx::shade(tex, "_out = lxTexture() - scalar;", lx::ShadeOpts().uniform("scalar", scalar)));
		}
		lx::Operable operator*(float scalar) {
			return lx::Operable(lx::shade(tex, "_out = lxTexture() * scalar;", lx::ShadeOpts().uniform("scalar", scalar)));
		}
		lx::Operable operator/(float scalar) {
			return lx::Operable(lx::shade(tex, "_out = lxTexture() / scalar;", lx::ShadeOpts().uniform("scalar", scalar)));
		}
		void operator+=(lx::gl::TextureRef other);
		void operator-=(lx::gl::TextureRef other);
		void operator*=(lx::gl::TextureRef other);
		void operator/=(lx::gl::TextureRef other);
		float dot(lx::gl::TextureRef other);
		lx::gl::TextureRef dotTex(lx::gl::TextureRef other);
		operator lx::gl::TextureRef();
	private:
		lx::gl::TextureRef tex;
	};

	lx::Operable op(lx::gl::TextureRef tex);

	lx::gl::TextureRef getGradients(lx::gl::TextureRef src, GLuint wrap) {
	GPU_SCOPE("getGradients");
	glActiveTexture(GL_TEXTURE0);
	src->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
       return lx::shade(src,
        "	float srcL=texture(tex0,texCoord+texelSize0*vec2(-1.0,0.0)).x;"
			"	float srcR=texture(tex0,texCoord+texelSize0*vec2(1.0,0.0)).x;"
			"	float srcT=texture(tex0,texCoord+texelSize0*vec2(0.0,-1.0)).x;"
			"	float srcB=texture(tex0,texCoord+texelSize0*vec2(0.0,1.0)).x;"
		"	float dx=(srcR-srcL)/2.0;"
		"	float dy=(srcB-srcT)/2.0;"
		"	_out.xy=vec2(dx,dy);"
		,
          lx::ShadeOpts().ifmt(GL_RG16F)
	);
   }

    lx::gl::TextureRef gaussianBlur3x3(lx::gl::TextureRef src) {
		auto state = lx::shade(src,
		"vec4 sum = vec4(0.0);"
              "sum += texture(tex0, texCoord + texelSize0 * vec2(-1.0, -1.0)) / 16.0;"
			"sum += texture(tex0, texCoord + texelSize0 * vec2(-1.0, 0.0)) / 8.0;"
			"sum += texture(tex0, texCoord + texelSize0 * vec2(-1.0, +1.0)) / 16.0;"

            "sum += texture(tex0, texCoord + texelSize0 * vec2(0.0, -1.0)) / 8.0;"
			"sum += texture(tex0, texCoord + texelSize0 * vec2(0.0, 0.0)) / 4.0;"
			"sum += texture(tex0, texCoord + texelSize0 * vec2(0.0, +1.0)) / 8.0;"

              "sum += texture(tex0, texCoord + texelSize0 * vec2(+1.0, -1.0)) / 16.0;"
			"sum += texture(tex0, texCoord + texelSize0 * vec2(+1.0, 0.0)) / 8.0;"
			"sum += texture(tex0, texCoord + texelSize0 * vec2(+1.0, +1.0)) / 16.0;"
		"_out = sum;"
      );
		return state;
	}

    lx::gl::TextureRef getLaplace(lx::gl::TextureRef src, GLuint wrap) {
	glActiveTexture(GL_TEXTURE0);
	src->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
     auto state = lx::shade(src,
		"vec4 sum = vec4(0.0);"
       "sum += texture(tex0, texCoord + texelSize0 * vec2(-1.0, 0.0)) * -1.0;"
		"sum += texture(tex0, texCoord + texelSize0 * vec2(0.0, -1.0)) * -1.0;"
		"sum += texture(tex0, texCoord + texelSize0 * vec2(0.0, +1.0)) * -1.0;"
		"sum += texture(tex0, texCoord + texelSize0 * vec2(+1.0, 0.0)) * -1.0;"
		"sum += texture(tex0, texCoord + texelSize0 * vec2(0.0, 0.0)) * 4.0;"
		"_out = -sum;"
      );
		return state;
	}

   lx::Operable op(lx::gl::TextureRef tex) {
		return lx::Operable(tex);
	}

    inline Operable::Operable(lx::gl::TextureRef aTex) {
		tex = aTex;
	}

    lx::Operable Operable::operator+(lx::gl::TextureRef other) {
		return lx::Operable(lx::shade({ tex, other }, "_out = lxTexture() + lxTexture(tex1);"));
	}
	lx::Operable Operable::operator-(lx::gl::TextureRef other) {
		return lx::Operable(lx::shade({ tex, other }, "_out = lxTexture() - lxTexture(tex1);"));
	}
	lx::Operable Operable::operator*(lx::gl::TextureRef other) {
		return lx::Operable(lx::shade({ tex, other }, "_out = lxTexture() * lxTexture(tex1);"));
	}
	lx::Operable Operable::operator/(lx::gl::TextureRef other) {
		return lx::Operable(lx::shade({ tex, other }, "_out = lxTexture() / lxTexture(tex1);"));
	}

   void Operable::operator+=(lx::gl::TextureRef other) {
		tex = lx::shade({ tex, other }, "_out = lxTexture() + lxTexture(tex1);");
	}
	void Operable::operator-=(lx::gl::TextureRef other) {
		tex = lx::shade({ tex, other }, "_out = lxTexture() - lxTexture(tex1);");
	}
	void Operable::operator*=(lx::gl::TextureRef other) {
		tex = lx::shade({ tex, other }, "_out = lxTexture() * lxTexture(tex1);");
	}
	void Operable::operator/=(lx::gl::TextureRef other) {
		tex = lx::shade({ tex, other }, "_out = lxTexture() / lxTexture(tex1);");
	}

   Operable::operator lx::gl::TextureRef() {
		return tex;
	}
}
