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

void to_pixel_coordinates(std::vector<Line3f> &lines) {
	float scale_factor = 10.0f;
	for (Line3f& line : lines) {
		// TODO move this: center the plane on 0
		line.col(0).x() -= 0.5f;
		line.col(1).x() -= 0.5f;
		line.col(0).y() -= 0.5f;
		line.col(1).y() -= 0.5f;

		line *= scale_factor;
	}
	
	Eigen::Vector3f camera_position(10.0f, 0.0f, 1.0f);
	Eigen::Vector3f camera_look_at(-1.0f, -1.0f, 1.0f);
	camera_look_at.normalize();
	
	Eigen::Vector3f camera_up(0.0f, 0.0f, 1.0f);
	
	Eigen::Vector3f camera_right = camera_up.cross(camera_look_at);
	camera_right.normalize();
	
	camera_up = camera_look_at.cross(camera_right);
	camera_up.normalize();

	Eigen::Matrix3f view_matrix;
	view_matrix.col(0) << camera_right;
	view_matrix.col(1) << camera_up;
	view_matrix.col(2) << camera_look_at;

	view_matrix.transposeInPlace();

	Eigen::Vector3f translation;
	translation.x() = camera_position.dot(camera_right);
	translation.y() = camera_position.dot(camera_up);
	translation.z() = camera_position.dot(camera_look_at);
	
	for (Line3f& line : lines) {
		line.col(0) -= camera_position;
		line.col(1) -= camera_position;
		line = view_matrix * line;
		
		float zoom_factor = 30.0f;
		line *= zoom_factor;

		// transform to pixel coordinates here
		line.col(0).y() += SCREEN_HEIGHT / 2.0f;
		line.col(1).y() += SCREEN_HEIGHT / 2.0f;

		line.col(0).x() += SCREEN_WIDTH / 2.0f;
		line.col(1).x() += SCREEN_WIDTH / 2.0f;
		
		// perspective division
		//if (line.col(0).z() < -1)
		//	line.col(0) /= abs(line.col(0).z() / 100.0f);
		//if (line.col(1).z() < -1)
		//	line.col(1) /= abs(line.col(1).z() / 100.0f);
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

void set_color_zbuf(Line3f line, SDL_Color& color) {
	float mini = -350.0f;
	float maxi = -300.0f;
	int blue = (std::max(mini, line.col(0).z()) / -400.0f) * 255.0f;
	int red = (std::min(mini, line.col(0).z()) / mini) * 255.0f;
	color.a = 0xFF;
	color.r = blue;
	color.g = 0x00;
	color.b = blue;
	
}

void draw_heightmap(SDL_Renderer *renderer, int **heightmap, int width, int height) {
	// We render with a color of choice at a time		
	
	SDL_SetRenderDrawColor(renderer, 0x5F, 0x00, 0x00, 0xFF); // red background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_ADD);

	SDL_SetRenderDrawColor(renderer, 0x0F, 0xFF, 0xFF, 0xAF);
	
	std::vector<Line3f> lines;
	lines_from_heightmap(heightmap, width, height, lines);
	
	to_pixel_coordinates(lines);
	
	for (const Line3f &line : lines)
	{
		assert(line.col(0).x() > 0);
		SDL_Color render_color;
		set_color_zbuf(line, render_color);
		SDL_SetRenderDrawColor(renderer, render_color.r, render_color.g, render_color.b, render_color.a);

		SDL_RenderDrawLine(renderer, line.col(0).x(), SCREEN_HEIGHT - line.col(0).y(), line.col(1).x(), SCREEN_HEIGHT - line.col(1).y());
	}
	// Present the render, otherwise what has been drawn will not be seen
	SDL_RenderPresent(renderer);
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

	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...

	// Get image dimensions

	// Create a 2D array to store pixel values
	int** pixelValues = new int* [height];
	for (int i = 0; i < height; ++i) {
		pixelValues[i] = new int[width];
	}

	// Copy pixel values to the array
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			pixelValues[i][j] = static_cast<int>(image[(i * height + j) * 4]);
		}
	}
	// Now Use the pixelValues array as needed

	// The window we'll be rendering to
	SDL_Window* window = NULL;

	// The surface contained by the window
	SDL_Surface* screenSurface = NULL;

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
			SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
			
			draw_heightmap(renderer, pixelValues, width, height);

			// Hack to get window to stay up
			SDL_Event e; bool quit = false; while (quit == false) { while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; } }
		}
	}

	// Destroy window
	SDL_DestroyWindow(window);

	// Quit SDL subsystems
	SDL_Quit();


	// Don't forget to free the allocated memory
	for (int i = 0; i < height; ++i) {
		delete[] pixelValues[i];
	}
	delete[] pixelValues;

	return 0;
}
