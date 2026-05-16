module;
#include "precompiled.h"
#include "toml.hpp"

export module lxlib.ConfigManager3;

template<class T> T& getOpt_Base(std::string const& name, T defaultValue) {
	static std::map<std::string, T> m;
	if (!m.count(name)) {
		m[name] = defaultValue;
	}
	return m[name];
}

export namespace lx {
	struct ConfigManager3
	{
private:
	toml::table tbl;
public:
	ConfigManager3(std::string const filePath)
	{
		tbl = toml::parse_file(filePath);
	}
	void init() // to avoid static initialization order fiasco. Call this at the start of setup() in Sketch.
	{
		ImGuiIO& io = ImGui::GetIO();
		//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	}
	bool getBool(std::string const& name)
	{
		auto val = tbl.at_path("param." + name);
		auto& ref = getOpt_Base<bool>(name, val["default"].value_or(false));
		ImGui::Checkbox(name.c_str(), &ref);
		return ref;
	}
	//int getInt(string const& name, int min, int max, int defaultValue, ImGuiSliderFlags flags = ImGuiSliderFlags_::ImGuiSliderFlags_None);
	// Note: using value_or throughout to handle the "entire table doesn't exist" possibility
	float getFloat(std::string const& name)
	{
		auto subTable = tbl.at_path("param." + name);
		float& ref = getOpt_Base<float>(name, subTable["default"].value_or(0.5));

		ImGui::DragFloat(
			name.c_str(),
			&ref,
			subTable["speed"].value_or(.1),
			subTable["min"].value_or(-100.0),
			subTable["max"].value_or(100.0),
			"%.3f",
			subTable["logarithmic"].value_or(false) ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None);

		return ref;
	}
	void begin()
	{
		ImGui::Begin("Parameters");
	}
	void end()
	{
		ImGui::End();
	}
  };
}
