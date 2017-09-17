#include "load_save_png.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <map>

static GLuint compile_shader(GLenum type, std::string const& source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

struct SpriteData {
	glm::vec2 min_uv = glm::vec2(0.0f);
	glm::vec2 max_uv = glm::vec2(0.5f);
	glm::vec2 center = glm::vec2(2.0f);
};

std::vector<SpriteData> sprites;

enum SpriteInfo {
	A,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	SPACE,
	MAP_RIGHT,
	MAP_LEFT,
	MAP_MIDDLE,
	PLAYER,
	PLAYER_HOLDING,
	CRYSTAL,
	APPLE,
	BOARDS,
	BRIDGE,
	PICKAXE,
	LONG_KNIFE,
	KEY,
	PICKAXE_HEAD,
	ROPE,
	KNIFE,
	COIN,
	HOLE,
	STICK,
	ROD,
	ROCK,
	DOOR,
	SCALE,
	SCALE_UNBALANCED
};

// from https://stackoverflow.com/a/33971769
// I think enum bitmaps are cool so even though this is verbose/overkill I put it in
enum class Workbench : uint32_t {
	EMPTY = 0,
	HAS_BOARDS = (1 << 0),
	HAS_ROPE = (1 << 1),
	HAS_PICK_HEAD = (1 << 2),
	HAS_STICK = (1 << 3),
	HAS_KNIFE = (1 << 4),
	HAS_ROD = (1 << 5)
};
constexpr enum Workbench operator|(const enum Workbench self, const enum Workbench other) {
	return (enum Workbench)(uint32_t(self) | uint32_t(other));
}
constexpr enum Workbench operator&(const enum Workbench self, const enum Workbench other) {
	return (enum Workbench)(uint32_t(self) & uint32_t(other));
}
const enum Workbench CAN_BUILD_BRIDGE = Workbench::HAS_BOARDS & Workbench::HAS_ROPE;
const enum Workbench CAN_BUILD_PICKAXE = Workbench::HAS_PICK_HEAD & Workbench::HAS_STICK;
const enum Workbench CAN_BUILD_KNIFE = Workbench::HAS_KNIFE & Workbench::HAS_ROD;

// Code inspired from
// https://github.com/ixchow/15-466-f17-base2/blob/bbda559b9156f5b539f6fab33f45fa684325d6c2/Meshes.cpp
void load_sprite_info(std::string const& filename) {
	std::ifstream file(filename, std::ios::binary);

	{
		struct Header {
			uint32_t size = 0;
			uint32_t padding;
		} header;
		static_assert(sizeof(Header) == 8, "Header is packed");

		if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) {
			std::cerr << "Failed to read header" << std::endl;
		}

		sprites.resize(header.size / sizeof(SpriteData));
		static_assert(sizeof(SpriteData) == 6 * 4, "SpriteData is packed");

		if (!file.read(reinterpret_cast<char*>(&sprites[0]), sprites.size() * sizeof(SpriteData))) {
			std::cerr << "Reading sprite info failed" << std::endl;
		}
	}
}

int main(int argc, char** argv) {
	// Configuration:
	struct {
		std::string title = "Escape The Courtyard";
		glm::uvec2 size = glm::uvec2(640, 480);
	} config;

	load_sprite_info("assets/stuff.file");

	//------------  initialization ------------

	// Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	// Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	// create window:
	SDL_Window* window =
			SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, config.size.x,
											 config.size.y, SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
											 );

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	// Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

#ifdef _WIN32
	// On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
