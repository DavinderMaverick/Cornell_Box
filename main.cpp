#include <iostream>
#include <fstream>
#include "ray.h"
#include "sphere.h"
#include "hitablelist.h"
#include "camera.h"
#include "material.h"
#include <stdlib.h>
#include <limits>
#include "bvh.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include "rect.h"
#include "box.h"
#include "triangle.h"
#include "rotate.h"

#include <SFML/Graphics.hpp>

#define MAXFLOAT 4000
#define drand48() (rand() / (RAND_MAX + 1.0))
using namespace std;

typedef unsigned int uint;

const uint WIDTH = 1024;
const uint HEIGHT = 512;

const uint N = 32; //Tile Size N X N

const uint N_SAMPLES = 64;

const uint N_TILES_Y = HEIGHT / N;
const uint N_TILES_X = WIDTH / N;

const float SQRT_3 = sqrt(3);
const float SQRT_3_INV = 1.0f / sqrt(3);

hitable *world;

vec3 lookfrom(278, 278, -800);
vec3 lookat(278, 278, 0);
float dist_to_focus = 10.0;
float aperture = 0.0;
float vfov = 40.0;

camera cam(lookfrom, lookat, vec3(0, 1, 0), vfov, float(WIDTH) / float(HEIGHT), aperture, dist_to_focus);

atomic<unsigned> done_count;

//Prototypes
vec3 color(const ray& r, hitable *world, int depth);
triangle* getEquilateralTriangle(vec3 centroid, float length, material *mat);

struct ImageData
{
public:
	ImageData(uint w, uint h, uint ns) : _width(w), _height(h), _ns(ns)
	{
		data = new float[_width * _height * 3]; //RGB
		pixels = new sf::Uint8[_width * _height * 4]; //RGBA
		memset(data, 0, sizeof(float) * _width * _height * 3);
	}

	sf::Uint8 *get_pixels()
	{
		//convert values so we can display them
		for (int y = 0; y < _height; y++)
		{
			for (int x = 0; x < _width; x++)
			{
				uint data_pos = (y * _width + x) * 3;
				uint pix_pos = ((_height - y - 1) * _width + x) << 2; // *4 = 2^2 
				pixels[pix_pos + 0] = sf::Uint8(255.99f * (sqrtf(data[data_pos + 0] / _ns)));
				pixels[pix_pos + 1] = sf::Uint8(255.99f * (sqrtf(data[data_pos + 1] / _ns)));
				pixels[pix_pos + 2] = sf::Uint8(255.99f * (sqrtf(data[data_pos + 2] / _ns)));
				pixels[pix_pos + 3] = 255u;
			}
		}
		return pixels;
	}

	void saveAsPPM(string fileName)
	{
		ofstream fout;
		fout.open(fileName.c_str(), ios::trunc);

		fout << "P3\n" << _width << " " << _height << "\n255\n";

		for (int i = _height - 1; i >= 0; i--)
		{
			for (int j = 0; j < _width; j++)
			{
				uint data_pos = (i * _width + j) * 3;
				vec3 pixColor = vec3(data[data_pos + 0], data[data_pos + 1], data[data_pos + 2]);

				pixColor /= float(_ns);
				pixColor = vec3(sqrt(pixColor[0]), sqrt(pixColor[1]), sqrt(pixColor[2])); //gamma 2 correction

				int ir = int(255.99 * pixColor[0]);
				int ig = int(255.99 * pixColor[1]);
				int ib = int(255.99 * pixColor[2]);
				fout << ir << " " << ig << " " << ib << endl;
			}
		}
		fout.close();
	}

	inline void setPixel(uint x, uint y, const vec3 &pixColor)
	{
		uint data_pos = (y * _width + x) * 3;
		data[data_pos + 0] = pixColor.r();
		data[data_pos + 1] = pixColor.g();
		data[data_pos + 2] = pixColor.b();
	}

	~ImageData()
	{
		delete[] data;
		delete[] pixels;
	}

private:
	uint _width;
	uint _height;
	uint _ns;
	float* data;
	sf::Uint8 *pixels;// RGBA
};

ImageData renderImage(WIDTH, HEIGHT, N_SAMPLES);

struct Task
{
public:
	Task() : _id(++num) { cout << "Thread " << _id << " created!" << endl; }

	void nextTile(int &rx, int &ry)
	{
		static int x = -1, y = -1;
		if ((x + 1) % N_TILES_X == 0)
		{
			x = 0;
			y++;
		}
		else
		{
			x++;
		}
		rx = x;
		ry = y;
	}

