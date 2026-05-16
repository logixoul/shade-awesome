#pragma once

#include <cmath>
#include <cassert>
#include <algorithm>
#include <complex>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
//#include <mutex>
#include <functional>
#include <fstream>
#include <map>
#include <numeric>
#include <type_traits>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// glm doesn't compile unless I I define the following...
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include "imgui.h"

using namespace glm;
using namespace std;
