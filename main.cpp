#include <SDL.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include "lodepng.h"
#include "Eigen/Core"

// Screen dimension constants
const int SCREEN_WIDTH = 950;
const int SCREEN_HEIGHT = 600;

// We store lines as 3x2 matrices (two 3D points)
typedef Eigen::Matrix<float, 3, 2> Line3f;

void to_pixel_coordinates(std::vector<Line3f> &lines) {
	float factor = 10.0f;
	for (Line3f &line : lines)
		line *= factor;
	
	Eigen::Matrix3f transform_mat;
	transform_mat << 1.0f, 0.0f, 0.0f,
					0.5f, 0.5f, 0.0f,
					0.0f, 1.0f, 0.0f;
	transform_mat.transposeInPlace();
	
	for (Line3f &line : lines)
		line = transform_mat * line;
}

void lines_from_heightmap(int** heightmap, int width, int height, std::vector<Line3f> &lines) {
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			float z_fact = -10;
			float z_trans = 16.1f;
			
			Eigen::Vector3f current(i, j, z_fact * heightmap[i][j] / 255.0f + z_trans);

			if (i > 0)
			{
				Eigen::Vector3f up(i - 1.0f, j, z_fact* heightmap[i - 1][j] / 255.0f + z_trans);
				Line3f temp;
				temp << current, up;
				lines.push_back(temp);
			}
			if (j > 0)
			{
				Eigen::Vector3f left(i, j - 1.0f, z_fact* heightmap[i][j - 1] / 255.0f + z_trans);
				Line3f temp;
				temp << current, left;
				lines.push_back(temp);
			}

		}
	}
}

void draw_heightmap(SDL_Renderer *renderer, int **heightmap, int width, int height) {
	// We render with a color of choice at a time		
	
	SDL_SetRenderDrawColor(renderer, 0x5F, 0x00, 0x00, 0xFF); // red background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_ADD);

	SDL_SetRenderDrawColor(renderer, 0x0F, 0xFF, 0xFF, 0xAF);
	
	std::vector<Line3f> lines;
	lines_from_heightmap(heightmap, width, height, lines);
	
	// transform lines to pixel coordinates
	to_pixel_coordinates(lines);
	
	for (const Line3f &line : lines)
	{
		SDL_RenderDrawLine(renderer, line.col(0).x(), line.col(0).y(), line.col(1).x(), line.col(1).y());
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
