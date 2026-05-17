module;
#include "precompiled.h"
#include <lxlib/macros.h>

export module ThisSketch_ImageProcessingHelpers;

import lxlib.Array2D_imageProc;
import lxlib.gpgpu;
import lxlib.Array2D;
import lxlib.TextureRef;
import lxlib.stuff;
import lxlib.shade;
import gpuBlurClaude;

export namespace ThisSketch {

using Img = lx::Array2D<float>;

vec2 perpLeft(vec2 const& v);

template<class T, class WrapPolicy>
lx::Array2D<T> gaussianBlur3x3(lx::Array2D<T> src) {
lx::Array2D<T> dst1(src.width(), src.height());
lx::Array2D<T> dst2(src.width(), src.height());
for(auto p : dst1.coords())
dst1(p) = .25f * (2 * WrapPolicy::fetch(src, p.x, p.y) + WrapPolicy::fetch(src, p.x - 1, p.y) + WrapPolicy::fetch(src, p.x + 1, p.y));
for(auto p : dst2.coords())
dst2(p) = .25f * (2 * WrapPolicy::fetch(dst1, p.x, p.y) + WrapPolicy::fetch(dst1, p.x, p.y - 1) + WrapPolicy::fetch(dst1, p.x, p.y + 1));
return dst2;
}

float mulContrastize(float i, float contrastizeStrength);

template<class T, class WrapPolicy>
float hessianDirectionalSecondDeriv(lx::Array2D<T>& src, ivec2 const& p, vec2 const& d) {
float fxx = WrapPolicy::fetch(src, p.x + 1, p.y) - 2.0f * WrapPolicy::fetch(src, p.x, p.y) + WrapPolicy::fetch(src, p.x - 1, p.y);
float fyy = WrapPolicy::fetch(src, p.x, p.y + 1) - 2.0f * WrapPolicy::fetch(src, p.x, p.y) + WrapPolicy::fetch(src, p.x, p.y - 1);
float fxy = 0.25f * (
WrapPolicy::fetch(src, p.x + 1, p.y + 1)
- WrapPolicy::fetch(src, p.x - 1, p.y + 1)
- WrapPolicy::fetch(src, p.x + 1, p.y - 1)
+ WrapPolicy::fetch(src, p.x - 1, p.y - 1));
return d.x * d.x * fxx + 2.0f * d.x * d.y * fxy + d.y * d.y * fyy;
}

//Array2D<float> resize(Array2D<float> src, ivec2 dstSize, const ci::FilterBase& filter);
std::vector<Img> buildGaussianPyramid(Img src, float scalePerLevel = 0.5f);

lx::gl::TextureRef redToLuminance(lx::gl::TextureRef const& in);
float blendHardLight(float base, float blend);

lx::Array2D<float> resize_referenceImplementation(lx::Array2D<float> const& src, ivec2 dstSize);

} // namespace ThisSketch

namespace ThisSketch {

vec2 perpLeft(vec2 const& v) {
return vec2(-v.y, v.x);
}



float mulContrastize(float i, float contrastizeStrength) {
i = glm::clamp(i, 0.0f, 1.0f);
const bool invert = i > .5f;
if (invert) {
i = 1.0f - i;
}
i *= 2.0f;
i = pow(i, contrastizeStrength);
i *= .5f;
if (invert) {
i = 1.0f - i;
}
return i;
}

std::vector<Img> buildGaussianPyramid(Img src, float scalePerLevel) {
std::vector<Img> scales;
auto state = src.clone();
while (true)
{
const int size = std::min(state.width(), state.height());
if (size <= 2)
break;
scales.push_back(state);
ivec2 newSize = ivec2(vec2(state.size()) * scalePerLevel);
state = gpuBlurClaude::singleblurLikeCinder(state, newSize);
}
return scales;
}

class FilterGaussianNonNormalized {
public:
float operator()(float x) const {
return exp(-2.0f * x * x);
}

float getSupport() const { return 1.25f; }
};

lx::Array2D<float> resize_referenceImplementation(lx::Array2D<float> const& src, ivec2 dstSize)
{
const int srcW = src.width();
const int srcH = src.height();
const int dstW = dstSize.x;
const int dstH = dstSize.y;

const float sx = dstW / (float)srcW;
const float sy = dstH / (float)srcH;

FilterGaussianNonNormalized filter;

const float filterScaleX = std::max(1.0f, 1.0f / sx);
const float filterScaleY = std::max(1.0f, 1.0f / sy);
const float supportX = std::max(0.5f, filterScaleX * filter.getSupport());
const float supportY = std::max(0.5f, filterScaleY * filter.getSupport());

lx::Array2D<float> tmp(dstW, srcH);
lx::Array2D<float> out(dstW, dstH);

for (int dstY = 0; dstY < srcH; ++dstY) {
for (int dstX = 0; dstX < dstW; ++dstX) {
const float center = (dstX + .5f) / sx;
int start = (int)(center - supportX + 0.5f);
int end = (int)(center + supportX + 0.5f);
if (start < 0) start = 0;
if (end > srcW) end = srcW;

float den = 0.0f;
float sum = 0.0f;
for (int i = start; i < end; ++i) {
float d = (i + 0.5f - center) / filterScaleX;
float w = filter(d);
sum += w * src.data()[dstY * srcW + i];
den += w;
}

tmp.data()[dstY * dstW + dstX] = sum / den;
}
}

for (int dstY = 0; dstY < dstH; ++dstY) {
const float center = (dstY + .5f) / sy;
int start = (int)(center - supportY + 0.5f);
int end = (int)(center + supportY + 0.5f);
if (start < 0) start = 0;
if (end > srcH) end = srcH;


for (int dstX = 0; dstX < dstW; ++dstX) {
float sum = 0.0f;
float den = 0.0f;
for (int i = start; i < end; ++i) {
float d = (i + 0.5f - center) / filterScaleY;
float w = filter(d);
sum += w * tmp.data()[i * dstW + dstX];
den += w;
}

out.data()[dstY * dstW + dstX] = sum / den;
}
}

return out;
}

lx::gl::TextureRef redToLuminance(lx::gl::TextureRef const& in) {
return lx::shade(in,
"_out.rgb = vec3(lxTexture().x);",
lx::ShadeOpts().ifmt(GL_RGBA16F)
);
}

float blendHardLight(float base, float blend) {
if (blend < 0.5f) {
return 2.0f * base * blend;
}
else {
return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
}
}

}
