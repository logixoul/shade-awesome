module;
#include "precompiled.h"

import lxlib.Array2D;
import lxlib.Array2D_imageProc;
import lxlib.stuff;
import lxlib.TextureRef;
import lxlib.gpgpu;
import lxlib.gpuBlur;
import lxlib.SketchBase;
import lxlib.shade;
import lxlib.colorspaces;
import lxlib.VaoVbo;
import lxlib.GlslProg;
import lxlib.KissFFTWrapper;
import lxlib.ConfigManager3;

export module FftRaysSketch;

using FFT = lx::KissFFTWrapper<float>;

bool paused = false;
export struct FftRaysSketch : public lx::SketchBase {
	struct Options {
		float gain;
		float crepuscularFalloff;
     lx::ConfigManager3 cfg;

		Options() : cfg("FftRaysConfig.toml") {}

		void init() {
			cfg.init();
		}

		void update() {
			gain = cfg.getFloat("gain");
			crepuscularFalloff = cfg.getFloat("crepuscularFalloff");
		}
	};

	Options options;

  lx::Array2D<FFT::Complex> freqDomainState;
	lx::Array2D<FFT::Complex> freqDomainStateNext;
	lx::Array2D<FFT::Complex> spatialDomainState;
	lx::Array2D<FFT::Complex> spatialDomainStateNext;
	lx::gl::TextureRef spatialDomainTex;
	lx::gl::TextureRef spatialDomainTexNext;
	
	const int scale = 1;
	
	void setup()
	{
		reset();

		options.init();
	}

	void reset() {
		freqDomainState = generateRandomState();
		freqDomainStateNext = generateRandomState();
		spatialDomainState = spatialDomainStateFromFrequencyDomain(freqDomainState);
		spatialDomainStateNext = spatialDomainStateFromFrequencyDomain(freqDomainStateNext);
        spatialDomainTex = darken(uploadTex(spatialDomainState));
		spatialDomainTexNext = darken(uploadTex(spatialDomainStateNext));
	}

   lx::Array2D<FFT::Complex> generateRandomState() {
      lx::Array2D<FFT::Complex> state(windowSize / scale, lx::nofill());
      for(auto p : state.coords()) {
          float wrappedX = std::min((float)p.x, (float)(state.width() - p.x));
			float wrappedY = std::min((float)p.y, (float)(state.height() - p.y));
			float distToOrigin = glm::length(glm::vec2(wrappedX, wrappedY));
			float amplitude = 1.0f / pow(std::max(distToOrigin, 1.0f), 1.1f);
			amplitude *= 100.0f; // boost overall brightness
          float phase = lx::randFloat(0.0f, 2.0f * (float)lx::pi);
			state(p) = FFT::Complex(
				amplitude * std::cos(phase),
				amplitude * std::sin(phase));
		}
		return state;
	}

  lx::Array2D<FFT::Complex> spatialDomainStateFromFrequencyDomain(lx::Array2D<FFT::Complex> const& freqDomain) {
		int const numElements = freqDomain.width() * freqDomain.height();
		return FFT::inverseFftC2C(freqDomain) / std::sqrt((float)(numElements));
	}

	void keyDown(int key)
	{
		if (key == 'p')
		{
			paused = !paused;
		}
		if (key == 'r')
		{
			reset();
		}
	}
	int elapsedFrames = 0;
	static const int NUM_FRAMES_BETWEEN_REGENERATIONS = 40;
	void update() {
		options.update();

		if (paused)
			return;
		elapsedFrames++;
		if(elapsedFrames % NUM_FRAMES_BETWEEN_REGENERATIONS == 0) {
			freqDomainState = freqDomainStateNext;
			freqDomainStateNext = generateRandomState();
			
			spatialDomainState = spatialDomainStateNext;
			spatialDomainStateNext = spatialDomainStateFromFrequencyDomain(freqDomainStateNext);
			
			spatialDomainTex = spatialDomainTexNext;
            spatialDomainTexNext = darken(uploadTex(spatialDomainStateNext));
		}
	}

 static lx::gl::TextureRef uploadTex(lx::Array2D<FFT::Complex> const& arr) {
       return lx::uploadTex(arr.size(), GL_RG16F, GL_RG, GL_FLOAT, (void*)arr.data());
	}

 lx::gl::TextureRef darken(lx::gl::TextureRef tex) {
		return lx::shade(tex,
			"vec2 c = lxTexture().xy;"
			"float len = length(c);"
			"float newLen = pow(len, 3.0);"
			"_out.xy = c * (newLen / len);");
	}



