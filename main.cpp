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

typedef struct s_vertex3d {
	Eigen::Vector3f position;
	Eigen::Vector3f normal;
	SDL_Color color;
} t_vertex3d;

Eigen::Vector3f camera_position(1.0f, 1.0f, 1.0f);

float perspective_factor = camera_position.norm();

// sunlight simulation
Eigen::Vector3f light_direction(0.0f, 0.0f, -1.0f); // pointing straight down

void compute_color(t_vertex3d& v) {
	int val = abs(v.normal.dot(light_direction)) * 255;
	
	SDL_Color color{ val , val , val , 0xFF };
	v.color = color;
}

void set_heightmap_normal(int** heightmap, int i, int j, int width, int height, Eigen::Vector3f &normal) {
	int num_normals = 0;
	normal.x() = normal.y() = normal.z() = 0;
	// normal of line above
	float delta_vertical { 0.0f };
	float delta_horizontal { 0.0f };
	float current = heightmap[i][j] / 255.0f;
	
	if (i - 1 >= 0) // up
		delta_vertical += current - heightmap[i - 1][j] / 255.0f;
	if (i + 1 < height) // down
		delta_vertical += heightmap[i + 1][j] / 255.0f - current;
	if (j - 1 >= 0) // left
		delta_horizontal += heightmap[i][j - 1] / 255.0f - current;
	if (i + 1 < width) // right
		delta_horizontal += current - heightmap[i][j + 1] / 255.0f;

	// create tangent space vectors so we can compute normal of vertex
	// note that each vertex corresponds to one pixel in the given heightmap
	Eigen::Vector3f tangent_vertical(2.0f / height, 0.0f, delta_vertical);
	Eigen::Vector3f tangent_horizontal(0.0f, 2.0f / width, delta_horizontal);
	normal = tangent_vertical.cross(tangent_horizontal);
	normal.normalize();
}

void initialize_vertex(int i, int j, int** heightmap, int width, int height, t_vertex3d& vertex) {
	float z_fact = 0.2f; // TODO paramatrize this
	Eigen::Vector3f pos(i, j, z_fact * heightmap[i][j] / 255.0f);
	
	vertex.position << pos;
	vertex.position.x() = vertex.position.x() / height - 0.5f; // the substraction centers the heightmap plane on the x and y -axes
	vertex.position.y() = vertex.position.y() / width - 0.5f;
	
	set_heightmap_normal(heightmap, i, j, width, height, vertex.normal);
	compute_color(vertex);
}

// creates a strip of triangles
void tris_from_heightmap(int** heightmap, int width, int height, std::vector<t_vertex3d>& triangle_points) {
	for (int i = 1; i < height; i++)
	{
		for (int j = 1; j < width; j++)
		{			
			t_vertex3d v[4]; // four points corresponding to the quad of the heightmap we are currently processing

			initialize_vertex(i - 1, j - 1, heightmap, width, height, v[0]);
			initialize_vertex(i - 1, j, heightmap, width, height, v[1]);
			initialize_vertex(i, j - 1, heightmap, width, height, v[2]);
			initialize_vertex(i, j, heightmap, width, height, v[3]);

			// triangle 1
			triangle_points.push_back(v[0]);
			triangle_points.push_back(v[1]);
			triangle_points.push_back(v[2]);

			// triangle 2
			triangle_points.push_back(v[1]);
			triangle_points.push_back(v[2]);
			triangle_points.push_back(v[3]);
		}
	}
}

void to_pixel_coordinates_verticies(std::vector<t_vertex3d>& verticies) {
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

	// TODO this can be just one matrix multiplication, right?
	Eigen::Vector3f translation;
	translation.x() = camera_position.dot(camera_right);
	translation.y() = camera_position.dot(camera_up);
	translation.z() = camera_position.dot(camera_look_at);

	// TODO: if we do not modify the vertices but instead pass the matrix on, we could get rid of this expensive copy
	for (t_vertex3d& v : verticies) {
		v.position -= camera_position;
		v.position = view_matrix * v.position;

		// perspective division
		v.position.x() *= perspective_factor / abs(v.position.z());
		v.position.y() *= perspective_factor / abs(v.position.z());
		// !!! NOTE that we do not divide the z-coordinate since we need to perserve it for visible surface detection

		float zoom_factor = 500.0f;
		v.position *= zoom_factor;

		// transform to pixel coordinates
		v.position.x() += SCREEN_WIDTH / 2.0f;
		v.position.y() += SCREEN_HEIGHT / 2.0f;
	}
}

