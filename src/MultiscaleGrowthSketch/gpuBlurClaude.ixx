module;
#include "precompiled.h"
#include <lxlib/macros.h>

export module gpuBlurClaude;

import lxlib.shade;
import lxlib.TextureRef;
import lxlib.Array2D;
import lxlib.gpuBlur;
import lxlib.gpgpu;
import lxlib.stuff;

export namespace gpuBlurClaude {
 lx::Array2D<float> singleblurLikeCinder(lx::Array2D<float> src, ivec2 dstSize);
	lx::gl::TextureRef singleblurLikeCinder(lx::gl::TextureRef src, ivec2 dstSize);
	std::vector<lx::gl::TextureRef> buildGaussianPyramid(lx::gl::TextureRef const& src, float scalePerLevel = 0.5f);
	lx::gl::TextureRef blurWithInvKernel(lx::gl::TextureRef const& src);
}

namespace gpuBlurClaude {
	// todo: move this to stuff.cpp/h. Copy-pasted in gpuBlur.cpp as well.
    void setTextureBorderColor(lx::gl::TextureRef tex, float r, float g, float b, float a) {
		tex->bind();
		float color[] = { r, g, b, a };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
	}
    lx::gl::TextureRef gtexF32(lx::Array2D<float> a)
	{
        return lx::uploadTex(a.size(), GL_R32F, GL_RED, GL_FLOAT, a.data());
	}
    lx::Array2D<float> singleblurLikeCinder(lx::Array2D<float> src, ivec2 dstSize) {
     return lx::downloadTex<float>(singleblurLikeCinder(gtexF32(src), dstSize));
	}
    lx::gl::TextureRef singleblurLikeCinder(lx::gl::TextureRef src, ivec2 dstSize) {
		GPU_SCOPE("singleblur");
		float hscale = float(dstSize.x) / src->getWidth();
		float vscale = float(dstSize.y) / src->getHeight();
		
		string shaderH =
         "vec2 texSize = vec2(textureSize(tex0, 0));"
			"int dstX = int(gl_FragCoord.x);"
			"int dstY = int(gl_FragCoord.y);"
			"float filterScaleX = max(1.0f, 1.0f / scaleX);"
			"float support = max(0.5f, filterScaleX * 1.25);"
			"float sum = 0.0;"
			"float wsum = 0.0;"
			"float cen = (dstX + .5f) / scaleX;"
			"int start = int(cen - support + 0.5f);"
			"int end = int(cen + support + 0.5f);"
			"start = max(0, start);"
			"end = min(int(texSize.x), end);"
			"for (int i = start; i < end; ++i) {"
			"	 float d = (float(i) + 0.5f - cen) / filterScaleX;"
			"	 float w = exp(-2.0f * d * d);"
			"    ivec2 pos = ivec2(i, dstY);"
         "    sum += w * texelFetch(tex0, pos, 0).r;"
			"    wsum += w;"
			"}"
			"_out.r = sum / wsum;"
			;

		string shaderV =
         "vec2 texSize = vec2(textureSize(tex0, 0));"
			"int dstX = int(gl_FragCoord.x);"
			"int dstY = int(gl_FragCoord.y);"
			"float filterScaleY = max(1.0f, 1.0f / scaleY);"
			"float support = max(0.5f, filterScaleY * 1.25);"
			"float sum = 0.0;"
			"float wsum = 0.0;"
			"float cen = (dstY + .5f) / scaleY;"
			"int start = int(cen - support + 0.5f);"
			"int end = int(cen + support + 0.5f);"
			"start = max(0, start);"
			"end = min(int(texSize.y), end);"
			"for (int i = start; i < end; ++i) {"
			"	 float d = (float(i) + 0.5f - cen) / filterScaleY;"
			"	 float w = exp(-2.0f * d * d);"
			"    ivec2 pos = ivec2(dstX, i);"
         "    sum += w * texelFetch(tex0, pos, 0).r;"
			"    wsum += w;"
			"}"
			"_out.r = sum / wsum;"
			;

      auto hscaled = lx::shade(src, shaderH,
			lx::ShadeOpts()
			.dstRectSize(ivec2(dstSize.x, src->getHeight()))
			.scale(hscale, 1.0f)
			.uniform("scaleX", hscale)
		);
      auto vscaled = lx::shade(hscaled, shaderV,
			lx::ShadeOpts()
			.dstRectSize(dstSize)
			.uniform("scaleY", vscale)
		);
		return vscaled;
	}

  std::vector<lx::gl::TextureRef> buildGaussianPyramid(lx::gl::TextureRef const& src, float scalePerLevel) {
		std::vector<lx::gl::TextureRef> result;
		result.push_back(src);
		auto state = src;
		while (true) {
			int minDim = std::min(state->getWidth(), state->getHeight());
			if(minDim <= 2)
				break;
			ivec2 dstSize = ivec2(state->getWidth() * scalePerLevel, state->getHeight() * scalePerLevel);
			//state = singleblurLikeCinder(state, dstSize);
           state = lx::gpuBlur::singleblur(state, scalePerLevel, scalePerLevel, GL_CLAMP_TO_BORDER);
			result.push_back(state);
		}
		return result;
	}

   lx::gl::TextureRef blurWithInvKernel(lx::gl::TextureRef const& src) {
		// Build Gaussian pyramid. Each level is half the resolution of the previous.
       std::vector<lx::gl::TextureRef> levels = lx::gpuBlur::buildGaussianPyramid(src, .5f);
		
		// 1/r kernel in 2D: each octave contributes equal weight,
		// so each pyramid level gets equal weight.
		int numLevels = (int)levels.size();
		float weight = 1.0f / numLevels;
		ivec2 dstSize = ivec2(src->getWidth(), src->getHeight());

      auto result = lx::shade(levels[0],
			"_out.rgb = lxTexture().xyz * _w;",
            lx::ShadeOpts().uniform("_w", weight).dstRectSize(dstSize));

		for (int i = 1; i < numLevels; i++) {
           auto upscaled = lx::gpuBlur::upscale(levels[i], dstSize);
			result = lx::shade({ result, upscaled },
			"_out = lxTexture() + lxTexture(tex1) * _w;",
             lx::ShadeOpts().uniform("_w", weight));
		}

		return result;
	}

}
