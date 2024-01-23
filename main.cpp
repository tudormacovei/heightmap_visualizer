#include <SDL.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include "lodepng.h"
#include "Eigen/Core"
#include "Eigen/Geometry"


// Screen dimension constants
const int SCREEN_WIDTH = 950;
const int SCREEN_HEIGHT = 600;

// We store lines as 3x2 matrices (two 3D points)
typedef Eigen::Matrix<float, 3, 2> Line3f;

Eigen::Vector3f camera_position(1.0f, 1.0f, 1.0f);

float perspective_factor = camera_position.norm();

void to_pixel_coordinates(std::vector<Line3f> &lines) {
	for (Line3f& line : lines) {
		// TODO move this
		// center the plane on 0
		line.col(0).x() -= 0.5f;
		line.col(1).x() -= 0.5f;
		line.col(0).y() -= 0.5f;
		line.col(1).y() -= 0.5f;
	}
	
	Eigen::Vector3f focus_point(0.0f, 0.0f, 0.0f);
	Eigen::Vector3f camera_look_at = camera_position - focus_point ;
	camera_look_at.normalize();
	
	Eigen::Vector3f camera_up(0.0f, 0.0f, 1.0f);
	
	Eigen::Vector3f camera_right = camera_up.cross(camera_look_at);
	camera_right.normalize();
	
	camera_up = camera_look_at.cross(camera_right);
	camera_up.normalize();

	Eigen::Matrix3f view_matrix;
	// these lines build the matrix that transforms from view coords to world coords
	view_matrix.col(0) << camera_right;
	view_matrix.col(1) << camera_up;
	view_matrix.col(2) << camera_look_at;

	// we take the transpose of it to take its inverse
	view_matrix.transposeInPlace();

	Eigen::Vector3f translation;
	translation.x() = camera_position.dot(camera_right);
	translation.y() = camera_position.dot(camera_up);
	translation.z() = camera_position.dot(camera_look_at);
	
	for (Line3f& line : lines) {
		line.col(0) -= camera_position;
		line.col(1) -= camera_position;
		line = view_matrix * line;
		
		// perspective division
		if (line.col(0).z() < -1)
			line.col(0) *= perspective_factor / abs(line.col(0).z());
		if (line.col(1).z() < -1)
			line.col(1) *= perspective_factor / abs(line.col(1).z());

		float zoom_factor = 500.0f;
		line *= zoom_factor;

		// transform to pixel coordinates here
		line.col(0).y() += SCREEN_HEIGHT / 2.0f;
		line.col(1).y() += SCREEN_HEIGHT / 2.0f;

		line.col(0).x() += SCREEN_WIDTH / 2.0f;
		line.col(1).x() += SCREEN_WIDTH / 2.0f;
	}
}

void lines_from_heightmap(int** heightmap, int width, int height, std::vector<Line3f> &lines) {
	// heightmap will displace a unit rectangle with corners at (0,0,0) and (1, 1, 0)
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			float z_fact = 0.2f;
			float z_trans = 0.0f;
			
			Eigen::Vector3f current(i, j, z_fact * heightmap[i][j] / 255.0f + z_trans);

			if (i > 0)
			{
				Eigen::Vector3f up(i - 1.0f, j, z_fact* heightmap[i - 1][j] / 255.0f + z_trans);
				Line3f temp;
				temp << current, up;
				temp.col(0).x() /= height;
				temp.col(1).x() /= height;
				temp.col(0).y() /= width;
				temp.col(1).y() /= width;
				lines.push_back(temp);
			}
			if (j > 0)
			{
				Eigen::Vector3f left(i, j - 1.0f, z_fact* heightmap[i][j - 1] / 255.0f + z_trans);
				Line3f temp;
				temp << current, left;
				temp.col(0).x() /= height;
				temp.col(1).x() /= height;
				temp.col(0).y() /= width;
				temp.col(1).y() /= width;
				lines.push_back(temp);
			}

		}
	}
}

