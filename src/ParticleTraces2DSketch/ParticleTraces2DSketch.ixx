module;
#include "precompiled.h"
#include <lxlib/simplexnoise.h>

import lxlib.Array2D;
import lxlib.stuff;
import lxlib.TextureRef;
import lxlib.gpgpu;
import lxlib.gpuBlur;
import lxlib.SketchBase;
import lxlib.shade;
import lxlib.colorspaces;
import lxlib.VaoVbo;
import lxlib.GlslProg;

export module ParticleTraces2DSketch;

int wsx = 1280, wsy = 720;
int scale = 1;
int sx = wsx / ::scale;
int sy = wsy / ::scale;

float noiseTimeDim = 0.0f;
const int MAX_AGE = 100;

bool pause;
const double M_PI = 3.14159265359;
vec3 complexToColor_HSV(vec2 comp) {
	float hue = (float)M_PI + (float)atan2(comp.y, comp.x);
	hue /= (float)(2 * M_PI);
	float lightness = length(comp);
	lightness = .5f;
	//lightness /= lightness + 1.0f;
 lx::HslF hsl(hue, 1.0f, lightness);
	return lx::FromHSL(hsl);
}

struct Walker {
	vec2 pos;
	int age;
	vec3 color;
	vec2 lastMove;

	float alpha() {
		return std::min((age / (float)MAX_AGE) * 5.0, 1.0);
	}

	Walker() {
        pos = vec2(lx::randFloat()* sx, lx::randFloat()*sy);
		age = std::rand() % MAX_AGE;
	}
	float noiseXAt(vec2 p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseX = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale, noiseTimeDim);
		return noiseX;
	}

	float noiseYAt(vec2 p) {
		int numDetailsX = 5;
		float nscale = numDetailsX / (float)sx;
		float noiseY = ::octave_noise_3d(3, .5, 1.0, p.x * nscale, p.y * nscale + numDetailsX, noiseTimeDim);
		return noiseY;
	}
	vec2 noisevec2At(vec2 p) {
		return vec2(noiseXAt(p), noiseYAt(p));
	}
	vec2 curlNoisevec2At(vec2 p) {
		float eps = 1;
		float noiseXAbove = noiseXAt(p - vec2(0, eps));
		float noiseXBelow = noiseXAt(p + vec2(0, eps));
		float noiseYOnLeft = noiseYAt(p - vec2(eps, 0));
		float noiseYOnRight = noiseYAt(p + vec2(eps, 0));
		return vec2(noiseXBelow - noiseXAbove, -(noiseYOnRight - noiseYOnLeft)) / (2.0f * eps);
	}
	void update() {
		vec2 toAdd = curlNoisevec2At(pos) * 50.0f;
		//toAdd.y -= 1.0f;
		pos += toAdd / float(::scale);
		color = complexToColor_HSV(toAdd);
		//color *= min(1.0f, age / (MAX_AGE / 40.0f));
		lastMove = toAdd;
		//color = vec3::one();

		if (pos.x < 0) pos.x += sx;
		if (pos.y < 0) pos.y += sy;
		pos.x = fmod(pos.x, sx);
		pos.y = fmod(pos.y, sy);

		age++;
	}
};

vector<Walker> walkers;

export struct ParticleTraces2DSketch : public lx::SketchBase {
	lx::GlslProgRef colorProg;

	void setup()
	{
     colorProg = std::make_shared<lx::GlslProg>(
			"#version 330\n"
			"in vec4 vColor;\n"
			"out vec4 outColor;\n"
			"void main() {\n"
			"	outColor = vColor;\n"
			"}",
			"#version 330\n"
			"in vec2 pos;\n"
			"in vec4 color;\n"
			"out vec4 vColor;\n"
			"void main() {\n"
			"	gl_Position = vec4(pos * 2.0 - 1.0, 0, 1);\n"
			"	vColor = color;\n"
			"}"
		);

      for (int i = 0; i < 4000 / lx::sq(::scale); i++) {
			walkers.push_back(Walker());
		}
	}
	void keyDown(int key)
	{
		if (key == 'p')
		{
			pause = !pause;
		}
	}
	float noiseProgressSpeed;

