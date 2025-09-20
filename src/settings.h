#pragma once

#include <string>
#include <vector>
#include <array>

using Color = std::array<float, 4>;

// settings structure
struct Settings {
	float targetFPS;
	bool vsync;

	Color backgroundColor;

	float hexagonSize;

	struct Cube {
		Color topColor;
		Color leftColor;
		Color rightColor;
	} cube;

	struct Edges {
		float width;
		Color color;
	} edges;

	struct Barrier {
		float radius;
		bool reverse;
		float fadeArea;
	} barrier;

	struct Wave {
		float speed;
		float width;
		float interval;
		Color color;
	} wave;

	int MSAA;
};

// Function to load settings from a JSON file
Settings loadSettings(const std::string& filename);