// TODO find a more efficient way of doing this than making a copy of the lines data structure
void draw_heightmap(SDL_Renderer *renderer, std::vector<Line3f> lines) {
	// We render with a color of choice at a time			
	SDL_SetRenderDrawColor(renderer, 0x5F, 0x00, 0x00, 0xFF); // red background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xAF);
	
	to_pixel_coordinates(lines);
	
	for (const Line3f &line : lines)
	{
		// assert(line.col(0).x() > 0);
		// assert(line.col(0).x() < SCREEN_WIDTH + 1);
		// SDL_Color render_color;

		// SDL_SetRenderDrawColor(renderer, render_color.r, render_color.g, render_color.b, render_color.a);

		SDL_RenderDrawLine(renderer, line.col(0).x(), SCREEN_HEIGHT - line.col(0).y(), line.col(1).x(), SCREEN_HEIGHT - line.col(1).y());
	}
	// Present the render, otherwise what has been drawn will not be seen
	SDL_RenderPresent(renderer);
}

void game_loop(SDL_Renderer* renderer, int** heightmap, int width, int height) {
	bool quit{ false };
	SDL_Event e;

	// pre-processing (things that will not be updated between rendering frames)
	std::vector<Line3f> lines;
	lines_from_heightmap(heightmap, width, height, lines);
	// draw initial view
	draw_heightmap(renderer, lines);

	// Handle events on queue
	while (!quit) {
		while (SDL_PollEvent(&e) != 0)
		{
			// User requests quit
			if (e.type == SDL_QUIT)
			{
				return;
			}
			// User presses a key
			else if (e.type == SDL_KEYDOWN) {
				// Move camera based on key press,
				// this assumes we are orbiting around the center
				Eigen::Vector3f up_down_axis = camera_position.cross(Eigen::Vector3f::UnitZ());
				up_down_axis.normalize();
				switch (e.key.keysym.sym)
				{
				case SDLK_UP:
					camera_position = Eigen::AngleAxisf(0.1 * M_PI, up_down_axis) * camera_position;
					break;

				case SDLK_DOWN:
					camera_position = Eigen::AngleAxisf(-0.1 * M_PI, up_down_axis) * camera_position;
					break;

				case SDLK_LEFT:
					camera_position = Eigen::AngleAxisf(0.1 * M_PI, Eigen::Vector3f::UnitZ()) * camera_position;
					break;

				case SDLK_RIGHT:
					camera_position = Eigen::AngleAxisf(-0.1 * M_PI, Eigen::Vector3f::UnitZ()) * camera_position;
					break;

				case SDLK_RIGHTBRACKET:
					perspective_factor += 0.1;
					break;

				case SDLK_LEFTBRACKET:
					perspective_factor -= 0.1;
					break;

				default:
					// error if a diff key is pressed to check behaviour
					assert(false);
					break;
				}
				// only redraw if view changed
				draw_heightmap(renderer, lines);
			}
		}
	}
}

// SDL requires specifically this signature for main
int main(int argc, char* args[])
{
	std::vector<unsigned char> image; //the raw pixels
	unsigned width, height;

	//decode
	unsigned error = lodepng::decode(image, width, height, "heightmap_64.png");

	//if there's an error, display it
	if (error) {
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
		return -1;
	}

	// Create a 2D array to store pixel values
	int** pixelValues = new int* [height];
	for (int i = 0; i < height; ++i) {
		pixelValues[i] = new int[width];
	}

	// Copy pixel values to the array
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			// 4 bytes per pixel (RGBA), we poll the R byte
			pixelValues[i][j] = static_cast<int>(image[(i * height + j) * 4]); 
		}
	}

	// The window we'll be rendering to
	SDL_Window* window = NULL;

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
	}
	else
	{
		// Create window
		window = SDL_CreateWindow("Heightmap Visualizer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (window == NULL)
		{
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		}
		else
		{
			SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); // TODO error handle this maybe
			game_loop(renderer, pixelValues, width, height);
		}
	}

	// Destroy window
	SDL_DestroyWindow(window);

	// Quit SDL subsystems
	SDL_Quit();

	// Don't forget to free memory
	for (int i = 0; i < height; ++i) {
		delete[] pixelValues[i];
	}
	delete[] pixelValues;

	return 0;
}