	void draw() {
		glClearColor(0, 0, 0.7, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, windowSize.x, windowSize.y);
		glDisable(GL_BLEND);

		const float normalizedStateAge = (float)(elapsedFrames % NUM_FRAMES_BETWEEN_REGENERATIONS) / (float)NUM_FRAMES_BETWEEN_REGENERATIONS;
		const float interpolationCoef = 1.0f - std::pow(1.0f - normalizedStateAge, 2.0f); // ease out curve
        auto tex = lx::shade({ spatialDomainTex, spatialDomainTexNext },
			"vec2 state = lxTexture(tex0).rg;"
			"vec2 stateNext = lxTexture(tex1).rg;"
			"vec2 mixed = mix(state, stateNext, interpolationCoef);"

			"_out.rgb = complexToColor_HSV(mixed);",
         lx::ShadeOpts()
				.ifmt(GL_RGB16F)
				.uniform("interpolationCoef", interpolationCoef)
                .functions(lx::FileCache::get("stuff.fs") + 
					R"(
				vec3 complexToColor_HSV(vec2 comp) {
					float hue = atan(comp.y, comp.x);
					hue /= (2.0 * 3.14159265359);
					hue += .5;
					float lightness = length(comp);
					const float saturation = 0.99; // not 1.0 because we want tonemapping bright colors to desaturate them
					vec3 hsv = vec3(hue, saturation, lightness);
					return hsv2rgb(hsv);
				}
			)")
		);
     tex = lx::shade(tex, // upscale
			"_out = lxTexture();"
           , lx::ShadeOpts().dstRectSize(vec2(windowSize)));

        tex = lx::shade(tex,
          "vec2 localTc = texCoord - 0.5;"
			"localTc *= 1.5; /* look from 'up high' */"
			"vec3 col = vec3(0.0);"
			"const int NUM_STEPS = 300;"
			"float sumWeights = 0.0f;"
			"for(int i = 0; i < NUM_STEPS; i++) {"
			"	float weight = pow(crepuscularFalloff, float(i));" // exponential weight falloff
			"	if(i == 0) weight *= 15.0f;" // boost center sample
			//"	vec2 offset = texture(tex0, localTc + 0.5).rg;"
			//"	vec2 localTc2 = localTc + offset*0.0004;"
            //"	col += texture(tex0, localTc2 + 0.5).rgb * weight;"
			"	col += texture(tex0, localTc + 0.5).rgb * weight; "
			"	localTc -= localTc * 0.005;" // "zoom blur" effect
			"	sumWeights += weight;"
			"}"
			"_out.rgb = col / sumWeights;",
             lx::ShadeOpts().uniform("crepuscularFalloff", options.crepuscularFalloff));

       auto texb = lx::gpuBlur::run(tex, 5);
		tex = lx::op(tex) + lx::op(texb);
		//tex = shade(tex, "_out.rgb = lxTexture().rgb * .1;");
        tex = lx::shade(tex,
			"_out.rgb = lxTexture().rgb*gain;"
			//"_out.rgb = reinhardTonemapLuma(_out.rgb);"
			//"_out.rgb = uncharted2Tonemap(_out.rgb);" // filmic tonemapping
			//"_out.rgb = clipChroma(_out.rgb);" // chroma clipping to prevent colors from going out of gamut after tonemapping
			"_out.rgb /= _out.rgb + vec3(1.0);" // reinhard-ish tonemapping
			"_out.rgb *= whitePoint * 1.5;"
			"_out.rgb = desaturateHighlights(_out.rgb);" // desaturate highlights to prevent them from looking too colorful after tonemapping
			//"_out.rgb = pow(_out.rgb, vec3(1.0/2.2));" // gamma correction
             , lx::ShadeOpts().uniform("gain", options.gain)
				.functions(lx::FileCache::get("stuff.fs") +
					R"(
					const float whitePoint = 1.4;
					// http://filmicworlds.com/blog/filmic-tonemapping-operators/
					vec3 uncharted2Tonemap(vec3 color) {
						// Filmic tonemapping curve (from Uncharted 2)
						const float A = 0.15;
						const float B = 0.50;
						const float C = 0.10;
						const float D = 0.20;
						const float E = 0.02;
						const float F = 0.30;
						return (color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F);
					}
					vec3 rgbWeights = vec3(0.22, 0.71, 0.07);
					vec3 reinhardTonemapLuma(vec3 color) {
						float luma = dot(color, rgbWeights);
						float tonemappedLuma = luma / (luma + 1.0);
						return color * (tonemappedLuma / luma);
					}
					vec3 desaturateHighlights(vec3 color) {
						vec3 hsv = rgb2hsv(color);
						float desaturationAmount = hsv.z / whitePoint;
						desaturationAmount = pow(desaturationAmount, 2.0); // make the desaturation kick in more gradually
						hsv.y *= 1.0 - desaturationAmount;
						return hsv2rgb(hsv);
					}
					vec3 clipChroma(vec3 inColor) {
						float luma = dot(inColor, rgbWeights);
						if(luma > 1.0) {
							return vec3(1.0);
						}
						vec3 gray = vec3(luma);

						vec3 chroma = inColor - gray;

						// If already inside cube, return as-is
						if (all(lessThanEqual(inColor, vec3(1.0)))) {
							return inColor;
						}

						float t = 1.0;

						for (int i = 0; i < 3; i++) {
							if (inColor[i] > 1.0) {
								t = min(t, (1.0 - gray[i]) / chroma[i]);
							}
						}
    
						return gray + chroma * t;
					}
				)")
		);
        lx::lxDraw(tex);
	}
};

export using StartupSketch = FftRaysSketch;