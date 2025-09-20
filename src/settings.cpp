#include "settings.h"
#include <fstream>
#include <nlohmann/json.hpp>


Settings loadSettings(const std::string& filename) {
	std::ifstream file(filename);
	nlohmann::json j;
	file >> j;

	Settings settings;

	settings.targetFPS = j["fps"];
	settings.vsync = j["vsync"];

	settings.hexagonSize = j["hexagon-size"];

	settings.edges.width =j["edges"]["width"];
	settings.edges.color = j["edges"]["color"].get<Color>();

	settings.barrier.radius = j["mouse-barrier"]["radius"];

	settings.MSAA = j["MSAA"];

	return settings;
}
