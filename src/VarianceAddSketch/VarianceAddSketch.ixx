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

export module VarianceAddSketch;

int wsx = 1280, wsy = 720;
int scale = 8;
int sx = wsx / ::scale;
int sy = wsy / ::scale;


bool paused = false;
const double kPi = 3.14159265359;
vec3 complexToColor_HSV(vec2 comp) {
	float hue = (float)kPi + (float)atan2(comp.y, comp.x);
	hue /= (float)(2 * kPi);
	float lightness = length(comp);
	lightness = .5f;
	//lightness /= lightness + 1.0f;
 lx::HslF hsl(hue, 1.0f, lightness);
	return lx::FromHSL(hsl);
}

export struct VarianceAddSketch : public lx::SketchBase {
	lx::Array2D<float> state;
	lx::Array2D<float> get_variance(lx::Array2D<float> const& in);

	void setup()
	{
     state = lx::Array2D<float>(sx, sy);
		reset();
	}
	void reset() {
      for(auto p : state.coords()) {
         state(p) = lx::randFloat();
		}
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

	void update() {
		if (paused)
			return;

		auto variance = get_variance(state);
      for(auto p : state.coords()) {
			state(p) += variance(p) * 0.1f;
		}
        state = lx::to01(state);
	}

	void draw() {
		glClearColor(0, 0, 0.7, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, wsx, wsy);
		glDisable(GL_BLEND);
        auto tex = lx::uploadTex(state);
        lx::lxDraw(tex);
	}
};

lx::Array2D<float> VarianceAddSketch::get_variance(lx::Array2D<float> const& in)
{
   lx::Array2D<float> out(in.width(), in.height(), 0.0f);

	auto clamp_index = [](int v, int maxv) {
		if (v < 0) return 0;
		if (v > maxv) return maxv;
		return v;
	};

  for (int y = 0; y < in.height(); ++y)
	{
      for (int x = 0; x < in.width(); ++x)
		{
			float sum = 0.0f;
			float sumSq = 0.0f;

			for (int dy = -1; dy <= 1; ++dy)
			{
             int yy = clamp_index(y + dy, in.height() - 1);
				for (int dx = -1; dx <= 1; ++dx)
				{
                 int xx = clamp_index(x + dx, in.width() - 1);
					float v = in(xx, yy);
					sum += v;
					sumSq += v * v;
				}
			}

			float mean = sum / 9.0f;
			out(x, y) = sumSq / 9.0f - mean * mean;
		}
	}

	return out;
}