	void move_in_pattern(int &rx, int &ry)
	{
		// snake pattern implementation
		static int x = -1, y = N_TILES_Y - 1;
		static int dir = 0;

		x = dir ? x - 1 : x + 1;
		if (x == N_TILES_X || x == -1) {
			x = y & 1 ? N_TILES_X - 1 : 0;
			y--;
			dir = !dir;
		}
		rx = x;
		ry = y;
	}

	bool getNextTask()
	{
		static bool taken[N_TILES_Y][N_TILES_X] = {};

		static mutex m;
		lock_guard<mutex> guard(m);

		bool taskFound = false;
		int x, y;
		while (!taskFound)
		{
			//nextTile(x, y);
			nextTile(x, y);
			if (x < 0 || x >= N_TILES_X || y < 0 || y >= N_TILES_Y)
				break;

			if (!taken[y][x])
			{
				sx = x * N;
				sy = y * N;
				taken[y][x] = true;
				taskFound = true;
			}
		}
		return taskFound;
	}

	void run()
	{
		bool taskDone = false;
		do
		{
			if (!getNextTask())
			{
				taskDone = true;
				continue;
			}

			for (uint y = sy; y < sy + N; y++)
			{
				for (uint x = sx; x < sx + N; x++)
				{
					if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT)
						continue;
					vec3 pixColor(0.0f, 0.0f, 0.0f);
					for (uint s = 0; s < N_SAMPLES; s++)
					{
						float u = float(x + drand48()) / float(WIDTH);
						float v = float(y + drand48()) / float(HEIGHT);
						ray r = cam.get_ray(u, v);
						pixColor += color(r, world, 0);
					}
					renderImage.setPixel(x, y, pixColor);
				}
			}
		} while (!taskDone);

		done_count++;

		cout << "Thread " << _id << " is done!" << endl;
	}

private:
	int sx = 0, sy = 0;
	int _id;
	static int num;
};

int Task::num = 0;

vec3 color(const ray& r, hitable *world, int depth)
{
	hit_record rec;
	float light_bouncing_rate = 0.1f;
	if (world->hit(r, 0.001, MAXFLOAT, rec))
	{
		ray scattered;
		vec3 attenuation;
		vec3 emitted = rec.mat_ptr->emitted();
		if (depth < 50 && rec.mat_ptr->scatter(r, rec, attenuation, scattered))
		{
			return emitted + attenuation * color(scattered, world, depth + 1);
		}
		else
		{
			return emitted;
		}
	}
	else
	{
		return vec3(0.0, 0.0, 0.0);//Background
	}
}

hitable *random_scene()
{
	int n = 500;
	hitable **list = new hitable*[n + 1];

	list[0] = new sphere(vec3(0, -1000, 0), 1000, new lambertian(vec3(0.5, 0.5, 0.5)));
	int i = 1;
	for (int a = -11; a < 11; a++)
	{
		for (int b = -11; b < 11; b++)
		{
			float choose_mat = drand48();
			vec3 center(a + 0.9 * drand48(), 0.2, b + 0.9 * drand48());
			if ((center - vec3(4, 0.2, 0)).length() > 0.9)
			{
				if (choose_mat < 0.8)
				{
					//diffuse
					list[i++] = new sphere(center, 0.2, new lambertian(vec3(drand48() * drand48(), drand48() * drand48(), drand48() * drand48())));
				}
				else if (choose_mat < 0.95)
				{
					//metal
					list[i++] = new sphere(center, 0.2, new metal(vec3(0.5 * (1 + drand48()), 0.5 * (1 + drand48()), 0.5 * (1 + drand48())), 0.5 * drand48()));
				}
				else
				{
					//glass
					list[i++] = new sphere(center, 0.2, new dielectric(1.5));
				}
			}
		}
	}

	list[i++] = new sphere(vec3(0, 1, 0), 1.0, new dielectric(1.5));
	list[i++] = new sphere(vec3(-4, 1, 0), 1.0, new lambertian(vec3(0.4, 0.2, 0.1)));
	list[i++] = new sphere(vec3(4, 1, 0), 1.0, new metal(vec3(0.7, 0.6, 0.5), 0.0));

	//return new hitable_list(list, i);
	return new bvh_node(list, i);
}

