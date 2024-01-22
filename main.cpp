#include <SDL.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include "lodepng.h"
#include "Eigen/Core"

// Screen dimension constants
const int SCREEN_WIDTH = 950;
const int SCREEN_HEIGHT = 600;

int HEIGHTMAP_ROWS = 4;
int HEIGHTMAP_COLS = 4;

typedef struct s_point {
	float x, y, z;
} t_point;

typedef std::pair<t_point, t_point> t_line;

// TODO simplify the functions below by writing your own vector math library
void basis_transform(t_point &point, t_point new_x, t_point new_y, t_point new_z) {
	auto temp = point;
	point.x = temp.x * new_x.x + temp.y * new_y.x + temp.z * new_z.x;
	point.y = temp.x * new_x.y + temp.y * new_y.y + temp.z * new_z.y;
	point.z = temp.x * new_x.z + temp.z * new_y.z + temp.z * new_z.z;
}

void basis_transform(t_line &line, t_point new_x, t_point new_y, t_point new_z) {
	basis_transform(line.first, new_x, new_y, new_z);
	basis_transform(line.second, new_x, new_y, new_z);
}

void basis_transform(std::vector<t_line> &lines, t_point new_x, t_point new_y, t_point new_z) {
	for (t_line &line : lines) {
		basis_transform(line, new_x, new_y, new_z);
	}
}

void point_scale(t_point &point, float factor) {
	point.x *= factor;
	point.y *= factor;
	point.z *= factor;
}

void lines_scale(std::vector<t_line> &lines, float factor) {
	for (t_line &line : lines)
	{
		point_scale(line.first, factor);
		point_scale(line.second, factor);
	}
	fprintf(stdout, "exiting lines_scale\n");
}

void lines_from_heightmap(int** heightmap, int width, int height, std::vector<t_line> &lines) {
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			float z_fact = -10;
			float z_trans = 16.1f;
			t_point current = { i, j, z_fact * heightmap[i][j] / 255.0f + z_trans };
			if (i > 0)
			{
				t_point up = { i - 1, j, z_fact * heightmap[i - 1][j] / 255.0f + z_trans };
				lines.push_back(t_line(current, up));
			}
			if (j > 0)
			{
				t_point left = { i, j - 1, z_fact * heightmap[i][j - 1] / 255.0f + z_trans };
				lines.push_back(t_line(current, left));
			}

		}
	}
}

void draw_heightmap(SDL_Renderer *renderer, int **heightmap, int width, int height) {
	// We render with a color of choice at a time		
	
	SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF); // red background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_ADD);

	SDL_SetRenderDrawColor(renderer, 0x0F, 0xFF, 0xFF, 0x4F);
	
	std::vector<t_line> lines;
	lines_from_heightmap(heightmap, width, height, lines);
	
	// transform lines so we can visualize them
	lines_scale(lines, 10);
	basis_transform(lines, { 1.0f, 0, 0 }, { 0.5f, 0.5f, 0 }, { 0, 1.0f, 0 });
	
	for (const t_line &line : lines)
	{
		SDL_RenderDrawLine(renderer, line.first.x, line.first.y, line.second.x, line.second.y);
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
