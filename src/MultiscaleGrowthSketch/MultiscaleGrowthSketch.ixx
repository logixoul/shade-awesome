module;
#include "../precompiled.h"
#include <numeric>
#include <lxlib/macros.h>

export module MultiscaleGrowthSketch;

import lxlib.Array2D;
import lxlib.stuff;
import lxlib.Array2D_imageProc;
import lxlib.gpgpu;
import lxlib.ConfigManager3;
import lxlib.SketchBase;
import lxlib.shade;
import lxlib.TextureRef;
import lxlib.gpuBlur;
import ThisSketch_ImageProcessingHelpers;
import gpuBlurClaude;

int wsx = 700, wsy = 700;

using namespace ThisSketch;

lx::Array2D<float> img(256, 256);

export struct MultiscaleGrowthSketch : public lx::SketchBase {
	struct Options {
		float morphogenesisStrength;
		const float contrastizeStrength = 1.0f;
		float blendWeaken;
		float weightFactor;
		bool multiscale;
		bool binarizePostprocessing;
		float highPassStrength;
		lx::ConfigManager3 cfg;

		Options() : cfg("MultiscaleGrowthConfig.toml") {}

		void init() {
			cfg.init();
		}

		void update() {
			morphogenesisStrength = cfg.getFloat("morphogenesisStrength");
			blendWeaken = cfg.getFloat("blendWeaken");
			weightFactor = cfg.getFloat("weightFactor");
			multiscale = cfg.getBool("multiscale");
			binarizePostprocessing = cfg.getBool("binarizePostprocessing");
			highPassStrength = cfg.getFloat("highPassStrength");
		}
	};

	Options options;
	bool isPaused = false;
		
	void setup()
	{
		options.init();
		reset();
	}

	void keyDown(int key)
	{
		if (keys['p']) {
			isPaused = !isPaused;
		}
		if (keys['r'])
		{
			reset();
		}
	}
	void reset() {
        for(auto p : img.coords()) {
         img(p) = lx::randFloat();
		}
	}
	lx::Array2D<float> updateSingleScale(lx::Array2D<float> aImg)
	{
		auto img = aImg.clone();

		auto gradients = lx::get_gradients<float, lx::WrapModes::Clamp>(img);
		auto img2 = img.clone();
        for(auto p : img.coords()) {
			vec2 const& pf = vec2(p);
			vec2 const& grad = gradients(p);
           vec2 const& gradN = lx::safeNormalized(grad);
			vec2 const& gradNPerp = perpLeft(gradN);
			float add = -hessianDirectionalSecondDeriv<float, lx::WrapModes::Clamp>(img, p, gradNPerp);
			img2(p) += add * options.morphogenesisStrength;
			//lx::splatBilinearPoint<float, lx::WrapModes::Clamp>(img2, pf - gradN * add, add * options.morphogenesisStrength);
		}
		auto kernel = lx::getGaussianKernel(3, lx::sigmaFromKsize(3));
		//auto blurredImg2 = ::separableConvolve<float, WrapModes::Clamp>(img2, kernel);
		auto blurredImg2 = ThisSketch::gaussianBlur3x3<float, lx::WrapModes::Clamp>(img2);
		img = blurredImg2;
		img = applyVerticalGradient(img);

		return img;
	}
	lx::Array2D<float> updateSingleScale_GPT(lx::Array2D<float> aImg)
	{
		auto img = aImg.clone();
		auto tex = lx::uploadTex(img);
		tex->setWrap(GL_CLAMP_TO_EDGE);
		auto levelSetCurvatureTex = lx::shade(tex, R"(
				float here = texture(tex0, texCoord).r;
				float left = texture(tex0, texCoord - vec2(texelSize0.x, 0.0)).r;
				float right = texture(tex0, texCoord + vec2(texelSize0.x, 0.0)).r;
				float down = texture(tex0, texCoord - vec2(0.0, texelSize0.y)).r;
				float up = texture(tex0, texCoord + vec2(0.0, texelSize0.y)).r;
				float downLeft = texture(tex0, texCoord - texelSize0).r;
				float downRight = texture(tex0, texCoord + vec2(texelSize0.x, -texelSize0.y)).r;
				float upLeft = texture(tex0, texCoord + vec2(-texelSize0.x, texelSize0.y)).r;
				float upRight = texture(tex0, texCoord + texelSize0).r;

				float fx = (right - left) * 0.5;
				float fy = (up - down) * 0.5;
				float fxx = right - 2.0 * here + left;
				float fyy = up - 2.0 * here + down;
				float fxy = (upRight - upLeft - downRight + downLeft) * 0.25;

				float gradSq = fx * fx + fy * fy;
				if(gradSq <= 0.03) {
					_out.r = 0.0;
					return;
				}

				float numerator = fxx * fy * fy - 2.0 * fx * fy * fxy + fyy * fx * fx;
				float denominator = pow(gradSq, 1.5);
				float curvature = numerator / denominator;
				_out.r = curvature;
		)");
		auto levelSetCurvature = lx::downloadTex<float>(levelSetCurvatureTex);
		auto img2 = img.clone();
		for (auto p : img.coords()) {
			float curvature = levelSetCurvature(p);
			img2(p) += -curvature * options.morphogenesisStrength;
		}
		auto kernel = lx::getGaussianKernel(3, lx::sigmaFromKsize(3));
		//auto blurredImg2 = ::separableConvolve<float, WrapModes::Clamp>(img2, kernel);
		auto blurredImg2 = ThisSketch::gaussianBlur3x3<float, lx::WrapModes::Clamp>(img2);
		img = blurredImg2;
		img = applyVerticalGradient(img);

		return img;
	}
	Img applyVerticalGradient(Img const& img) {
		Img result = lx::uninitializedArrayLike(img);
     for(auto p : result.coords()) {
           float floatY = p.y / (float)result.height();
			floatY = glm::mix(options.blendWeaken, 1.0f - options.blendWeaken, floatY);
			result(p) = blendHardLight(img(p), floatY);
		}
		return result;
	}
	float getLevelWeight(int level, int maxLevel) const {
		float iNormalized = level / float(maxLevel - 1);
		return exp(options.weightFactor * iNormalized);
	}
	std::vector<float> getLevelWeights(int numLevels) const {
		std::vector<float> result;
		for (int i = 0; i < numLevels; i++) {
			result.push_back(getLevelWeight(i, numLevels));
		}
		float sum = std::accumulate(result.begin(), result.end(), 0.0f);
		for (auto& weight : result) {
			//	weight /= sum;
		}
		return result;
	}
	Img multiscaleApply(Img src, function<Img(Img)> func) {
		std::vector<Img> origScales = ThisSketch::buildGaussianPyramid(src, 0.5f);
		std::vector<Img> updatedScales(origScales.size());
		const int last = origScales.size() - 1;
		updatedScales[last] = func(origScales[last]);
		auto weights = getLevelWeights(origScales.size());
		for (int i = updatedScales.size() - 1; i >= 1; i--) {
			auto diff = updatedScales[i] - origScales[i];
			diff = diff * weights[i];
			auto const upscaledDiff = gpuBlurClaude::singleblurLikeCinder(diff, origScales[i - 1].size());
			auto& nextScale = updatedScales[i - 1];
			nextScale = origScales[i - 1] + upscaledDiff;
			nextScale = func(nextScale);
		}
		return updatedScales[0];
	}
	void update() {
		lx::Array2D<float> newImg;
		if (options.multiscale)
			//newImg = multiscaleApply(img, [this](auto arg) { return updateSingleScale(arg); });
			newImg = multiscaleApply(img, [this](auto arg) { return updateSingleScale(arg); });
		else
			newImg = updateSingleScale(img);
		if(!isPaused)
			img = newImg;
		img = lx::to01(img);

		//testMatchingFunctionality();
	}

