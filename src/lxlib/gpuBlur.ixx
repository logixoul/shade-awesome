module;
#include "precompiled.h"
#include "macros.h"

export module lxlib.gpuBlur;

import lxlib.shade;
import lxlib.TextureRef;
import lxlib.stuff;
import lxlib.gpgpu;

export namespace lx {
	namespace gpuBlur {
		lx::gl::TextureRef run(lx::gl::TextureRef src, int lvls);
		lx::gl::TextureRef run_longtail(lx::gl::TextureRef src, int lvls, float lvlmul, float hscale = .5f, float vscale = .5f);
		float getGaussW();
		float gauss(float f, float width);
		lx::gl::TextureRef upscale(lx::gl::TextureRef src, ivec2 toSize);
		lx::gl::TextureRef upscale(lx::gl::TextureRef src, float hscale, float vscale);
		lx::gl::TextureRef singleblur(lx::gl::TextureRef src, float hscale, float vscale, GLenum wrap = GL_CLAMP_TO_BORDER);
		std::vector<lx::gl::TextureRef> buildGaussianPyramid(lx::gl::TextureRef const& src, float scalePerLevel);
	}

	namespace gpuBlur {

  float logAB(float a, float b) {
		return log(b) / log(a);
	}

      lx::gl::TextureRef run(lx::gl::TextureRef src, int lvls) {
			auto state = lx::shade(src, "_out = lxTexture();");

		for (int i = 0; i < lvls; i++) {
			state = singleblur(state, .5, .5);
		}
         state = lx::gpuBlur::upscale(state, src->getSize());
		return state;
	}

       lx::gl::TextureRef run_longtail(lx::gl::TextureRef src, int lvls, float lvlmul, float hscale, float vscale) {
		vector<float> weights;
		float sumw = 0.0f;
		int maxHLvls = floor(logAB(1.0f / hscale, (float)src->getWidth()));
		int maxVLvls = floor(logAB(1.0f / vscale, (float)src->getHeight()));
		int maxLvls = std::min(maxHLvls, maxVLvls);
		lvls = std::min(lvls, maxLvls);
		for (int i = 0; i < lvls; i++) {
			float w = pow(lvlmul, float(i));
			w += 1.0f;
			weights.push_back(w);
			sumw += w;
		}
		for (float& w : weights) {
			w /= sumw;
		}
          vector<lx::gl::TextureRef> zoomstates;
		zoomstates.push_back(src);
            zoomstates[0] = lx::shade(zoomstates[0],
			"_out = lxTexture().xyz * _mul;",
              lx::ShadeOpts().uniform("_mul", 1.0f / sumw));
		for (int i = 0; i < lvls; i++) {
              auto newZoomstate = lx::gpuBlur::singleblur(zoomstates[i], hscale, vscale);
			zoomstates.push_back(newZoomstate);
			if (newZoomstate->getWidth() < 1 || newZoomstate->getHeight() < 1) throw runtime_error("too many blur levels");
		}
		for (int i = lvls - 1; i > 0; i--) {
               auto upscaled = lx::gpuBlur::upscale(zoomstates[i], zoomstates[i - 1]->getSize());
			float w = pow(lvlmul, float(i)); // tmp copypaste
		  zoomstates[i - 1] = lx::shade({ zoomstates[i - 1], upscaled },
			  "vec4 acc = lxTexture(tex0);"
				"vec4 nextzoom = lxTexture(tex1);"
				"vec4 c = acc + nextzoom * _mul;"
				"_out = c;"
                , lx::ShadeOpts().uniform("_mul", w)
			);
		}
		return zoomstates[0];
	}

	float getGaussW() {
		// default value determined by trial and error
		return 0.75f;
	}

	float gauss(float f, float width) {
		return exp(-f * f / (width*width));
	}

      lx::gl::TextureRef upscale(lx::gl::TextureRef src, ivec2 toSize) {
			return lx::gpuBlur::upscale(src, float(toSize.x) / src->getWidth(), float(toSize.y) / src->getHeight());
	}

