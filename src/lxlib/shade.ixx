module;
#include "precompiled.h"

export module lxlib.shade;

import lxlib.GlslProg;
import lxlib.TextureRef;
import lxlib.Array2D;
import lxlib.stuff;
import lxlib.VaoVbo;
import lxlib.TextureCache;

export namespace lx {
	struct GpuScope {
		GpuScope(string name);
		~GpuScope();
	};

	void drawRect();

	struct Str {
		string s;
		lx::Str& operator<<(string s2) {
			s += s2 + "\n";
			return *this;
		}
		lx::Str& operator<<(lx::Str s2) {
			s += s2.s + "\n";
			return *this;
		}
		operator std::string() {
			return s;
		}
	};

	struct Uniform {
		function<void(lx::GlslProgRef)> setter;
		string shortDecl;
	};

	template<class T>
	struct optional {
		T val;
		bool exists;
		optional(T const& t) { val=t; exists=true; }
		optional() { exists=false; }
	};

	template<class T> string typeToString();
	template<> inline string typeToString<float>() {
		return "float";
	}
	template<> inline string typeToString<int>() {
		return "int";
	}
	template<> inline string typeToString<ivec2>() {
		return "ivec2";
	}
	template<> inline string typeToString<vec2>() {
		return "vec2";
	}

	struct ShadeOpts
	{
		ShadeOpts();
		lx::ShadeOpts& ifmt(GLenum val) { _ifmt=val; return *this; }
		lx::ShadeOpts& scale(float val) { _scaleX=val; _scaleY=val; return *this; }
		lx::ShadeOpts& scale(float valX, float valY) { _scaleX=valX; _scaleY=valY; return *this; }
		lx::ShadeOpts& scope(std::string name) { _scopeName = name; return *this; }
		lx::ShadeOpts& targetTex(lx::gl::TextureRef val) { _targetTexs = { val }; return *this; }
		lx::ShadeOpts& targetTexs(vector<lx::gl::TextureRef> val) { _targetTexs = val; return *this; }
		lx::ShadeOpts& targetImg(lx::gl::TextureRef val) { _targetImg = val; return *this; }
		lx::ShadeOpts& dstPos(ivec2 val) { _dstPos = val; return *this; }
		lx::ShadeOpts& dstRectSize(ivec2 val) { _dstRectSize = val; return *this; }
		lx::ShadeOpts& enableResult(bool val) {
			_enableResult = val; return *this;
		}
		template<class T>
		lx::ShadeOpts& uniform(string name, T val) {
			_uniforms.push_back(lx::Uniform{
				[val, name](lx::GlslProgRef prog) { prog->uniform(name, val); },
				lx::typeToString<T>() + " " + name
				});
			return *this;
		}
		lx::ShadeOpts& vshaderExtra(string val) {
			_vshaderExtra = val;
			return *this;
		}
		lx::ShadeOpts& functions(string val) {
			_functions = val;
			return *this;
		}

		lx::optional<GLenum> _ifmt;
		float _scaleX, _scaleY;
		std::string _scopeName;
		vector<lx::gl::TextureRef> _targetTexs;
		lx::gl::TextureRef _targetImg = nullptr;
		ivec2 _dstPos;
		ivec2 _dstRectSize = ivec2(0, 0);
		bool _enableResult = true;
		vector<lx::Uniform> _uniforms;
		string _vshaderExtra;
		std::string _functions;
	};

	lx::gl::TextureRef shade(vector<lx::gl::TextureRef> const& texv, std::string const& fshader, lx::ShadeOpts const& opts = lx::ShadeOpts());
	lx::gl::TextureRef shade(lx::gl::TextureRef const& tex, std::string const& fshader, lx::ShadeOpts const& opts = lx::ShadeOpts());
}

void lx::drawRect() {
	static std::shared_ptr<lx::QuadGpu> quad = lx::createQuadVAO_VBOs();
	quad->vao.bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
  lx::VAO::unbind();
}

auto samplerSuffix = [](int i) -> string {
   return std::to_string(i);
};

auto samplerName = [](int i) -> string {
	return "tex" + samplerSuffix(i);
};

std::string getCompleteFshader(vector<lx::gl::TextureRef> const& texv, vector<lx::Uniform> const& uniforms, std::string const& fshader, std::string const& functions, string* uniformDeclarationsRet) {
	auto texIndex = [&](lx::gl::TextureRef t) {
		return std::to_string(
         std::find(texv.begin(), texv.end(), t) - texv.begin()
		);
		};
	stringstream uniformDeclarations;
	int location = 0;
	uniformDeclarations << "uniform ivec2 viewportSize;\n";
	uniformDeclarations << "uniform vec2 mouse;\n";
	for (int i = 0; i < texv.size(); i++)
	{
		string samplerType = "sampler2D";
		uniformDeclarations << "uniform " + samplerType + " " + samplerName(i) + ";\n";
		uniformDeclarations << "uniform vec2 texelSize" + samplerSuffix(i) + ";\n";
	}
	for (auto& p : uniforms)
	{
		uniformDeclarations << "uniform " + p.shortDecl + ";\n";
	}
	*uniformDeclarationsRet = uniformDeclarations.str();
	string const fullText =
       lx::Str()
		<< "#version 150"
		<< "#extension GL_ARB_explicit_uniform_location : enable"
		<< "#extension GL_ARB_texture_gather : enable"
		<< uniformDeclarations.str()
        << "in vec2 texCoord;"
		<< "out vec4 _out;"
		<< "vec4 lxTexture(sampler2D tex_) {"
		<< "	return texture(tex_, texCoord);"
		<< "}"
		<< "vec4 lxTexture() {"
	       << "	return texture(tex0, texCoord);"
		<< "}"
		<< functions
		<< "void main() {"
		<< "#line 0\n\n" // the \n\n is needed only on Intel gpus. Probably a driver bug.
		<< fshader
		<< "}"
		;
	return fullText;
}

