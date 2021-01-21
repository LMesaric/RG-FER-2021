// ReSharper disable CppInconsistentNaming
#include <cstdio>
#include <gl/freeglut.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <chrono>


GLuint width = 900, height = 900;

const auto spline_coeffs = (1.f / 6.f) * glm::mat4(
	-1, 3, -3, 1,
	3, -6, 0, 4,
	-3, 3, 3, 1,
	1, 0, 0, 0
);

struct Face3D
{
	int v1, v2, v3;
};

std::vector<glm::vec4> v;
std::vector<Face3D> f;

std::vector<glm::vec3> points_v;

glm::vec3 eye = { 0,0,0 };
glm::vec3 look_at = { 0,0,0 };
glm::vec3 view_up = { 0,1,0 };

glm::vec3 current_pos = { 0, 0, 0 };
glm::vec3 current_spline_z = { 0, 0, 1 };
glm::vec3 current_spline_y = { 0, 0, 1 };
glm::vec3 current_spline_x = { 0, 0, 1 };


void myDisplay();
void myReshape(int w, int h);
void myIdle();
long long getCurrentTimeMs();
void loadPoints(char* file);
void loadModel(char* file);
void precalculate();
void drawSpline();
void drawObject();
glm::vec3 bSplinePos(int segment, float t);
glm::vec3 bSplineDir(int segment, float t);
glm::vec3 bSplineSecond(int segment, float t);
glm::mat4 bSplineHelp(int segment);


int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("Need exactly two parameters!\n");
		exit(1);
	}

	loadModel(argv[1]);
	loadPoints(argv[2]);
	precalculate();

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(width, height);
	glutInitWindowPosition(400, 200);
	glutInit(&argc, argv);

	glutCreateWindow("B-Spline");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glutReshapeFunc(myReshape);
	glutDisplayFunc(myDisplay);

	glutIdleFunc(myIdle);

	glutMainLoop();
	return 0;
}

void myDisplay()
{
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);

	const auto view_mat = lookAt(eye, look_at, view_up);

	glColor3f(0.0f, 0.0f, 0.6f);
	glLoadMatrixf(value_ptr(view_mat));
	drawSpline();

	const auto translate_spline = translate(glm::mat4(1.0f), current_pos);
	const auto rotate_spline = glm::mat4(
		current_spline_x.x, current_spline_x.y, current_spline_x.z, 0,
		current_spline_y.x, current_spline_y.y, current_spline_y.z, 0,
		current_spline_z.x, current_spline_z.y, current_spline_z.z, 0,
		0, 0, 0, 1
	);

	glColor3f(0.7f, 0.f, 0.f);
	glLoadMatrixf(value_ptr(view_mat * translate_spline * rotate_spline));
	glScaled(2, 2, 2);
	drawObject();

	glutSwapBuffers();
}

void myReshape(const int w, const int h)
{
	width = w; height = h;
	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(value_ptr(glm::perspective(glm::radians(60.0f), float(w) / float(h), 0.01f, 10000.0f)));
}

void myIdle()
{
	static const auto total_animation_time = 10000.0;
	static const auto num_segments = double(points_v.size()) - 3;
	static const auto time_per_segment = total_animation_time / num_segments;

	static auto startTime = -1LL;
	if (startTime == -1LL)
		startTime = getCurrentTimeMs();

	const auto ms = static_cast<double>(getCurrentTimeMs()) - startTime;

	const auto seg = static_cast<int>(std::floor(ms / time_per_segment));
	if (seg >= num_segments) {
		startTime = getCurrentTimeMs();
		return;
	}
	const auto t = (ms / time_per_segment) - std::floor(ms / time_per_segment);

	current_pos = bSplinePos(seg, t);
	current_spline_z = bSplineDir(seg, t);
	current_spline_y = bSplineSecond(seg, t);
	current_spline_x = cross(current_spline_y, current_spline_z);

	glutPostRedisplay();
}

long long getCurrentTimeMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

void loadPoints(char* file)
{
	std::string line;
	std::ifstream infile(file);
	while (getline(infile, line))
	{
		if (line.length() < 1)
			continue;

		float v1, v2, v3;
		sscanf_s(line.c_str(), "%f %f %f", &v1, &v2, &v3);
		points_v.emplace_back(v1, v2, v3);
	}
	infile.close();
}

void loadModel(char* file)
{
	std::string line;
	std::ifstream infile(file);
	while (getline(infile, line))
	{
		if (line.length() < 1)
			continue;

		if (line.at(0) == 'v')
		{
			float v1, v2, v3;
			sscanf_s(line.c_str(), "%*s %f %f %f", &v1, &v2, &v3);
			v.emplace_back(v1, v2, v3, 1.0);
		}
		else if (line.at(0) == 'f')
		{
			int f1, f2, f3;
			sscanf_s(line.c_str(), "%*s %d %d %d", &f1, &f2, &f3);
			f.push_back(Face3D{ f1 - 1, f2 - 1, f3 - 1 });
		}
	}
	infile.close();
}

