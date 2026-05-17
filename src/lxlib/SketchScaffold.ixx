module;
#include "precompiled.h"
#include <cstdlib>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

export module lxlib.SketchScaffold;

import lxlib.IntegratedConsole;
import lxlib.SketchBase;
import lxlib.stuff;

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

export namespace lx { class SketchScaffold; }

static lx::SketchScaffold* instance = nullptr;

static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

export namespace lx {
	class SketchScaffold {
private:
	GLFWwindow* window;

    shared_ptr<lx::IntegratedConsole> integratedConsole;

public:

 lx::SketchBase* sketch;

  SketchScaffold(lx::SketchBase* sketch) : sketch(sketch) {}

	void setup()
	{
		sketch->windowSize = ivec2(850, 850);
		::instance = this;

		glfwSetErrorCallback(glfw_error_callback);

		if (!glfwInit())
			throw std::runtime_error("can't initialize glfw");
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

		this->window = glfwCreateWindow(sketch->windowSize.x, sketch->windowSize.y, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
		if (window == nullptr)
			throw std::runtime_error("can't create window");
		glfwMakeContextCurrent(window);
		glfwSwapInterval(1); // Enable vsync

		glfwSetCursorPosCallback(window, cursorPositionCallback);
		glfwSetMouseButtonCallback(window, mouseButtonCallback);
		glfwSetKeyCallback(window, keyCallback);
		
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			throw std::runtime_error("could not load glad");
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 430");
		

      integratedConsole = make_shared<lx::IntegratedConsole>();;

        lx::enableDenormalFlushToZero();

       lx::disableGLReadClamp();

		sketch->setup();
	}

	// https://github.com/ocornut/imgui/blob/master/examples/example_glfw_opengl3/main.cpp
	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
			{
				ImGui_ImplGlfw_Sleep(10);
				continue;
			}
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			this->update();
			
			ImGui::Render();
			glfwGetFramebufferSize(window, &sketch->windowSize.x, &sketch->windowSize.y);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(window);
		}
	}

	~SketchScaffold() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void update()
	{
		ImGui::Begin("Parameters");
		
		sketch->update();
		sketch->draw();
		integratedConsole->update();
		ImGui::End();
	}

	void mouseMove(ivec2 pos)
	{
		sketch->mouseMove(pos);
	}
  };
}

static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    instance->mouseMove(ivec2(xpos, ypos));
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

	instance->sketch->mouseDown_[button] = action == GLFW_PRESS;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    //cout << "key: " << key << " scancode: " << scancode << " action: " << action << " mods: " << mods << endl;
    if (key >= 0 && key < 256)
    {
        key = tolower(key);
        instance->sketch->keys[key] = action != GLFW_RELEASE;
        if(action == GLFW_PRESS)
            instance->sketch->keyDown(key);
        else if (action == GLFW_RELEASE)
            instance->sketch->keyUp(key);

    }
}