	void update() {
		noiseProgressSpeed = .00008f;

		if (!pause) {
			noiseTimeDim += noiseProgressSpeed;

			for(Walker & walker : walkers) {
				walker.update();
				if (walker.age > MAX_AGE) {
					walker = Walker();
				}
			}
		}
	}

	void drawPoints(std::vector<vec2> const& pos, std::vector<vec4> const& color)
	{
        lx::VAO vao;
		lx::VBO vboPos, vboColor;
		
		vao.bind();

		vboPos.setData(pos.data(), pos.size()*sizeof(pos[0]), GL_STATIC_DRAW, GL_ARRAY_BUFFER);
		vboPos.bind(GL_ARRAY_BUFFER);
		vao.defineAttrib(0, 2, GL_FLOAT, GL_FALSE, sizeof(pos[0]), (const void*)0);

		vboColor.setData(color.data(), color.size()*sizeof(color[0]), GL_STATIC_DRAW, GL_ARRAY_BUFFER);
		vboColor.bind(GL_ARRAY_BUFFER);
		vao.defineAttrib(1, 4, GL_FLOAT, GL_FALSE, sizeof(color[0]), (const void*)0);

		glDrawArrays(GL_POINTS, 0, (GLsizei)pos.size());

       lx::VBO::unbind(GL_ARRAY_BUFFER);
		lx::VAO::unbind();
	}

	int elapsedFrames = 0;
	void draw() {
		elapsedFrames++;
		auto t = elapsedFrames * 0.01f;
		//mat3 rotMat3 = glm::rotate(rotMat3, std::sin(t / 10.0f));
		//mat2 rotMat = mat2(rotMat3);
		auto refVec = vec2(sinf(t), cosf(t));

      lx::lxClear();
		static lx::Array2D<vec3> sizeSource(sx, sy);
      static auto sizeSourceTex = lx::uploadTex(sizeSource);
      static auto walkerTex = lx::shade(sizeSourceTex, "_out.rgb = vec3(0.0);");
		if (!pause) {
          walkerTex = lx::shade(walkerTex, "_out.rgb = texture().xyz * 0.993;");

			glPointSize(2.5);
			std::vector<vec4> color;
			std::vector<vec2> pos;
			{
				for(Walker & walker : walkers) {
					auto walkerColor = walker.color;
                    float hueDot = dot(refVec, lx::safeNormalized(walker.lastMove));
					hueDot = std::max(0.0f, hueDot);
					hueDot = std::max(0.0f, 1 - hueDot);
					walkerColor *= hueDot;
					auto c = vec4(walkerColor, walker.alpha());

					color.push_back(c);
					pos.push_back(walker.pos / vec2(sx, sy));
				}
			}
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				//gl::ScopedBlend(GL_SRC_ALPHA, GL_ONE);
				glViewport(0, 0, sx, sy);
				//gl::setMatricesWindow(sx, sy, true);
				colorProg->bind();

                lx::beginRTT(walkerTex);
				{
					drawPoints(pos, color);
				}
               lx::endRTT();
			}
		}
      auto walkerTexThres = lx::shade(walkerTex,
			"vec3 c = texture().xyz;"
			"float avg = dot(c, vec3(1)/3.0f);"
			"if(avg < .25)"
			"	 c = vec3(0);"
			"_out.rgb = c;"
		);
      auto walkerTexB = lx::gpuBlur::run(walkerTexThres, 4);
		auto walkerTex2 = lx::shade({ walkerTex, walkerTexB },
			"vec3 c = texture().xyz;"
			"vec3 hsl = rgb2hsl(c);"
			"hsl.z /= .5;"
			"hsl.z = min(hsl.z, 1.0);"
			"hsl.z = pow(hsl.z, 3.0);"
			"c = hsl2rgb(hsl);"
           "c += texture(tex1).xyz;"
			"_out.rgb = c;",
         lx::ShadeOpts()
				.ifmt(GL_RGB32F)
              .functions(lx::FileCache::get("stuff.fs"))
		);
		glViewport(0, 0, wsx, wsy);
		glDisable(GL_BLEND);
		lx::lxDraw(walkerTex2, lx::Rect<float>(0, 0, 1, 1));
	}
};

export using StartupSketch = ParticleTraces2DSketch;