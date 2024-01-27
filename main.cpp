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

typedef struct s_triangle {
	Eigen::Vector3f position;
	SDL_Color color;
} t_vertex3d;

Eigen::Vector3f camera_position(1.0f, 1.0f, 1.0f);

float perspective_factor = camera_position.norm();

// sunlight simulation
Eigen::Vector3f light_direction(0.0f, 0.0f, -1.0f); // pointing straight down

void compute_color(t_vertex3d& v1, t_vertex3d& v2, t_vertex3d& v3) {
	Eigen::Vector3f side1 = v1.position - v2.position;
	Eigen::Vector3f side2 = v1.position - v3.position;
	side1.normalize();
	side2.normalize();
	Eigen::Vector3f normal = side1.cross(side2);
	normal.normalize();
	int val = abs(normal.dot(light_direction)) * 255;
	SDL_Color color{ val, val, val, val };
	v1.color = v2.color = v3.color = color;
}

// list of triangles (vertices are rendered in sequential order)
void tris_from_heightmap(int** heightmap, int width, int height, std::vector<t_vertex3d>& triangle_points) {
	for (int i = 1; i < height; i++)
	{
		for (int j = 1; j < width; j++)
		{
			float z_fact = 0.2f;
			float z_trans = 0.0f;

			Eigen::Vector3f pos1(i - 1.0f, j - 1.0f, z_fact * heightmap[i - 1][j - 1] / 255.0f + z_trans);
			Eigen::Vector3f pos2(i - 1.0f, j, z_fact * heightmap[i - 1][j] / 255.0f + z_trans);
			Eigen::Vector3f pos3(i, j - 1.0f, z_fact * heightmap[i][j - 1] / 255.0f + z_trans);
			Eigen::Vector3f pos4(i, j, z_fact * heightmap[i][j] / 255.0f + z_trans);
			
			t_vertex3d v1;
			t_vertex3d v2;
			t_vertex3d v3;
			t_vertex3d v4;
			
			// TODO D.R.Y.
			v1.position << pos1;
			v1.position.x() /= height;
			v1.position.y() /= width;
			
			v2.position << pos2;
			v2.position.x() /= height;
			v2.position.y() /= width;

			v3.position << pos3;
			v3.position.x() /= height;
			v3.position.y() /= width;

			v4.position << pos4;
			v4.position.x() /= height;
			v4.position.y() /= width;
			
			// triangle 1
			compute_color(v1, v2, v3);
			triangle_points.push_back(v1);
			triangle_points.push_back(v2);
			triangle_points.push_back(v3);

			// triangle 2
			compute_color(v2, v3, v4);
			triangle_points.push_back(v2);
			triangle_points.push_back(v3);
			triangle_points.push_back(v4);
		}
	}
}

void to_pixel_coordinates_verticies(std::vector<t_vertex3d>& verticies) {
	for (t_vertex3d& v : verticies) {
		// TODO move this
		// center the plane on 0
		v.position.x() -= 0.5f;
		v.position.y() -= 0.5f;
	}

	Eigen::Vector3f focus_point(0.0f, 0.0f, 0.0f);
	Eigen::Vector3f camera_look_at = camera_position - focus_point;
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

	for (t_vertex3d& v : verticies) {
		v.position -= camera_position;
		v.position = view_matrix * v.position;

		// perspective division
		if (v.position.z() < -1)
			v.position *= perspective_factor / abs(v.position.z());

		float zoom_factor = 500.0f;
		v.position *= zoom_factor;

		// transform to pixel coordinates
		v.position.x() += SCREEN_WIDTH / 2.0f;
		v.position.y() += SCREEN_HEIGHT / 2.0f;
	}
}

void to_pixel_coordinates_lines(std::vector<Line3f> &lines) {
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

// TODO this still makes a copy per call
void draw_heightmap(SDL_Renderer* renderer, std::vector<t_vertex3d> verticies) {
	// We render with a color of choice at a time			
	SDL_SetRenderDrawColor(renderer, 0x5F, 0x00, 0x00, 0xFF); // red background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xAF);

	to_pixel_coordinates_verticies(verticies);

	std::vector<SDL_Vertex> sdl_verticies;

	for (const t_vertex3d& v : verticies)
	{
		SDL_Vertex temp;
		SDL_Color color{ 0xFF, 0xFF, 0xFF, 0xAF };

		temp.position.x = v.position.x();
		temp.position.y = SCREEN_HEIGHT - v.position.y();
		temp.color = v.color; 
		// temp.color = color;

		sdl_verticies.push_back(temp);
		// SDL_RenderDrawLine(renderer, line.col(0).x(), SCREEN_HEIGHT - line.col(0).y(), line.col(1).x(), SCREEN_HEIGHT - line.col(1).y());
	}
	// C++ std::vertex is guaranteed to store elements contiguously
	// TODO: might be more efficient to just allocate elems contiguously
	SDL_RenderGeometry(renderer, NULL, &(sdl_verticies[0]), verticies.size(), NULL, 0);
	// Present the render, otherwise what has been drawn will not be seen
	SDL_RenderPresent(renderer);
}

// TODO find a more efficient way of doing this than making a copy of the lines data structure
void draw_heightmap(SDL_Renderer *renderer, std::vector<Line3f> lines) {
	// We render with a color of choice at a time			
	SDL_SetRenderDrawColor(renderer, 0x5F, 0x00, 0x00, 0xFF); // red background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xAF);
	
	to_pixel_coordinates_lines(lines);
	
	for (const Line3f &line : lines)
	{
		SDL_RenderDrawLine(renderer, line.col(0).x(), SCREEN_HEIGHT - line.col(0).y(), line.col(1).x(), SCREEN_HEIGHT - line.col(1).y());
	}
	// Present the render, otherwise what has been drawn will not be seen
	SDL_RenderPresent(renderer);
}

void game_loop(SDL_Renderer* renderer, int** heightmap, int width, int height) {
	bool quit{ false };
	SDL_Event e;

	// pre-processing (things that will not be updated between rendering frames)
	std::vector<t_vertex3d> verticies;
	tris_from_heightmap(heightmap, width, height, verticies);
	// draw initial view
	draw_heightmap(renderer, verticies);

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
				draw_heightmap(renderer, verticies);
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
