#include "precompiled.h"

#define WIN32_LEAN_AND_MEAN
#include <fstream>
#include <Windows.h>

import lxlib.SketchScaffold;

//import FftRaysSketch;
//import MultiscaleGrowthSketch;
import ParticleTraces2DSketch;

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow
) {
	try {
		StartupSketch sketch;
     lx::SketchScaffold sketchScaffold(&sketch);
		sketchScaffold.setup();
		sketchScaffold.mainLoop();
	} catch (std::exception& e) {
      std::ofstream("log.txt") << e.what() << '\n';
		return -1;
	}
	catch (...) {
       std::ofstream("log.txt") << "Unknown exception\n";
		return -1;
	}
}