        lx::gl::TextureRef upscale(lx::gl::TextureRef src, float hscale, float vscale) {
		string lib =
			"float gauss(float f, float width) {"
			"	return exp(-f*f/(width*width));"
			"}"
			;
		string shader =
			"	float gaussW = 0.75f;"
			"	vec2 offset = vec2(GB2_offsetX, GB2_offsetY);"
          "	vec2 texSize = vec2(textureSize(tex0, 0));"
         // here texCoord2 is half a texel TO THE TOP LEFT of the texel center. IT IS IN UV SPACE.
			"	vec2 texCoord2 = floor(texCoord * texSize) / texSize;"
			// here I make texCoord2 be the texel center
          "	texCoord2 += texelSize0 / 2.0;"
			// frXY is in PIXEL SPACE. its x and y go from -.5 to .5
          "	vec2 frXY = (texCoord - texCoord2) * texSize;"
			"	float fr = (GB2_offsetX == 1.0) ? frXY.x : frXY.y;"
              "	vec4 aM1 = texture(tex0, texCoord2 + (-1.0) * offset * texelSize0);"
			"	vec4 a0 = texture(tex0, texCoord2 + (0.0) * offset * texelSize0);"
			"	vec4 aP1 = texture(tex0, texCoord2 + (+1.0) * offset * texelSize0);"
			"	"
			"	float wM1=gauss(-1.0-fr, gaussW);"
			"	float w0=gauss(-fr, gaussW);"
			"	float wP1=gauss(1.0-fr, gaussW);"
			"	float sum=wM1+w0+wP1;"
			"	wM1/=sum;"
			"	w0/=sum;"
			"	wP1/=sum;"
			"	_out = wM1*aM1 + w0*a0 + wP1*aP1;";
      lx::setWrapBlack(src);
           auto hscaled = lx::shade(src, shader,
				lx::ShadeOpts()
				.scale(hscale, 1.0f)
				.uniform("GB2_offsetX", 1.0f)
				.uniform("GB2_offsetY", 0.0f)
				.functions(lib)
			);
      lx::setWrapBlack(hscaled);
           auto vscaled = lx::shade(hscaled, shader,
				lx::ShadeOpts()
				.scale(1.0f, vscale)
				.uniform("GB2_offsetX", 0.0f)
				.uniform("GB2_offsetY", 1.0f)
				.functions(lib)
			);
		return vscaled;
	}

        lx::gl::TextureRef singleblur(lx::gl::TextureRef src, float hscale, float vscale, GLenum wrap) {
		GPU_SCOPE("singleblur");
		float gaussW = getGaussW();
		float w0 = gauss(0.0, gaussW);
		float w1 = gauss(1.0, gaussW);
		float w2 = gauss(2.0, gaussW);
		float sum = 2.0f*(w1+w2) + w0;
		w2 /= sum;
		w1 /= sum;
		w0 /= sum;
		std::stringstream weights;
		weights << fixed << "float w0=" << w0 << ", w1=" << w1 << ", w2=" << w2 << ";" << endl;
		string shader =
			"vec2 offset = vec2(GB2_offsetX, GB2_offsetY);"
              "vec4 aM2 = texture(tex0, texCoord + (-2.0) * offset * texelSize0);"
			"vec4 aM1 = texture(tex0, texCoord + (-1.0) * offset * texelSize0);"
			"vec4 a0 = texture(tex0, texCoord + (0.0) * offset * texelSize0);"
			"vec4 aP1 = texture(tex0, texCoord + (+1.0) * offset * texelSize0);"
			"vec4 aP2 = texture(tex0, texCoord + (+2.0) * offset * texelSize0);"
			""
			+ weights.str() +
			"_out = w2 * (aM2 + aP2) + w1 * (aM1 + aP1) + w0 * a0;";

     lx::setWrap(src, wrap);
           auto hscaled = lx::shade(src, shader,
				lx::ShadeOpts()
				.scale(hscale, 1.0f)
				.uniform("GB2_offsetX", 1.0f)
				.uniform("GB2_offsetY", 0.0f)
			);
     lx::setWrap(hscaled, wrap);
           auto vscaled = lx::shade(hscaled, shader,
				lx::ShadeOpts()
			.uniform("GB2_offsetX", 0.0f)
			.uniform("GB2_offsetY", 1.0f)
			.scale(1.0f, vscale));
		return vscaled;
	}

      std::vector<lx::gl::TextureRef> buildGaussianPyramid(lx::gl::TextureRef const& src, float scalePerLevel) {
			std::vector<lx::gl::TextureRef> result;
		result.push_back(src);
		auto state = src;
		while (true) {
			int minDim = std::min(state->getWidth(), state->getHeight());
			if (minDim <= 2)
				break;
			ivec2 dstSize = ivec2(state->getWidth() * scalePerLevel, state->getHeight() * scalePerLevel);
            state = lx::gpuBlur::singleblur(state, scalePerLevel, scalePerLevel, GL_CLAMP_TO_BORDER);
			result.push_back(state);
		}
		return result;
	}
	}
}