#endif

	// Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	// Hide mouse cursor (note: showing can be useful for debugging):
	SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	// texture:
	GLuint tex = 0;
	glm::uvec2 tex_size = glm::uvec2(0, 0);

	{	// load texture 'tex':
		std::vector<uint32_t> data;
		if (!load_png("assets/stuff.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
			std::cerr << "Failed to load texture." << std::endl;
			exit(1);
		}
		// create a texture object:
		glGenTextures(1, &tex);
		// bind texture object to GL_TEXTURE_2D:
		glBindTexture(GL_TEXTURE_2D, tex);
		// upload texture data from data:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
		// set texture sampling parameters:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_TexCoord = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_tex = 0;
	{	// compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
																					"#version 330\n"
																					"uniform mat4 mvp;\n"
																					"in vec4 Position;\n"
																					"in vec2 TexCoord;\n"
																					"in vec4 Color;\n"
																					"out vec2 texCoord;\n"
																					"out vec4 color;\n"
																					"void main() {\n"
																					"	gl_Position = mvp * Position;\n"
																					"	color = Color;\n"
																					"	texCoord = TexCoord;\n"
																					"}\n");

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
																						"#version 330\n"
																						"uniform sampler2D tex;\n"
																						"in vec4 color;\n"
																						"in vec2 texCoord;\n"
																						"out vec4 fragColor;\n"
																						"void main() {\n"
																						"	fragColor = texture(tex, texCoord) * color;\n"
																						"}\n");

		program = link_program(fragment_shader, vertex_shader);

		// look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U)
			throw std::runtime_error("no attribute named Position");
		program_TexCoord = glGetAttribLocation(program, "TexCoord");
		if (program_TexCoord == -1U)
			throw std::runtime_error("no attribute named TexCoord");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U)
			throw std::runtime_error("no attribute named Color");

		// look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U)
			throw std::runtime_error("no uniform named mvp");
		program_tex = glGetUniformLocation(program, "tex");
		if (program_tex == -1U)
			throw std::runtime_error("no uniform named tex");
	}

	// vertex buffer:
	GLuint buffer = 0;
	{	// create vertex buffer
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}

	struct Vertex {
		Vertex(glm::vec2 const& Position_, glm::vec2 const& TexCoord_, glm::u8vec4 const& Color_)
				: Position(Position_), TexCoord(TexCoord_), Color(Color_) {}
		glm::vec2 Position;
		glm::vec2 TexCoord;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 20, "Vertex is nicely packed.");

	// vertex array object:
	GLuint vao = 0;
	{	// create vao and set up binding:
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte*)0);
		glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte*)0 + sizeof(glm::vec2));
		glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
													(GLbyte*)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
		glEnableVertexAttribArray(program_Position);
		glEnableVertexAttribArray(program_TexCoord);
		glEnableVertexAttribArray(program_Color);
	}

	//------------ sprite info -----------

	auto load_sprite = [](SpriteInfo name) -> SpriteData { return sprites[name]; };

	//------------ game state ------------

	glm::vec2 mouse = glm::vec2(0.0f, 0.0f);	// mouse position in [-1,1]x[-1,1] coordinates

	struct {
		glm::vec2 at = glm::vec2(0.0f, 0.0f);
		glm::vec2 radius = glm::vec2(16.0f, 12.0f);
	} camera;
	// correct radius for aspect ratio:
	camera.radius.x = camera.radius.y * (float(config.size.x) / float(config.size.y));

	struct BoundingBox {
		glm::vec2 min;
		glm::vec2 max;
		glm::vec2 center;
		glm::vec2 radius;

		BoundingBox(){};
		BoundingBox(glm::vec2 center, glm::vec2 radius) { set(center, radius); };

		void set(const glm::vec2& cent, const glm::vec2& rad) {
			center = cent;
			radius = rad;
			min.x = center.x - rad.x;
			min.y = center.y - rad.y;
			max.x = center.x + rad.x;
			max.y = center.y + rad.y;
		}

		// AABB from
		// https://developer.mozilla.org/en-US/docs/Games/Techniques/2D_collision_detection#Axis-Aligned_Bounding_Box
		bool contains(const BoundingBox& other) const {
			return (min.x < other.max.x && max.x > other.min.x && min.y < other.max.y && max.y > other.min.y);
		}
	};

	struct Circle {
		glm::vec2 center;
		float radius;

		Circle(){};
		Circle(glm::vec2 center, float radius) : center(center), radius(radius){};

		bool contains(const glm::vec2& point) const {
			float dx = center.x - point.x;
			float dy = center.y - point.y;
			float distance = sqrt(dx * dx + dy * dy);

			return distance < radius;
		}
	};

	struct Hint {
		Circle circle;
		std::string hint;

		Hint(Circle circle, std::string hint) : circle(circle), hint(hint){};
	};

	SpriteInfo currentMap = MAP_MIDDLE;

	std::map<SpriteInfo, std::vector<Hint>> hints;

	Circle treeCircle({10.25f, 4.75f}, 3.0f);
	hints[MAP_LEFT].emplace_back(treeCircle, "LOOK UP");
	hints[MAP_LEFT].emplace_back(Circle({-5.0f, 2.0f}, 2.0f), "I LEFT WITHOUT A TRACE");
	hints[MAP_RIGHT].emplace_back(Circle({3.0f, 13.5f}, 8.0f), "IM ALWAYS RIGHT");

	Circle workbench = Circle({12.25f, -15.0f}, 8.0f);
	Workbench workbenchState = Workbench::EMPTY;
	(void)workbenchState;
	hints[MAP_MIDDLE].emplace_back(workbench, "FIND SOMETHING TO BUILD");

	std::map<SpriteInfo, std::vector<BoundingBox>> mapCollisions = {{MAP_RIGHT, {}}, {MAP_LEFT, {}}, {MAP_MIDDLE, {}}};

	BoundingBox castleWall(glm::vec2(0.0f, 8.5f), glm::vec2(32.0f, 1.0f));

	mapCollisions[MAP_LEFT].emplace_back(castleWall);
	mapCollisions[MAP_LEFT].emplace_back(glm::vec2(-14.8f, 0.0f), glm::vec2(1.0f, 24.0f));
	mapCollisions[MAP_LEFT].emplace_back(glm::vec2(-8.1f, 2.0f), glm::vec2(2.0f, 3.0f));
	mapCollisions[MAP_LEFT].emplace_back(glm::vec2(-11.0f, 2.0f), glm::vec2(0.5f, 2.0f));
	mapCollisions[MAP_LEFT].emplace_back(glm::vec2(-12.0f, 1.5f), glm::vec2(0.8f, 1.5f));
	BoundingBox bridge(glm::vec2(-4.5f, 2.0f), glm::vec2(1.0f, 1.5f));
	mapCollisions[MAP_LEFT].emplace_back(glm::vec2(-4.5, 0.5f), glm::vec2(0.25f, 0.25f));


	// tree
	mapCollisions[MAP_LEFT].emplace_back(glm::vec2(9.875f, 8.25f), glm::vec2(1.625f, 4.0f));

	mapCollisions[MAP_MIDDLE].emplace_back(castleWall);

	// workbench
	mapCollisions[MAP_MIDDLE].emplace_back(glm::vec2(11.05f, -11.0f), glm::vec2(3.35f, 2.0f));

	mapCollisions[MAP_RIGHT].emplace_back(castleWall);

	// right wall
	mapCollisions[MAP_RIGHT].emplace_back(glm::vec2(15.5f, 0.0f), glm::vec2(1.0f, 40.0f));

	// rocks
	mapCollisions[MAP_RIGHT].emplace_back(glm::vec2(-0.55f, 0.4f), glm::vec2(0.9f, 0.3f));
	mapCollisions[MAP_RIGHT].emplace_back(glm::vec2(7.07f, 3.25f), glm::vec2(0.6f, 0.45f));
	mapCollisions[MAP_RIGHT].emplace_back(glm::vec2(3.5f, -4.0f), glm::vec2(0.6f, 0.25f));

	struct Object {
		glm::vec2 at = glm::vec2(0.0f);
		glm::vec2 radius = glm::vec2(1.0f);
		SpriteData sprite;
		BoundingBox bounds;

		Object(){};
		Object(glm::vec2 at, glm::vec2 radius, SpriteData sprite, BoundingBox bounds)
				: at(at), radius(radius), sprite(sprite), bounds(bounds){};
	} player;
	const glm::vec2 PLAYER_SPEED = glm::vec2(10.0f, 8.5f);
	player.at = glm::vec2(-1.0f);
	player.radius = glm::vec2(0.5f, 1.0f);
	player.sprite = load_sprite(PLAYER);
	player.bounds.set(player.at, player.radius);

	struct Item {
		Object obj;
		Circle circle;
		Workbench addition;
		std::string name;

		Item(){};
		Item(Object obj, Circle circle, Workbench addition, std::string name="") : obj(obj), circle(circle), addition(addition), name(name){};
	};

	Circle leftPillar({-3.5f, 0.0f}, 2.0f);
	Circle topPillar({0.0f, 3.5f}, 2.0f);
	Circle rightPillar({3.5f, 0.0f}, 2.0f);
	Circle bottomPillar({0.0f, -3.5f}, 2.0f);

	Item* playerItem = nullptr;

	std::map<SpriteInfo, std::vector<Item>> items;

	// back hack to try to keep playerItem pointers valid while still using vector :/
	// talk about stupid code
	items[MAP_RIGHT].reserve(100);
	items[MAP_LEFT].reserve(100);
	items[MAP_MIDDLE].reserve(100);

	items[MAP_LEFT].emplace_back(
		Object({7.0f, 10.25f}, {0.5f, 0.5f}, load_sprite(APPLE), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
		Circle({0.0f, 0.0f}, 0.0f), Workbench::EMPTY, "APPLE");
	items[MAP_LEFT].emplace_back(
		Object({-6.0f, 2.25f}, {0.35f, 0.75f}, load_sprite(CRYSTAL), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
		Circle({-6.0f, 2.25f}, 0.75f), Workbench::EMPTY, "CRYSTAL");

	items[MAP_LEFT].emplace_back(
		Object({-6.0f, -8.0f}, {0.7f, 0.5f}, load_sprite(ROPE), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
		Circle({-6.0f, -8.0f}, 0.75f), Workbench::HAS_ROPE);

	items[MAP_LEFT].emplace_back(
		Object({0.0f, 0.0f}, {0.7f, 0.5f}, load_sprite(BOARDS), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
		Circle({0.0f, 0.0f}, 0.75f), Workbench::HAS_BOARDS);

	Item door(Object({0.1f, 8.75f}, {1.5f, 2.5f}, load_sprite(DOOR), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
						Circle({0.0f, 0.0f}, 0.5f), Workbench::EMPTY);

	Item scale(Object({-9.0f, 2.0f}, {2.0f, 1.5f}, load_sprite(SCALE), BoundingBox({-9.0f, 2.0f}, {0.0f, 0.0f})),
						 Circle({-7.0f, 2.0f}, 1.0f), Workbench::EMPTY);

	hints[MAP_RIGHT].emplace_back(Circle(scale.obj.at, 3.0f), "SETTLE ME DOWN");

	// for digging on right map
	Circle hole({7.0f, -5.0f}, 0.5f);
	bool holeDug = false;

	Object map;
	map.radius = glm::vec2(16.0f, 12.0f);
	map.sprite = load_sprite(currentMap);

	bool hasPickaxe = true;
	bool hasBridge = false;
	bool hasKnife = true;

	bool correctLeft = false;
	bool correctRight = false;
	bool correctMiddle = false;
	bool correctTop = false;
	bool correctBottom = false;

	int keyCount;
	const uint8_t* keys = SDL_GetKeyboardState(&keyCount);
	uint8_t prevKeys[keyCount];
	std::memcpy(prevKeys, keys, sizeof prevKeys);

	std::string hint = "PRESS C TO INTERACT";
	float hintTimer = -10.0f;	// start off giving extra time

	//------------ game loop ------------

	bool should_quit = false;
	unsigned random = 0;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			// handle input:
			if (evt.type == SDL_MOUSEMOTION) {
				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) * -2.0f + 1.0f;
			} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_KEYDOWN) {
				if (evt.key.keysym.sym == SDLK_ESCAPE) {
					should_quit = true;
				}
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit)
			break;

		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration<float>(current_time - previous_time).count();
		previous_time = current_time;

		glm::vec2 delta;
		{	// update game state:
			++random;

			std::cout << player.at.x << " " << player.at.y << std::endl;

			hintTimer += elapsed;

			delta = glm::vec2(0.0f);

			if (keys[SDL_SCANCODE_A]) {
				delta.x -= PLAYER_SPEED.x * elapsed;
				if (player.radius.x < 0.0f) {
					player.radius.x = -player.radius.x;
				}
			}

			if (keys[SDL_SCANCODE_D]) {
				delta.x += PLAYER_SPEED.x * elapsed;
				if (player.radius.x > 0.0f) {
					player.radius.x = -player.radius.x;
				}
			}

			if (keys[SDL_SCANCODE_W]) {
				delta.y += PLAYER_SPEED.y * elapsed;
			}

			if (keys[SDL_SCANCODE_S]) {
				delta.y -= PLAYER_SPEED.y * elapsed;
			}

			if (keys[SDL_SCANCODE_C] && !prevKeys[SDL_SCANCODE_C]) {
				bool done = false;

				if (playerItem) {
					if (currentMap == MAP_MIDDLE) {
						if (workbench.contains(player.at)) {
							if (!hasBridge && (workbenchState & CAN_BUILD_BRIDGE) == CAN_BUILD_BRIDGE) {
								hasBridge = true;
								bridge.set({-100.0f, -100.0f}, {0.0f, 0.0f});
							} else if (!hasKnife && (workbenchState & CAN_BUILD_KNIFE) == CAN_BUILD_KNIFE) {
								hasKnife = true;
							} else if (!hasPickaxe && (workbenchState & CAN_BUILD_PICKAXE) == CAN_BUILD_PICKAXE) {
								hasPickaxe = true;
							}

							std::cout << "Crash?" << std::endl;
							// hide old object and radius
							playerItem->obj.radius.x = 0.0f;
							playerItem->circle.radius = 0.0f;
							playerItem = nullptr;
							std::cout << "Cras..." << std::endl;
							
							done = true;
						}

						// get or set item on pillars
						if (playerItem->name == "CRYSTAL" && bottomPillar.contains(player.at)) {
							// hide old object and radius
							playerItem->obj.radius.x = 0.0f;
							playerItem->circle.radius = 0.0f;
							playerItem = nullptr;
							done = true;
						}

						if (playerItem->name == "ROCK" && topPillar.contains(player.at)) {
							// hide old object and radius
							playerItem->obj.radius.x = 0.0f;
							playerItem->circle.radius = 0.0f;
							playerItem = nullptr;
							done = true;
						}

						if (playerItem->name == "COIN" && rightPillar.contains(player.at)) {
							// hide old object and radius
							playerItem->obj.radius.x = 0.0f;
							playerItem->circle.radius = 0.0f;
							playerItem = nullptr;
							done = true;
						}

						if (playerItem->name == "APPLE" && leftPillar.contains(player.at)) {
							// hide old object and radius
							playerItem->obj.radius.x = 0.0f;
							playerItem->circle.radius = 0.0f;
							playerItem = nullptr;
							done = true;
						}
					}
				} else {
					// dig hole
					if (!done && currentMap == MAP_RIGHT && !holeDug && hole.contains(player.at)) {
						if (hasPickaxe) {
							items[MAP_RIGHT].emplace_back(Object(hole.center, {hole.radius - 0.15f, hole.radius - 0.1f},
																									 load_sprite(COIN), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
																						Circle(hole.center, hole.radius), Workbench::EMPTY, "COIN");
							holeDug = true;
							hint = "YOU FOUND SOMETHING";
							hintTimer = 5.0f;
							done = true;
						} else {
							hint = "YOU NEED A TOOL";
							done = true;
							hintTimer = 0.0f;
						}
					}

					// cut apple
					if (!done && currentMap == MAP_LEFT && hasKnife && treeCircle.contains(player.at)) {
						playerItem = &items[MAP_LEFT][0];
						hint = "NICE FIND";
						hintTimer = 0.0f;
						done = true;
					}

					// grab from scale
					if (!done && currentMap == MAP_RIGHT && scale.circle.contains(player.at)) {
						scale.obj.sprite = load_sprite(SCALE_UNBALANCED);
						items[MAP_RIGHT].emplace_back(Object(scale.circle.center, {0.5f, 0.5f},
							load_sprite(ROCK), BoundingBox({0.0f, 0.0f}, {0.0f, 0.0f})),
			 Circle(scale.circle.center, 0.5f), Workbench::EMPTY, "ROCK");
						playerItem = &items[MAP_RIGHT].back();
						done = true;
					}

					// grab items
					for (auto item = items[currentMap].begin(); !done && item != items[currentMap].end(); ++item) {
						if (item->circle.contains(player.at)) {
							playerItem = &(*item);
							hintTimer = 20.0f;	// remove any hint
							done = true;
						}
					}
				}

				for (auto h = hints.at(currentMap).cbegin(); !done && h != hints.at(currentMap).cend(); ++h) {
					if (h->circle.contains(player.at)) {
						hint = h->hint;
						done = true;
						hintTimer = 0.0f;
					}
				}
				if (!done) {
					if (random % 5 == 0) {
						hint = "MAKE SOME TOOLS";
					} else {
						hint = "FIND SOMETHING MEANINGFUL";
					}
					std::cout << random <<" " << random % 5 << std::endl;
					hintTimer = 8.0f;	// start timer late because this hint sucks
				}
			}

			player.bounds.set(player.at + delta, player.radius);

			for (const BoundingBox& box : mapCollisions.at(currentMap)) {
				if (box.contains(player.bounds)) {
					delta.x = delta.y = 0.0f;
				}
			}

			if (bridge.contains(player.bounds)) {
				delta.x = delta.y = 0.0f;
			}

			if (player.bounds.min.y < -12.0f) {
				delta.y = 0.0f;
			}

			if (player.bounds.max.x > camera.radius.x - 0.25f) {
				if (currentMap == MAP_MIDDLE) {
					player.at.x = -camera.radius.x + 0.25f + player.radius.x;
					currentMap = MAP_RIGHT;
					map.sprite = load_sprite(currentMap);
				} else if (currentMap == MAP_LEFT) {
					player.at.x = -camera.radius.x + 0.25f + player.radius.x;
					currentMap = MAP_MIDDLE;
					map.sprite = load_sprite(currentMap);
				}
			} else if (player.bounds.min.x < -camera.radius.x + 0.25f) {
				if (currentMap == MAP_MIDDLE) {
					player.at.x = camera.radius.x - 0.25f - player.radius.x;
					currentMap = MAP_LEFT;
					map.sprite = load_sprite(currentMap);
				} else if (currentMap == MAP_RIGHT) {
					player.at.x = camera.radius.x - 0.25f - player.radius.x;
					currentMap = MAP_MIDDLE;
					map.sprite = load_sprite(currentMap);
				}
			}

			player.at += delta;
			player.bounds.set(player.at, player.radius);
		}

		// draw output:
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		{	// draw game state:
			std::vector<Vertex> verts;

			static auto draw_sprite = [&verts](SpriteData const& sprite, glm::vec2 const& rad, glm::vec2 const& at,
																				 glm::u8vec4 tint = glm::u8vec4(0xff, 0xff, 0xff, 0xff), float angle = 0.0f) {
				glm::vec2 min_uv = sprite.min_uv;
				glm::vec2 max_uv = sprite.max_uv;
				glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
				glm::vec2 up = glm::vec2(-right.y, right.x);

				verts.emplace_back(at + right * -rad.x + up * -rad.y, glm::vec2(min_uv.x, min_uv.y), tint);
				verts.emplace_back(verts.back());
				verts.emplace_back(at + right * -rad.x + up * rad.y, glm::vec2(min_uv.x, max_uv.y), tint);
				verts.emplace_back(at + right * rad.x + up * -rad.y, glm::vec2(max_uv.x, min_uv.y), tint);
				verts.emplace_back(at + right * rad.x + up * rad.y, glm::vec2(max_uv.x, max_uv.y), tint);
				verts.emplace_back(verts.back());
			};

			static auto draw_word = [&](const std::string& word, const glm::vec2& at) {
				for (unsigned i = 0; i < word.length(); i++) {
					SpriteData sprite;
					if (word[i] == ' ') {
						sprite = load_sprite(SPACE);
					} else {
						sprite = load_sprite(static_cast<SpriteInfo>(word[i] - 'A'));
					}
					draw_sprite(sprite, glm::vec2(0.5f, 0.6f), glm::vec2(at.x + float(i), at.y));
				}
			};

			draw_sprite(map.sprite, map.radius, map.at);

			if (currentMap == MAP_RIGHT && holeDug) {
				draw_sprite(load_sprite(HOLE), {hole.radius + 0.5f, hole.radius + 0.2f}, hole.center);
			}

			if (currentMap == MAP_RIGHT) {
				draw_sprite(scale.obj.sprite, scale.obj.radius, scale.obj.at);
			}

			if (hasBridge) {
				if (currentMap == MAP_LEFT) {
					draw_sprite(load_sprite(BRIDGE), {1.0f, 0.7f}, {-3.4f, 2.0f});
				}
				draw_sprite(load_sprite(BRIDGE), {0.5f, 0.35f}, {-13.0f, 11.0f});
			}

			if (playerItem != nullptr) {
				draw_sprite(load_sprite(PLAYER_HOLDING), player.radius, player.at);
				draw_sprite(playerItem->obj.sprite, playerItem->obj.radius, player.at + glm::vec2(0.0f, 0.5f));
			} else {
				draw_sprite(load_sprite(PLAYER), player.radius, player.at);
			}

			bool win = correctLeft && correctRight && correctMiddle && correctTop && correctBottom;
			if (win) {
				draw_word("YOU WIN", { -3.0f, -8.0f});
			} else if (currentMap == MAP_MIDDLE) {
				draw_sprite(door.obj.sprite, door.obj.radius, door.obj.at);
			}

			for (const Item& item : items[currentMap]) {
				if (&item != playerItem) {
					draw_sprite(item.obj.sprite, item.obj.radius, item.obj.at);
				}
			}

			if (hasPickaxe) {
				draw_sprite(load_sprite(PICKAXE), {0.5f, 0.5f}, {-15.0f, 11.0f});
			}

			if (hasKnife) {
				draw_sprite(load_sprite(LONG_KNIFE), {0.5f, 0.5f}, {-11.5f, 11.0f});
			}

			if (correctBottom) {
				draw_sprite(load_sprite(CRYSTAL), {1.0f, 0.5f}, bottomPillar.center);
			}

			if (correctRight) {
				draw_sprite(load_sprite(COIN), {0.5f, 0.6f}, rightPillar.center);
			}

			if (correctLeft) {
				draw_sprite(load_sprite(APPLE), {0.75f, 0.75f}, leftPillar.center);
			}

			if (correctTop) {
				draw_sprite(load_sprite(ROCK), {0.75f, 0.75f}, topPillar.center);
			}

			if (hintTimer < 10.0f) {
				draw_word(hint, {-15.2f, -11.2f});
			}

			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);

			glUseProgram(program);
			glUniform1i(program_tex, 0);
			glm::vec2 scale = 1.0f / camera.radius;
			glm::vec2 offset = scale * -camera.at;
			glm::mat4 mvp = glm::mat4(glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
																glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(offset.x, offset.y, 0.0f, 1.0f));
			glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

			glBindTexture(GL_TEXTURE_2D, tex);
			glBindVertexArray(vao);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());
		}

		SDL_GL_SwapWindow(window);

		std::memcpy(prevKeys, keys, sizeof prevKeys);
	}

	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}

static GLuint compile_shader(GLenum type, std::string const& source) {
	GLuint shader = glCreateShader(type);
	GLchar const* str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector<GLchar> info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector<GLchar> info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
