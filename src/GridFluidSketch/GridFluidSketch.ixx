module;
#include "precompiled.h"
//#include "using_namespace.h"
import lxlib.shade;
import lxlib.gpgpu;
import lxlib.Array2D_imageProc;
import lxlib.Array2D;
import lxlib.stuff;
import lxlib.gpuBlur;
import lxlib.ConfigManager3;
#include <lxlib/macros.h>
import lxlib.Rect;
import lxlib.SketchBase;
import lxlib.TextureRef;

export module GridFluidSketch;

export struct GridFluidSketch : public lx::SketchBase {
	lx::ConfigManager3 cfg;

	GridFluidSketch() :
		cfg("gridFluidConfig.toml")
	{}

	/*void update() {
		surfTensionThres = cfg.getFloat("surfTensionThres");
		surfTension = cfg.getFloat("surfTension");
		gravity = cfg.getFloat("gravity");
		incompressibilityCoef = cfg.getFloat("incompressibilityCoef");
			

		cfg.end();
	}*/

	const int scale = 6;
	int sx;
	int sy;
	ivec2 sz;
	struct Material {
     lx::Array2D<float> density;
		lx::Array2D<vec2> momentum;
	};
	Material red, green;
	vector<Material*> materials{ &green, &red };//&red, &green };
	
	bool pause = false;

 lx::Array2D<float> bounces_dbg;