hitable *cornell_box()
{
	hitable **list = new hitable*[8];
	int i = 0;
	material *red = new lambertian(vec3(0.65, 0.05, 0.05));
	material *white = new lambertian(vec3(0.73, 0.73, 0.73));
	material *green = new lambertian(vec3(0.12, 0.45, 0.15));
	material *light = new diffuse_light(vec3(15, 15, 15));
	list[i++] = new flip_normals(new yz_rect(0, 555, 0, 555, 555, green));
	list[i++] = new yz_rect(0, 555, 0, 555, 0, red);
	list[i++] = new xz_rect(213, 343, 227, 332, 554, light);
	list[i++] = new flip_normals(new xz_rect(0, 555, 0, 555, 555, white));
	list[i++] = new xz_rect(0, 555, 0, 555, 0, white);
	list[i++] = new flip_normals(new xy_rect(0, 555, 0, 555, 555, white));
	list[i++] = new translate(new rotate_y(new box(vec3(0, 0, 0), vec3(165, 165, 165), white), -18), vec3(130, 0, 65));
	list[i++] = new translate(new rotate_y(new box(vec3(0, 0, 0), vec3(165, 330, 165), white), 15), vec3(265, 0, 295));
	//return new hitable_list(list, i);
	return new bvh_node(list, i);
}

hitable *cornell_box_triangle()
{
	hitable **list = new hitable*[9];
	int i = 0;
	material *red = new lambertian(vec3(0.65, 0.05, 0.05));
	material *white = new lambertian(vec3(0.73, 0.73, 0.73));
	material *green = new lambertian(vec3(0.12, 0.45, 0.15));
	material *blue = new lambertian(vec3(0.12, 0.30, 0.90));
	material *light = new diffuse_light(vec3(15, 15, 15));
	list[i++] = new flip_normals(new yz_rect(0, 555, 0, 555, 555, green));
	list[i++] = new yz_rect(0, 555, 0, 555, 0, red);
	list[i++] = new xz_rect(213, 343, 227, 332, 554, light);
	list[i++] = new flip_normals(new xz_rect(0, 555, 0, 555, 555, white));
	list[i++] = new xz_rect(0, 555, 0, 555, 0, white);
	list[i++] = new flip_normals(new xy_rect(0, 555, 0, 555, 555, white));

	list[i++] = new translate(new rotate_y(new box(vec3(0, 0, 0), vec3(165, 165, 165), white), -18), vec3(130, 0, 65));
	list[i++] = new translate(new rotate_y(new box(vec3(0, 0, 0), vec3(165, 330, 165), white), 15), vec3(265, 0, 295));

	list[i++] = getEquilateralTriangle(vec3(278, 368, 100), 120, blue);

	return new bvh_node(list, i);
}

int main()
{
	sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Ray Tracing", sf::Style::Titlebar | sf::Style::Close);
	sf::Texture tex;
	sf::Sprite sprite;

	if (!tex.create(WIDTH, HEIGHT))
	{
		cerr << "Couldn't create texture!" << endl;
		return 1;
	}

	tex.setSmooth(false);
	sprite.setTexture(tex);

	world = cornell_box_triangle();

	const uint n_threads = thread::hardware_concurrency() - 1;
	cout << "Detected " << n_threads + 1 << " concurrent threads." << endl;
	cout << "Launching " << 1 << " main thread + " << n_threads << " worker threads" << endl;
	vector<thread> threads(n_threads);
	chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();

	vector<Task> tasks(n_threads);

	int i = 0;
	for (auto &t : threads)
	{
		t = thread(&Task::run, &tasks[i]);
		i++;
	}

	bool finished_rendering = false;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (finished_rendering && event.type == sf::Event::Closed)
				window.close();
		}

		if (!finished_rendering)
		{

			tex.update(renderImage.get_pixels());
			window.clear();
			window.draw(sprite);
			window.display();
		}


		if (!finished_rendering && done_count == n_threads)
		{
			chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
			auto duration = chrono::duration_cast<chrono::seconds>(end - start).count();
			cout << "Finished rendering in " << duration << "s" << endl;
			finished_rendering = true;
		}

		if (!finished_rendering)
			this_thread::sleep_for(chrono::milliseconds(1000));
	}

	cout << "Waiting for all the threads to join." << endl;
	for (auto &t : threads)
		t.join();
	cout << "All Threads Joined" << endl;

	cout << "Saving Image" << endl;
	renderImage.saveAsPPM("output.ppm");
	tex.copyToImage().saveToFile("output.png");
	cout << "Image Saved" << endl;
	return 0;
}

triangle* getEquilateralTriangle(vec3 centroid, float length, material *mat)
{
	float length_div_2 = length / 2;
	vec3 v0, v1, v2;
	v0[2] = v1[2] = v2[2] = centroid[2];
	
	v0[0] = centroid[0] - length_div_2;
	v1[0] = centroid[0] + length_div_2;
	v2[0] = centroid[0];

	float y_bottom = centroid[1] - length * 0.5f * SQRT_3_INV;
	float y_top = y_bottom + SQRT_3 * 0.5f * length;
	
	v0[1] = v1[1] = y_bottom;
	v2[1] = y_top;

	return new triangle(v0, v1, v2, mat);
}