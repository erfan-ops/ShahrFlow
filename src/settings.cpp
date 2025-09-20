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
	settings.barrier.reverse = j["mouse-barrier"]["reverse"];
	settings.barrier.fadeArea= j["mouse-barrier"]["fade-area"];

	settings.wave.speed = j["wave"]["speed"];
	settings.wave.width = j["wave"]["width"];
	settings.wave.interval = j["wave"]["interval"];
	settings.wave.color = j["wave"]["color"].get<Color>();

	settings.MSAA = j["MSAA"];

	return settings;
}
