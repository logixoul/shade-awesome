#include "precompiled.h"

#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

import lxlib.SketchScaffold;

//import FftRaysSketch;
import MultiscaleGrowthSketch;

#ifdef _WIN32
int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow
) {
#else
int main() {
#endif
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