void precalculate()
{
	// Scale object to [-1,1] and center it

	auto x_min = v[0].x, x_max = x_min;
	auto y_min = v[0].y, y_max = y_min;
	auto z_min = v[0].z, z_max = z_min;

	for (auto vi : v)
	{
		x_min = std::min(x_min, vi.x);
		x_max = std::max(x_max, vi.x);
		y_min = std::min(y_min, vi.y);
		y_max = std::max(y_max, vi.y);
		z_min = std::min(z_min, vi.z);
		z_max = std::max(z_max, vi.z);
	}

	const auto x_mid = (x_min + x_max) / 2;
	const auto y_mid = (y_min + y_max) / 2;
	const auto z_mid = (z_min + z_max) / 2;
	const auto max_ = std::max(x_max - x_min,
		std::max(y_max - y_min, z_max - z_min));

	const auto translate = glm::mat4(
		1, 0, 0, -x_mid,
		0, 1, 0, -y_mid,
		0, 0, 1, -z_mid,
		0, 0, 0, 1
	);
	const auto scale_factor = 2 / max_;
	const auto scale = glm::mat4(
		scale_factor, 0, 0, 0,
		0, scale_factor, 0, 0,
		0, 0, scale_factor, 0,
		0, 0, 0, 1
	);
	const auto final_matrix = translate * scale;

	for (auto& vi : v)
		vi = vi * final_matrix;


	// Find rough spline center

	x_min = points_v[0].x, x_max = x_min;
	y_min = points_v[0].y, y_max = y_min;
	z_min = points_v[0].z, z_max = z_min;

	for (auto vi : points_v)
	{
		x_min = std::min(x_min, vi.x);
		x_max = std::max(x_max, vi.x);
		y_min = std::min(y_min, vi.y);
		y_max = std::max(y_max, vi.y);
		z_min = std::min(z_min, vi.z);
		z_max = std::max(z_max, vi.z);
	}

	look_at.x = (x_min + x_max) / 2;
	look_at.y = (y_min + y_max) / 2;
	look_at.z = (z_min + z_max) / 2;

	eye = { look_at.x + 50, look_at.y, look_at.z };
}

void drawSpline()
{
	glBegin(GL_LINE_STRIP);
	{
		for (auto seg = 0; seg < int(points_v.size()) - 3; seg++)
			for (float t = 0; t <= 1.f; t += 0.01f)  // NOLINT(cert-flp30-c)
				glVertex3fv(value_ptr(bSplinePos(seg, t)));
	}
	glEnd();

	glColor3f(0, 1, 0);
	glBegin(GL_LINES);
	{
		for (auto seg = 0; seg < int(points_v.size()) - 3; seg++)
			for (float t = 0; t <= 1.f; t += 0.06f) {  // NOLINT(cert-flp30-c)
				glVertex3fv(value_ptr(bSplinePos(seg, t)));
				glVertex3fv(value_ptr(bSplinePos(seg, t) + bSplineSecond(seg, t)));
			}
	}
	glEnd();
}

void drawObject()
{
	glPolygonMode(GL_FRONT, GL_LINE);
	for (auto fi : f)
	{
		glBegin(GL_POLYGON);
		{
			glVertex4fv(value_ptr(v[fi.v1]));
			glVertex4fv(value_ptr(v[fi.v2]));
			glVertex4fv(value_ptr(v[fi.v3]));
		}
		glEnd();
	}
}

glm::vec3 bSplinePos(const int segment, const float t)
{
	return glm::vec4(t * t * t, t * t, t, 1.0) * bSplineHelp(segment);
}

glm::vec3 bSplineDir(const int segment, const float t)
{
	return normalize(glm::vec4(3 * t * t, 2 * t, 1.0, 0.0) * bSplineHelp(segment));
}

glm::vec3 bSplineSecond(const int segment, const float t)
{
	return normalize(glm::vec4(6 * t, 2, 0.0, 0.0) * bSplineHelp(segment));
}

glm::mat4 bSplineHelp(const int segment)
{
	return spline_coeffs
		* glm::mat3x4(
			points_v[segment].x, points_v[segment + 1].x, points_v[segment + 2].x, points_v[segment + 3].x,
			points_v[segment].y, points_v[segment + 1].y, points_v[segment + 2].y, points_v[segment + 3].y,
			points_v[segment].z, points_v[segment + 1].z, points_v[segment + 2].z, points_v[segment + 3].z
		);
}
