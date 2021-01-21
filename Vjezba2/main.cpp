// ReSharper disable CppInconsistentNaming
#include <gl/freeglut.h>
#include <algorithm>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <chrono>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


GLuint width = 900, height = 900;


struct Particle {
	glm::vec3 pos, speed;
	unsigned char r, g, b;
	float size, start_size, end_size;
	float life, start_life;
	float cameraDistance;

	bool operator<(Particle& other) const
	{
		return this->cameraDistance > other.cameraDistance;
	}
};

struct ForceSource
{
	glm::vec3 pos;
	float power;
};

const int MaxParticles = 15000;
Particle ParticlesContainer[MaxParticles];
int LastUsedParticle = 0;

float particles_per_second = 3000.f;
float particles_per_frame_limit = 0.016f * particles_per_second;

glm::vec3 eye = { 50,0,0 };
glm::vec3 look_at = { 0,3,-20 };
glm::vec3 view_up = { 0,1,0 };

glm::mat4 view_mat = lookAt(eye, look_at, view_up);
glm::vec3 cameraRight_worldspace = { view_mat[0][0], view_mat[1][0], view_mat[2][0] };
glm::vec3 cameraUp_worldspace = { view_mat[0][1], view_mat[1][1], view_mat[2][1] };

glm::vec3 spawn_pos = { 0, 0, 0 };
glm::vec3 spawn_speed = { 0, 15, -9 };

glm::vec3 gravity = { 0.0f, -9.81f, 0.0f };
std::vector<ForceSource> forces = {
	{{0, 13.f, -40.f}, 2200 }
};

unsigned char start_color_r = 0, start_color_g = 0, start_color_b = 255;
unsigned char end_color_r = 170, end_color_g = 200, end_color_b = 230;

std::mt19937 rng(std::random_device{}());
std::normal_distribution<float> pos_normal_d(0.f, .7f);
std::normal_distribution<float> speed_normal_d(0.f, 1);
std::uniform_real_distribution<float> start_life_d(2.f, 4.f);
std::uniform_real_distribution<float> start_size_d(0.4f, 1.8f);
std::uniform_real_distribution<float> end_size_factor_d(3.f, 8.f);

GLuint textureId;


void myDisplay();
void myReshape(int w, int h);
void myIdle();
inline unsigned char interpolateColor(Particle& p, unsigned char start, unsigned char end);
long long getCurrentTimeMicro();
int findUnusedParticle();
void drawParticles();


int main(int argc, char** argv)
{
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(width, height);
	glutInitWindowPosition(400, 200);
	glutInit(&argc, argv);

	glutCreateWindow("Particles");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	auto image_x = 128, image_y = 128, n = 4;
	const auto imageData = stbi_load("snow_alpha.png", &image_x, &image_y, &n, 0);

	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image_x, image_y,
		0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

	glutReshapeFunc(myReshape);
	glutDisplayFunc(myDisplay);

	glutIdleFunc(myIdle);

	glutMainLoop();
	stbi_image_free(imageData);
	return 0;
}

void myDisplay()
{
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(value_ptr(view_mat));

	drawParticles();

	glutSwapBuffers();
}

void myReshape(const int w, const int h)
{
	width = w; height = h;
	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(value_ptr(glm::perspective(
		glm::radians(60.0f), float(w) / float(h), 0.01f, 10000.0f)));
}

void myIdle()
{
	static auto lastTime = -1LL;
	if (lastTime == -1LL)
		lastTime = getCurrentTimeMicro();

	const auto currTime = getCurrentTimeMicro();
	const auto timeDeltaMicro = static_cast<int>(currTime - lastTime);
	const auto timeDeltaSecond = timeDeltaMicro / float(1e6);

	lastTime = currTime;

	const auto newParticles = int(std::min(
		particles_per_second * timeDeltaSecond,
		particles_per_frame_limit));

	for (auto& p : ParticlesContainer)
	{
		p.life -= timeDeltaSecond;
		if (p.life <= 0)
			continue;

		auto acceleration = gravity;
		for (auto& force : forces)
			acceleration += force.power / powf(distance(force.pos, p.pos), 2) * normalize(force.pos - p.pos);

		p.speed += acceleration * timeDeltaSecond;
		p.pos += p.speed * timeDeltaSecond;
		p.cameraDistance = length(p.pos - eye);

		p.r = interpolateColor(p, start_color_r, end_color_r);
		p.g = interpolateColor(p, start_color_g, end_color_g);
		p.b = interpolateColor(p, start_color_b, end_color_b);

		p.size = (p.start_life - p.life) / p.start_life * (p.end_size - p.start_size) + p.start_size;
	}

	for (auto i = 0; i < newParticles; ++i)
	{
		auto& p = ParticlesContainer[findUnusedParticle()];

		p.pos = {
			spawn_pos.x + pos_normal_d(rng),
			spawn_pos.y + pos_normal_d(rng),
			spawn_pos.z + pos_normal_d(rng)
		};
		p.speed = {
			spawn_speed.x + speed_normal_d(rng),
			spawn_speed.y + speed_normal_d(rng),
			spawn_speed.z + speed_normal_d(rng)
		};
		p.cameraDistance = length(p.pos - eye);

		p.r = start_color_r;
		p.g = start_color_g;
		p.b = start_color_b;

		p.size = p.start_size = start_size_d(rng);
		p.end_size = p.start_size / end_size_factor_d(rng);

		p.life = p.start_life = start_life_d(rng);
	}

	std::sort(&ParticlesContainer[0], &ParticlesContainer[MaxParticles]);

	glutPostRedisplay();
}

inline unsigned char interpolateColor(Particle& p, const unsigned char start, const unsigned char end)
{
	return unsigned char((p.start_life - p.life) / p.start_life * (end - start) + start);
}

inline long long getCurrentTimeMicro()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int findUnusedParticle() {
	for (auto i = (LastUsedParticle + 1) % MaxParticles; i != LastUsedParticle; i = (i + 1) % MaxParticles)
		if (ParticlesContainer[i].life <= 0)
		{
			LastUsedParticle = i;
			return i;
		}
	return 0;
}

void drawParticles()
{
	for (auto& p : ParticlesContainer)
	{
		if (p.life <= 0)
			continue;

		auto delta_right = cameraRight_worldspace * p.size / 2.f;
		auto delta_up = cameraUp_worldspace * p.size / 2.f;

		glColor3ub(p.r, p.g, p.b);
		glBegin(GL_POLYGON);
		{
			glTexCoord2f(0.0, 0.0);
			glVertex3fv(value_ptr(p.pos - delta_right - delta_up));

			glTexCoord2f(1.0, 0.0);
			glVertex3fv(value_ptr(p.pos + delta_right - delta_up));

			glTexCoord2f(1.0, 1.0);
			glVertex3fv(value_ptr(p.pos + delta_right + delta_up));

			glTexCoord2f(0.0, 1.0);
			glVertex3fv(value_ptr(p.pos - delta_right + delta_up));
		}
		glEnd();
	}
}