	void testMatchingFunctionality() {
		lx::Array2D<float> arr(100, 100);
        for(auto p : arr.coords()) {
         arr(p) = lx::randFloat();
		}

		std::vector<int> testSizes{ 50, 200, 67, 107, 3 };
		for (int testSize : testSizes) {
			//auto newImpl = ThisSketch::resize_referenceImplementation(arr, ivec2(testSize, testSize)); // works
			auto newImpl = gpuBlurClaude::singleblurLikeCinder(arr, ivec2(testSize, testSize));
			auto oldImpl = ThisSketch::resize_referenceImplementation(arr, ivec2(testSize, testSize));
			//mm("new", newImpl);
			//mm("old", oldImpl);
				
            for(auto p : newImpl.coords()) {
				if (abs(newImpl(p) - oldImpl(p)) > 0.0001) {
					std::cout << "[" << testSize << "] mismatch at " << p.x << ", " << p.y << ": " << newImpl(p) << " vs " << oldImpl(p) << std::endl;
				}
			}
		}
	}

	static lx::gl::TextureRef gpuHighpass(lx::gl::TextureRef in, float strength) {
		auto blurred = gpuBlurClaude::blurWithInvKernel(in);
		auto highpassed = lx::shade({ in, blurred }, MULTILINE(
			float f = texture().x;
       float fBlurred = texture(tex1).x;
		float highPassed = f - fBlurred * highPassStrength;
		_out.r = highPassed;
			), lx::ShadeOpts().uniform("highPassStrength", strength)
			);
		return highpassed;
	}
	static lx::gl::TextureRef gpuHighpassNew(lx::gl::TextureRef in, float strength) {
		auto blurred = lx::gpuBlur::run(in, 2);
		auto highpassed = lx::shade({ in, blurred }, MULTILINE(
			float f = texture().x;
		float fBlurred = texture(tex1).x;
		float highPassed = f - fBlurred * highPassStrength;
		_out.r = highPassed;
			), lx::ShadeOpts().uniform("highPassStrength", strength)
			);
		return highpassed;
	}
	lx::gl::TextureRef postprocessNew() {
		auto imgClamped = img.clone();
		for (auto p : imgClamped.coords()) imgClamped(p) = glm::clamp(imgClamped(p), 0.0f, 1.0f);

		auto imgTex = lx::uploadTex(imgClamped);
		auto imgTexCentered = lx::shade(imgTex,
			"float f = texture().x;"
			"_out.r = f - .5;"
		);

		auto imgTexHighpassed = gpuHighpassNew(imgTexCentered, options.highPassStrength);
		//imgTexHighpassed = gpuHighpass(imgTexHighpassed, options.highPassStrength);

		auto result1 = lx::shade(imgTexHighpassed,
			"float f = texture().x;"
			"float fw = fwidth(f);"
			"f = smoothstep(-fw/2.0, fw/2.0, f) - smoothstep(.01-fw/2.0, .01+fw/2.0, f);"
			"_out.rgb = f * vec3(1.0, 0.1, 0.05);",
				lx::ShadeOpts()
				.dstRectSize(ivec2(wsx, wsy))
				.ifmt(GL_RGBA16F)
		);	
		auto result2 = lx::shade(imgTexHighpassed,
			"float f = texture().x;"
			"float fw = fwidth(f);"
			"f = smoothstep(.01-fw/2.0, .01+fw/2.0, f);"
			"_out.rgb = f * vec3(1.0, 0.01, 0.05).bgr;",
			lx::ShadeOpts()
			.dstRectSize(ivec2(wsx, wsy))
			.ifmt(GL_RGBA16F)
		);
		lx::gl::TextureRef result = op(result1) + result2;
		auto resultB = gpuBlurClaude::blurWithInvKernel(result);
		result = op(result) + op(resultB) * 3.0;

		result = lx::shade({ result, imgTex }, R"(
			vec3 bloomedHiPass = texture(tex0).rgb;
			vec3 original = vec3(texture(tex1).r);
			vec3 sum = bloomedHiPass * 4.0;
			//float L =  dot(vec3(.3333), sum);
			//float Lnew = L / (L + 1.0);
			//sum *= Lnew / L;
			sum /= sum + vec3(1.0);
			_out.rgb = sum;


			//_out.rgb = mix(original, vec3(1.0), bloomedHiPass);
		)");

		return result;
	}
	lx::gl::TextureRef postprocess() {
		auto imgClamped = img.clone();
        for(auto p : imgClamped.coords()) imgClamped(p) = glm::clamp(imgClamped(p), 0.0f, 1.0f);

        auto imgTex = lx::uploadTex(imgClamped);
		auto imgTexCentered = lx::shade(imgTex,
			"float f = texture().x;"
			"_out.r = f - .5;"
		);

		auto imgTexHighpassed = gpuHighpass(imgTexCentered, options.highPassStrength);
		imgTexHighpassed = gpuHighpass(imgTexHighpassed, options.highPassStrength);
      auto imgHighpassed = lx::downloadTex<float>(imgTexHighpassed);

		auto pyramid = buildGaussianPyramid(imgHighpassed);
		auto stateTex = lx::shade(imgTex, "_out = vec4(0.0);", lx::ShadeOpts().dstRectSize(windowSize));
		for (int i = pyramid.size() - 1; i >= 0; i--) {
			auto& thisLevel = pyramid[i];
           auto thisLevelTex = lx::uploadTex(thisLevel);
			auto thisLevelTexContrastized = lx::shade(thisLevelTex,
				"float f = texture().x;"
				"float fw = fwidth(f);"
				"f = smoothstep(-fw/2.0, fw/2.0, f);"
				"_out.r = f;", lx::ShadeOpts().dstRectSize(ivec2(wsx, wsy)));
			stateTex = lx::op(stateTex) + thisLevelTexContrastized;
		}
		stateTex = lx::op(stateTex) / float(pyramid.size());
		stateTex = (lx::op(stateTex) + lx::op(lx::gpuBlur::run(stateTex, 3)) * 2.0f) / 2;
		stateTex = lx::shade(stateTex, MULTILINE(
			float val = texture().x;
		vec3 fire = vec3(min(val * 1.5, 1.), pow(val, 2.5), pow(val, 12.));
		_out.rgb = fire;
			),
			lx::ShadeOpts().ifmt(GL_RGBA16F));
		return stateTex;
	}
	void draw()
	{
		lx::lxClear();
		options.update();

        lx::gl::TextureRef tex = lx::uploadTex(img);
		if (options.binarizePostprocessing) {
			tex = postprocessNew();
		}
		else {
			tex = redToLuminance(tex);
		}
		glViewport(0, 0, windowSize.x, windowSize.y);
		lx::lxDraw(tex, lx::Rect<float>(0, 0, 1, 1));
	}
};

export using StartupSketch = MultiscaleGrowthSketch;