	void setup()
	{
		cfg.init();
		sx = windowSize.x / scale;
		sy = windowSize.y / scale;
		sz = ivec2(sx, sy);

		for (auto& material : materials) {
         material->density = lx::Array2D<float>(sz);
			material->momentum = lx::Array2D<vec2>(sz);
		}

		glDisable(GL_BLEND);

		reset();
	}
	void keyDown(int key)
	{
		if (keys[' ']) {
			doFluidStep();
		}
		if (keys['r'])
		{
			reset();
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	void keyUp(int key)
	{
	}
	void reset() {
		std::fill(red.density.begin(), red.density.end(), 0.0f);
		std::fill(red.momentum.begin(), red.momentum.end(), vec2());

		std::fill(green.density.begin(), green.density.end(), 0.0f);
		std::fill(green.momentum.begin(), green.momentum.end(), vec2());

		for (int x = 0; x < sz.x; x++) {
			for (int y = sz.y * .75; y < sz.y; y++) {
				//red.density(x, y) = 1;
			}
		}
	}
	vec2 direction;
	vec2 lastm;
	void mouseMove(ivec2 pos)
	{
		mm(pos);
		cout << "move " << pos << endl;
	}
	void mm(ivec2 pos)
	{
		direction = vec2(pos) - lastm;
		lastm = pos;
	}
	
   lx::gl::TextureRef gauss3texScaled(lx::gl::TextureRef src, float scale) {
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
			"_out = sum;",
            lx::ShadeOpts().scale(scale)
		);
		return state;
	}

	void draw()
	{
      lx::lxClear();

		static float colorAmount = 0.06f;
		ImGui::DragFloat("colorAmount", &colorAmount, 1.0f, 0.01, 8.0, "%.3f", ImGuiSliderFlags_Logarithmic);
		static float matterAmount = 13.0f;
		ImGui::DragFloat("matterAmount", &matterAmount, 1.0f, 0.01, 8.0, "%.3f", ImGuiSliderFlags_Logarithmic);
		static float matterThreshold = 1.0f;
		ImGui::DragFloat("matterThreshold", &matterThreshold, 1.0f, 0.01, 8.0, "%.3f", ImGuiSliderFlags_Logarithmic);

     auto density = lx::uninitializedArrayLike(red.density);
        for(auto p : density.coords()) {
			density(p) = red.density(p) + green.density(p);
		}
       auto sumTex = lx::uploadTex(density);
		auto redTex = lx::uploadTex(red.density);
		auto greenTex = lx::uploadTex(green.density);

		sumTex = gauss3texScaled(sumTex, 1.0); // reduce upscale artefacts
		sumTex = gauss3texScaled(sumTex, 1.0); // reduce upscale artefacts
		sumTex = gauss3texScaled(sumTex, 1.0); // reduce upscale artefacts
      sumTex = lx::shade(sumTex,
			"float f = lxTexture().x;"
			"float fw = fwidth(f);"
			"_out.r = f * smoothstep(matterThreshold-fw/2, matterThreshold+fw/2, f);"
          , lx::ShadeOpts().uniform("matterThreshold", matterThreshold).scale(scale)
		);
       sumTex = lx::Operable(sumTex) * matterAmount;
		redTex = lx::Operable(redTex) * colorAmount;
		greenTex = lx::Operable(greenTex) * colorAmount;

     auto momentumTex = lx::uploadTex(red.momentum);
     auto hsvTex = lx::shade(momentumTex, MULTILINE(
			vec2 momentum = lxTexture().xy;
         float angle = atan(momentum.y, momentum.x) / (2 * lx::pi) + .5;
			//angle *= pi;
			float len = length(momentum);
			len /= len + 1.0;
			_out.rgb = hsl2rgb(vec3(angle, 1.0, .5)) * pow(len, 3.0) * 3.0;
			),
         lx::ShadeOpts()
				.ifmt(GL_RGB16F)
              .functions(lx::FileCache::get("stuff.fs"))
		);

		static float bloomSize = 1.5f;
		static int bloomIters = 6.0f;
		static float bloomIntensity = 0.07f;
		ImGui::DragFloat("bloomSize", &bloomSize, 1.0f, 0.1, 100, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragInt("bloomIters", &bloomIters, 1.0f, 1, 16, "%d", ImGuiSliderFlags_None);
		ImGui::DragFloat("bloomIntensity", &bloomIntensity, 1.0f, 0.0001, 2000, "%.3f", ImGuiSliderFlags_Logarithmic);
        auto redTexB = lx::gpuBlur::run_longtail(redTex, bloomIters, bloomSize);
		auto greenTexB = lx::gpuBlur::run_longtail(greenTex, bloomIters, bloomSize);
		auto hsvTexB = lx::gpuBlur::run_longtail(hsvTex, bloomIters, bloomSize);

     greenTex = lx::op(greenTex) * 0.16;

      hsvTex = lx::shade({ hsvTex, hsvTexB }, MULTILINE(
		  _out = (lxTexture() + lxTexture(tex1) * 1.0) * bloomIntensity;
		),
           lx::ShadeOpts().uniform("bloomIntensity", bloomIntensity)
		);
      static const auto format = lx::gl::Texture::Format().mipmap(true).minFilter(GL_LINEAR_MIPMAP_LINEAR).magFilter(GL_LINEAR).loadTopDown(true).wrap(GL_MIRRORED_REPEAT).internalFormat(GL_RGBA8);
		static auto envMap = lx::gl::Texture::create("milkyway.png", format);
		static auto envMap2 = lx::shade(envMap, MULTILINE(
			vec3 c = lxTexture().xyz;
		c /= vec3(1.0) - c * 0.99;
		_out.rgb = c;
			),
           lx::ShadeOpts().ifmt(GL_RGB16F));
		//static auto envMap = gl::TextureCubeMap::create(loadImage(loadAsset("envmap_cube.jpg")), gl::TextureCubeMap::Format().mipmap());

      auto grads = lx::getGradients(sumTex);
		std::string const shaderFunctions = "float PI = 3.14159265358979323846264;\n"
			"vec2 latlong(vec3 refl) {\n"
			"	return vec2(atan(refl.z, refl.x) / (2.0 * PI) + 0.5, asin(clamp(refl.y, -1.0, 1.0)) / PI + 0.5);"
			"}\n"
			"vec3 getEnv(vec3 v) {\n"
			"	vec3 c = texture(tex3, latlong(v)).xyz;\n"
			//"	c = pow(c, vec3(2.2));" // gamma correction
			"	return c;"
			"}\n"
			MULTILINE(
				float manualLod(vec2 uv, vec2 texSize, vec2 refractOffset) {
			vec2 uvPixels = uv * texSize;
			vec2 dx = dFdx(uvPixels);
			vec2 dy = dFdy(uvPixels);
			float rho = max(dot(dx, dx), dot(dy, dy));
			rho = max(rho, 1e-8);
			float lod = 0.5 * log2(rho);
			float refractMetric = length(dFdx(refractOffset)) + length(dFdy(refractOffset));
			lod += log2(1.0 + refractMetric * refractLodScale);
			float maxLod = floor(log2(max(texSize.x, texSize.y)));
			return clamp(lod, 0.0, maxLod);
		});
     auto tex2 = lx::shade({ sumTex, grads, envMap2, redTex, greenTex, hsvTex },


			"vec3 hsv = lxTexture(tex5).xyz;"
			"vec2 d = lxTexture(tex1).xy;"

			"vec3 normal = normalize(vec3(d.x, d.y, 1.0));"
			"vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0));"



			"float eta=1.0/1.3;"
			"vec3 refr=refract(viewDir, normal, eta);"
			"float z = max(abs(refr.z), 1e-3);"
			"vec2 refractOffset = refr.xy / z;"
         "vec2 refractUv = texCoord + refractOffset * .1;"
			//"float lod = manualLod(refractUv, textureSize(tex3, 0), refractOffset) + lodBias;"
			//"vec3 c = textureLod(tex3, refractUv, lod*0.0).rgb;"
			"vec2 dPdx = dFdx(refractUv);"
			"vec2 dPdy = dFdy(refractUv);"
            "vec3 c = textureGrad(tex2, refractUv, dPdx, dPdy).rgb;"
			"float redVal = lxTexture(tex3).x;"
			"float greenVal = lxTexture(tex4).x;"
			//"redVal = 1.0-exp(-redVal);"
			//"greenVal = 1.0-exp(-greenVal);"
			// this is taken from https://www.shadertoy.com/view/Mld3Rn
			"vec3 redColor = redVal * vec3(0.0, 0.4, 1.0).zyx;"
			"vec3 greenColor = greenVal / vec3(0.5, 0.99, 1.0);"
			//"vec3 redColor = vec3(min(redVal * 1.5, 1.), pow(redVal, 2.5), pow(redVal, 12.)); "
			//"vec3 greenColor = vec3(min(greenVal * 1.5, 1.), pow(greenVal, 2.5), pow(greenVal, 12.)).zyx; "
			"c *= exp(-greenColor * 10.0);"
			//"c += redColor;"
			/*"if(texture(tex).x > surfTensionThres) {"
			"	vec3 refl = reflect(-viewDir, normal);"
			"	float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);"
			"	float fresnelWeight = mix(0.1, 1.0, fresnel);"
			"	c += getEnv(refl) * fresnelWeight;"
			"}"*/


			"_out.rgb = c;"

			//"_out.rgb = vec3(d, 0.0);"
			//"_out.rgb /= _out.rgb + 1.0;"
           , lx::ShadeOpts().ifmt(GL_RGB16F)
			.uniform("surfTensionThres", cfg.getFloat("surfTensionThres"))
			.uniform("lodBias", 0.0f)
			.uniform("lodMax", 3.0f)
			.uniform("refractLodScale", 5.0f)
			//.scale(::scale)
			.functions(shaderFunctions)
			
		);

	const auto tex2Thres = lx::shade(tex2, "vec3 c=lxTexture().xyz; c *= step(vec3(1.0), c); _out.rgb=c;");
		auto tex2b = lx::gpuBlur::run_longtail(tex2Thres, bloomIters, bloomSize);
		tex2 = lx::op(tex2) + lx::op(tex2b) * bloomIntensity;

      tex2 = lx::shade(tex2,
		"vec3 c = lxTexture(tex0).xyz;"
			"if(c.r<0.0||c.g<0.0||c.b<0.0) { _out.rgb = vec3(1.0, 0.0, 0.0); }" // eases debugging
			"c /= c + vec3(1.0);"
			"c = pow(c, vec3(1.0/2.2));"
			"c = smoothstep(vec3(0.0), vec3(1.0), c);"
			"_out.rgb = c;"
		);
		//videoWriter->write(tex2);

       lx::lxDraw(tex2);
		//lxDraw(envMap);
	}
	void update()
	{
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		auto material = keys['g'] ? &red : &green;

		ivec2 mousePos = ivec2(this->lastm / float(scale));
        lx::Rect<int> a = lx::Rect<int>::fromBounds(mousePos, mousePos);
		if (mouseDown_[0])
		{
			
			int r = 80 / pow(2, scale);
			a.expand(r, r);
			for (int x = a.x1; x <= a.x2; x++)
			{
				for (int y = a.y1; y <= a.y2; y++)
				{
					vec2 v = ivec2(x, y) - mousePos;
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					if (!material->density.contains(ivec2(x, y)))
						continue;
					material->density(x, y) += 1.f * w * 100.0;
				}
			}
		}
		else if (mouseDown_[2]) {
			//mm();
			int r = 15;
			a.expand(r, r);
			for (int x = a.x1; x <= a.x2; x++)
			{
				for (int y = a.y1; y <= a.y2; y++)
				{
					vec2 v = ivec2(x, y) - mousePos;
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					if (!material->density.contains(ivec2(x, y)))
						continue;
					if (material->density(x, y) != 0.0f)
						material->momentum(x, y) += w * material->density(x, y) * 4.0f * direction / (float)scale;
				}
			}
		}
	}

	template<class T, class WrapPolicy>
  static lx::Array2D<T> convolve(lx::Array2D<T> in, lx::Array2D<float> kernel) {
       int r = kernel.width() / 2;
      lx::Array2D<T> out(in.size(), lx::nofill());
        for(auto p : out.coords()) {
			float sum = 0.0f;
			for (int kx = -r; kx < r; kx++) {
				for (int ky = -r; ky < r; ky++) {
					sum += kernel(kx + r, ky + r) * WrapPolicy::template fetch<T>(in, p.x + kx, p.y + ky);
				}
			}
			out(p) = sum;
		}
		return out;
	}
	void repel(Material& affectedMaterial, Material& actingMaterial) {
        lx::Array2D<float> kernel(7, 7);
		ivec2 center = kernel.size() / 2;
       int r = kernel.width() / 2;
     for(auto p : kernel.coords()) {
			ivec2 p2 = p - center;
			vec2 p2f = vec2(p2);
			float distance = length(p2f);
			//if (distance == 0)
			//	distance = .1;
			if (distance >= r)
				continue;
			kernel(p) = pow(1 - distance / r, 2.0f);
		}
		auto kernelSum = std::accumulate(kernel.begin(), kernel.end(), 0.0f);
     for(auto p : kernel.coords()) {
			kernel(p) /= kernelSum;
		}
		//auto guidance = convolve<float, WrapModes::Clamp>(actingMaterial.density, kernel);
		//auto guidance = gaussianBlur<float, WrapModes::Clamp>(actingMaterial.density, 3 * 2 + 1);
		auto guidance = actingMaterial.density;

		static float intermaterialRepelCoef = 0.5f;
		ImGui::DragFloat("intermaterialRepelCoef", &intermaterialRepelCoef, 1.0f, 0.1, 5.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

     for(auto p : affectedMaterial.density.coords())
		{
          auto g = lx::gradient_i<float, lx::WrapModes::ZeroesOutside>(guidance, p);
			//if(length(g) != 0.0f) g = glm::normalize(g);

			affectedMaterial.momentum(p) += -g * affectedMaterial.density(p) * intermaterialRepelCoef;
		}
	}

	void doFluidStep() {
		//repel(::red, ::green);
		//repel(::green, ::red);

		for (auto material : materials) {
			if (material == &red)
				continue;
			auto& momentum = material->momentum;
			auto& density = material->density;
			float gravity = cfg.getFloat("gravity");
         for(auto p : momentum.coords())
			{
				momentum(p) += vec2(0.0f, gravity) * density(p);
			}

         density = gauss3_forwardMapping<float, lx::WrapModes::MirrorWrap>(density);
			density = gauss3_forwardMapping<float, lx::WrapModes::MirrorWrap>(density);
			momentum = gauss3_forwardMapping<vec2, lx::WrapModes::MirrorWrap>(momentum);
			momentum = gauss3_forwardMapping<vec2, lx::WrapModes::MirrorWrap>(momentum);

			//auto density_b = density.clone();
			//for(int i < 0
			//density_b = gaussianBlur<float, WrapModes::Clamp>(density_b, 3 * 2 + 1);
			auto& guidance = density;
			float surfTensionThres = cfg.getFloat("surfTensionThres");
			float surfTension = cfg.getFloat("surfTension");
			float incompressibilityCoef = cfg.getFloat("incompressibilityCoef");
         for(auto p : momentum.coords())
			{
              auto g = lx::gradient_i<float, lx::WrapModes::ZeroesOutside>(guidance, p);
				if (guidance(p) < surfTensionThres)
				{
					g = g * surfTension * density(p);
				}
				else
				{
					g *= -incompressibilityCoef;
				}

				momentum(p) += g;
			}
			//auto density2 = empty_like(density);
			int count = 0;
			//const auto lowerBound = vec2(0.0f);
			//const auto upperBound = vec2(density.size() - ivec2(2));
            /*for(auto p : density.coords())
			{
				float hereMono = density(p);
				vec2 offset = momentum(p) / density(p);
				vec2 dst;
				do {
					dst = vec2(p) + offset;

					//dst = glm::clamp(dst, lowerBound, upperBound);
					const float atDst = getBilinear(density, dst);
					if (hereMono < surfTensionThres && atDst > surfTensionThres)
						offset *= .9f;
					else if (hereMono > surfTensionThres && atDst < surfTensionThres)
						offset *= .9f;
					else
						break;
				} while (true);
				momentum(p) = offset * hereMono;
				//aaPoint<float, WrapModes::WrapModes::Wrap>(density2, dst, density(p));
			}*/
			//density = density2;
            auto offsets = lx::uninitializedArrayLike(momentum);
            for(auto p : offsets.coords()) {
				offsets(p) = momentum(p) / density(p);
			}
			advect(*material, offsets);
		}


	}
    void advect(Material& material, lx::Array2D<vec2> offsets) {
		auto& density = material.density;
		auto& momentum = material.momentum;

     auto density3 = lx::Array2D<float>(sx, sy);
		auto momentum3 = lx::Array2D<vec2>(sx, sy, vec2());
		int count = 0;
		float sumOffsetY = 0; float div = 0;
      for(auto p : density.coords())
		{
			if (density(p) == 0.0f)
				continue;

			vec2 offset = offsets(p);
			sumOffsetY += abs(offset.y); div++;
			vec2 dst = vec2(p) + offset;

			vec2 newEnergy = momentum(p);
			bool bounced = false;
			for (int dim = 0; dim <= 1; dim++) {
				float maxVal = sz[dim] - 1;
				if (dst[dim] > maxVal) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = maxVal - (dst[dim] - maxVal);
					//if(dim==1)
						//cout << "dst[dim]=" << dst[dim] << endl;
					bounced = true;
				}
				if (dst[dim] < 0) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = -dst[dim];
					bounced = true;
				}
			}
			if (dst.y >= sz.y - 1)
				count++;
			//if(bounced)
			//	aaPoint<float, WrapModes::NoWrap>(bounces_dbg, dst, 1);
            lx::splatBilinearPoint<float, lx::WrapModes::MirrorWrap>(density3, dst, density(p));
			lx::splatBilinearPoint<vec2, lx::WrapModes::MirrorWrap>(momentum3, dst, newEnergy);
		}
		//cout << "bugged=" << count << endl;
		//cout << "sumOffsetY=" << sumOffsetY/div << endl;
		density = density3;
		momentum = momentum3;
	}
	template<class T, class WrapPolicy>
   static lx::Array2D<T> gauss3_forwardMapping(lx::Array2D<T> src) {
		lx::Array2D<T> dst1(src.width(), src.height());
		lx::Array2D<T> dst2(src.width(), src.height());
       for(auto p : dst1.coords()) {
			WrapPolicy::fetch(dst1, p.x - 1, p.y) += .25f * src(p);
			WrapPolicy::fetch(dst1, p.x, p.y) += .5f * src(p);
			WrapPolicy::fetch(dst1, p.x + 1, p.y) += .25f * src(p);
		}
       for(auto p : dst1.coords()) {
			//vector<float> weights = { .25, .5, .25 };
			//auto one = T(1);
			WrapPolicy::fetch(dst2, p.x, p.y - 1) += .25f * dst1(p);
			WrapPolicy::fetch(dst2, p.x, p.y) += .5f * dst1(p);
			WrapPolicy::fetch(dst2, p.x, p.y + 1) += .25f * dst1(p);

		}
		return dst2;
	}
};

export using StartupSketch = GridFluidSketch;