void to_pixel_coordinates_lines(std::vector<Line3f> &lines) {	
	Eigen::Vector3f focus_point(0.0f, 0.0f, 0.0f);
	Eigen::Vector3f camera_look_at = camera_position - focus_point;
	camera_look_at.normalize();
	
	Eigen::Vector3f camera_up(0.0f, 1.0f, 0.0f);
	
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
			
			Eigen::Vector3f current(i, j, z_fact * heightmap[i][j] / 255.0f);

			// TODO D.R.Y. or copy setup from tris
			if (i > 0)
			{
				Eigen::Vector3f up(i - 1.0f, j, z_fact* heightmap[i - 1][j] / 255.0f);
				Line3f temp;
				temp << current, up;
				temp.row(0) /= height;
				temp.row(1) /= width;
				lines.push_back(temp);
			}
			if (j > 0)
			{
				Eigen::Vector3f left(i, j - 1.0f, z_fact* heightmap[i][j - 1] / 255.0f);
				Line3f temp;
				temp << current, left;
				temp.row(0) /= height;
				temp.row(1) /= width;
				lines.push_back(temp);
			}

		}
	}
}

// this data structure is used to keep track of indices of SDL_triangles we want to render
typedef struct s_triangle {
	int i; // index of first point, by convertion i+1 and i+2 are the indices of the next points
	float depth;
}t_triangle;

// ascending sort for painter's algorithm
bool t_triangle_sorter(t_triangle& lhs, t_triangle& rhs) {
	return lhs.depth > rhs.depth;
}

void depth_order(const std::vector<t_vertex3d>& verticies, std::vector<int> &index_list) {
	std::vector<t_triangle> triangles;
	for (int i = 0; i < verticies.size(); i += 3) {
		t_triangle temp;
		temp.i = i;
		temp.depth = abs(verticies[i].position.z() + verticies[i + 1].position.z() + verticies[i + 2].position.z());
		triangles.push_back(temp);
	}
	std::sort(triangles.begin(), triangles.end(), &t_triangle_sorter);
	for (int i = 0; i < verticies.size() / 3; i ++) {
		index_list.push_back(triangles[i].i);
		index_list.push_back(triangles[i].i + 1);
		index_list.push_back(triangles[i].i + 2);
	}
}

// TODO this still makes a copy per call
void draw_heightmap(SDL_Renderer* renderer, std::vector<t_vertex3d> verticies) {
	// We render with a color of choice at a time			
	SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF); // white background
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_BLEND);

	to_pixel_coordinates_verticies(verticies);
	
	std::vector<int> index_list;
	depth_order(verticies, index_list); // index_list provides the order in which the triangles should be rendered

	std::vector<SDL_Vertex> sdl_verticies;

	for (t_vertex3d& v : verticies)
	{
		// SDL vertices are 2D, so that is why I created my own data struct
		SDL_Vertex temp;
		
		temp.position.x = v.position.x();
		temp.position.y = SCREEN_HEIGHT - v.position.y();
		
		// recompute shading 
		compute_color(v);
		temp.color = v.color; 

		sdl_verticies.push_back(temp);
	}

	SDL_RenderGeometry(renderer, NULL, &(sdl_verticies[0]), verticies.size(), &(index_list[0]), index_list.size());
	
	SDL_RenderPresent(renderer); // Present the render, otherwise what has been drawn will not be seen
}

// TODO find a more efficient way of doing this than making a copy of the lines data structure
// TODO: just make a new transform matrix every time?
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
					camera_position = Eigen::AngleAxisf(0.01 * M_PI, up_down_axis) * camera_position;
					break;

				case SDLK_DOWN:
					camera_position = Eigen::AngleAxisf(-0.01 * M_PI, up_down_axis) * camera_position;
					break;

				case SDLK_LEFT:
					camera_position = Eigen::AngleAxisf(0.01 * M_PI, Eigen::Vector3f::UnitZ()) * camera_position;
					break;

				case SDLK_RIGHT:
					camera_position = Eigen::AngleAxisf(-0.01 * M_PI, Eigen::Vector3f::UnitZ()) * camera_position;
					break;

				case SDLK_RIGHTBRACKET:
					camera_position *= 1.1;
					perspective_factor = camera_position.norm();
					break;

				case SDLK_LEFTBRACKET:
					camera_position *= 0.9;
					perspective_factor = camera_position.norm();
					break;

				// light source direction change
				case SDLK_1:
					light_direction = Eigen::AngleAxisf(0.05 * M_PI, Eigen::Vector3f::UnitX()) * light_direction;
					break;

				case SDLK_2:
					light_direction = Eigen::AngleAxisf(-0.05 * M_PI, Eigen::Vector3f::UnitX()) * light_direction;
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
	unsigned error = lodepng::decode(image, width, height, "./test_data/heightmap_128.png");

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
