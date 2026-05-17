module;
#include "precompiled.h"
#include <lxlib/macros.h>

export module ParticleFluidSketch;

import lxlib.stuff;
import lxlib.shade;
import lxlib.gpgpu;
import lxlib.Array2D_imageProc;
import lxlib.SketchBase;
import lxlib.Array2D;
import lxlib.TextureRef;

export struct ParticleFluidSketch : public lx::SketchBase {
	typedef lx::Array2D<float> Image;
	const int scale = 4;
	ivec2 sz;

	struct Particle {
		vec2 pos;
		vec2 velocity;
		vec2 force;
		float densityHere;
		float pressureHere;
		vec2 velocityContribs;
		float velocityContribsSumWeights;
	};

	vector<Particle> particles;

	void reset() {
		particles.clear();
	}

	bool pause = false;

	void setup()
	{
		sz = ivec2(windowSize.x / scale, windowSize.y / scale);

		reset();
	}
	void keyDown(int key)
	{
		if (keys['r'])
		{
			reset();
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	vec2 direction;
	vec2 lastm;
	void mouseDrag(ivec2 pos)
	{
		mm(pos);
	}
	void mouseMove(ivec2 pos)
	{
		mm(pos);
	}
	void mm(ivec2 pos)
	{
		direction = vec2(pos) - lastm;
		lastm = pos;
	}
	float surfaceTensionThreshold = 1.0f;
	float surfaceTensionCoef = 0.2f;
	void draw()
	{
		static float blurSize = 1.41f;
		ImGui::DragFloat("blurSize", &blurSize, 1.0f, 0.1, 100, "%.3f", ImGuiSliderFlags_Logarithmic);
		static int blurIters = 3;
		ImGui::DragInt("blurIters", &blurIters, 1.0f, 1, 16, "%d", ImGuiSliderFlags_None);
		static float renderThreshold = 0.07f;
		ImGui::DragFloat("renderThreshold", &renderThreshold, 1.0f, .001f, 20, "%.3f", ImGuiSliderFlags_Logarithmic);

		ImGui::DragFloat("surfaceTensionThreshold", &surfaceTensionThreshold, .1f, .001f, 20.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("surfaceTensionCoef", &surfaceTensionCoef, .1f, .001f, 20.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

      lx::lxClear();

      auto img = lx::Array2D<float>(sz);
		for (auto& particle : particles) {
          lx::splatBilinearPoint<float, lx::WrapModes::Clamp>(img, particle.pos, 1);
		}
        auto accum = lx::Array2D<float>(sz);
		for (int i = 3; i < 3 + blurIters; i++) {
           auto imgb = lx::gaussianBlur<float, lx::WrapModes::Wrap>(img, 1 + pow(2, i));
            for(auto p : img.coords()) {
				accum(p) += imgb(p);
			}
		}
		//img = gaussianBlur(img, blurSize);
		img = accum;
      auto tex = lx::uploadTex(img);
		//auto tex2 = gpuBlur::run(tex, blurSize);
		auto tex2 = tex;
      tex2 = lx::shade(tex2,
			"float f = lxTexture().x;"
			"float fw = fwidth(f);"
			"f = smoothstep(renderThreshold-fw/2, renderThreshold+fw/2, f);"
			//"f = dFdx(f)+dFdy(f);"
			"_out.rgb = vec3(0, f*2.0, f);"
           , lx::ShadeOpts().ifmt(GL_RGB16F).uniform("renderThreshold", renderThreshold)
		);

			//videoWriter->write(tex2);

       lx::lxDraw(tex2);
		//lxDraw(tex);
	}
	int elapsedFrames = 0;
	void update()
	{
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		vec2 mousePos = this->lastm;
		mousePos /= scale;
		if (mouseDown_[0])
		{
			static float t = 0.0f;

			for (int i = 0; i < 5; i++) {
				Particle part; part.pos = mousePos + vec2(sin(t), cos(t)) * 30.0f;
				particles.push_back(part);
				t++;
			}
		}
		else if (mouseDown_[1]) {
			for (Particle& part : particles) {
				if (distance(part.pos, mousePos) < 40) {
					const float velocityScaleFactor = 0.6f / (float)scale;
					part.velocity += velocityScaleFactor * direction;
					float speed = glm::length(part.velocity);
					float newSpeed = std::min(speed, velocityScaleFactor * 30);
					part.velocity = part.velocity * newSpeed / speed;
				}
			}
		}
		elapsedFrames++;
	}

	float steepKernel(float dist, float radius) {
		if (dist >= radius) return 0.0f;
		float x = 1.0f - dist / radius;
		return x * x;
	}

	float smoothKernel(float dist, float radius) {
		if (dist >= radius) return 0.0f;
		return glm::smoothstep(1.0f, 0.0f, dist / radius);
	}

	const float MAX_DIST = 20;
	void forEachNeighbourPair(std::function<void(Particle&, Particle&, vec2 const&, float)> const& cb) {
		for (int i = 0; i < particles.size(); i++) {
			auto& p1 = particles[i];
			for (int j = i + 1; j < particles.size(); j++) {
				auto& p2 = particles[j];
				auto vec = p1.pos - p2.pos;
				float dist = length(vec);
				if (dist <= MAX_DIST) {
					cb(p1, p2, vec, dist);
				}
			}
		}
	}

	void lxComputeForces() {
		for (auto& p : particles) {
			p.densityHere = 0;
			p.pressureHere = 0;
		}

		forEachNeighbourPair([&](auto& p1, auto& p2, vec2 const& vec, float dist) {
			auto vecNorm = vec / dist; // normalized
			float f = steepKernel(dist, MAX_DIST);

			float densityToAdd = f;
			p1.densityHere += densityToAdd;
			p2.densityHere += densityToAdd;
			});

		for (auto& p : particles) {
			p.pressureHere = p.densityHere - surfaceTensionThreshold;
		}

		forEachNeighbourPair([&](auto& p1, auto& p2, vec2 const& vec, float dist) {
			auto vecNorm = vec / (dist + 0.0001f); // normalized
			float f = steepKernel(dist, MAX_DIST);
			vec2 pushawayVec = vecNorm * f;
			const float pressureSum = p1.pressureHere + p2.pressureHere;
			p1.force += pushawayVec * surfaceTensionCoef * pressureSum;
			p2.force -= pushawayVec * surfaceTensionCoef * pressureSum;
			});

		smoothenVelocities();
	}

	void smoothenVelocities() {
		for (auto& p : particles) {
			float f = smoothKernel(0.0f, MAX_DIST);
			p.velocityContribs = p.velocity * f;
			p.velocityContribsSumWeights = f;
		}
		forEachNeighbourPair([&](Particle& p1, Particle& p2, vec2 const& vec, float dist) {
			float f = smoothKernel(dist, MAX_DIST);
			p1.velocityContribs += p2.velocity * f;
			p2.velocityContribs += p1.velocity * f;
			p1.velocityContribsSumWeights += f;
			p2.velocityContribsSumWeights += f;
			});
		for (auto& p : particles) {
			p.velocity = p.velocityContribs / p.velocityContribsSumWeights;
		}
	}


	void doFluidStep() {
		const auto maxX = sz.x;
		const auto maxY = sz.y;
		lxComputeForces();
		const float dampening = 0.75;
		for (auto& p : particles) {
			if (p.pos.x < 0 || p.pos.x > maxX) {
				p.velocity.x *= -1;
				p.velocity.x *= dampening;
			}
			if (p.pos.y < 0 || p.pos.y > maxY) {
				p.velocity.y *= -1;
				p.velocity.y *= dampening;
			}
			if (p.pos.x < 0) {
				p.pos.x = -p.pos.x;
			}
			if (p.pos.x > maxX) {
				p.pos.x = maxX - (p.pos.x - maxX);
			}
			if (p.pos.y < 0) {
				p.pos.y = -p.pos.y;
			}
			if (p.pos.y > maxY) {
				p.pos.y = maxY - (p.pos.y - maxY);
			}
		}
		for (auto& p : particles) {
			p.velocity += p.force + vec2(0, 0.07);
			p.pos += p.velocity;
			p.force = vec2(0, 0);
		}
	}
};