lx::gl::TextureRef lx::shade(vector<lx::gl::TextureRef> const& texv, std::string const& fshader, lx::ShadeOpts const& opts)
{
  shared_ptr<lx::GpuScope> gpuScope;
	if (opts._scopeName != "") {
      gpuScope = make_shared<lx::GpuScope>(opts._scopeName);
	}
   static std::map<string, lx::GlslProgRef> shaders;
	lx::GlslProgRef shader;
	if(shaders.find(fshader) == shaders.end())
	{
     string uniformDeclarations;
		std::string completeFshader = getCompleteFshader(texv, opts._uniforms, fshader, opts._functions, &uniformDeclarations);
      string completeVshader = lx::Str()
			<< "#version 150"
			<< "#extension GL_ARB_explicit_attrib_location : enable"
			//<< "#extension GL_ARB_shader_image_load_store : enable"
			<< "layout(location = 0) in vec4 ciPosition;"
			<< "layout(location = 1) in vec2 ciTexCoord0;"
         << "out highp vec2 texCoord;"
			<< uniformDeclarations

			<< "void main()"
			<< "{"
			<< "	gl_Position = ciPosition * 2 - 1;"
          << "	texCoord = ciTexCoord0;"
			<< opts._vshaderExtra
			<< "}";
		try{
          shader = std::make_shared<lx::GlslProg>(completeFshader, completeVshader);
			shaders[fshader] = shader;
		} catch(std::runtime_error const& e) {
			cerr << "GlslProgCompileExc: " << e.what() << endl;
			cerr << "Fragment shader source:" << endl;
			cerr << completeFshader << endl;
			cerr << "Vertex shader source:" << endl;
			cerr << completeVshader << endl;
			throw std::runtime_error(
				string("GlslProgCompileExc: ") + e.what() +
				"\nFragment shader source:\n" + completeFshader +
				"\nVertex shader source:\n" + completeVshader
			);
		}
	} else {
		shader = shaders[fshader];
	}
	auto tex0 = texv[0];
	shader->bind();
	ivec2 viewportSize(
		floor(tex0->getWidth() * opts._scaleX),
		floor(tex0->getHeight() * opts._scaleY)
	);

	if (opts._dstRectSize != ivec2(0, 0)) {
		viewportSize = opts._dstRectSize;
	}
 vector<lx::gl::TextureRef> results;
	if (opts._enableResult) {
		if (opts._targetTexs.size() != 0)
		{
			results = opts._targetTexs;
		}
		else {
			GLenum ifmt = opts._ifmt.exists ? opts._ifmt.val : tex0->getInternalFormat();
            lx::TextureCacheKey key;
			key.ifmt = ifmt;
			key.size = viewportSize;
			key.allocateMipmaps = false;
            lx::gl::TextureRef tex = lx::TextureCache::instance()->get(key);
			results = { tex };
		}
	}
	
	int location = 0;
	shader->uniform("viewportSize", viewportSize);
	location++;
 for (int i = 0; i < texv.size(); i++) {
		shader->uniform(samplerName(i), i); texv[i]->bind(GL_TEXTURE0 + i);
      shader->uniform("texelSize" + samplerSuffix(i), vec2(1.0f) / vec2(texv[i]->getSize()));
	}
	for (auto& uniform : opts._uniforms)
	{
		uniform.setter(shader);
	}
	
	shader->bind();

	if (opts._enableResult) {
        lx::beginRTT(results);
	}
	else {
		// if we don't do that, OpenGL clamps the viewport (that we set) by the cinder window size
        lx::beginRTT(opts._targetImg);

		glColorMask(false, false, false, false);
	}
	
	{
		glViewport(0, 0, viewportSize.x, viewportSize.y);
		glDisable(GL_BLEND);
       lx::drawRect();
	}
	if (opts._enableResult) {
        lx::endRTT();
	}
	else {
        lx::endRTT();
		glColorMask(true, true, true, true);
	}
	return results[0];
}

lx::gl::TextureRef lx::shade(lx::gl::TextureRef const& tex, std::string const& fshader, lx::ShadeOpts const& opts) {
	return lx::shade(std::vector<lx::gl::TextureRef>{ tex }, fshader, opts);
}

lx::GpuScope::GpuScope(string name) {
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
}

lx::GpuScope::~GpuScope() {
	glPopDebugGroup();
}

lx::ShadeOpts::ShadeOpts() {
	//_ifmt=GL_RGBA16F;
	_scaleX = _scaleY = 1.0f;
}
