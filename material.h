#pragma once
#include "hitable.h"

#include <stdlib.h>
#include <limits>

#define drand48() (rand() / (RAND_MAX + 1.0))

vec3 random_in_unit_sphere();
vec3 reflect(const vec3& v, const vec3& n);
bool refract(const vec3& v, const vec3& n, float ni_over_nt, vec3& refracted);
float schlick(float cosine, float ref_idx);

//Abstract Class
class material
{
public:
	virtual bool scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered) const = 0;
	virtual vec3 emitted() const { return vec3(0, 0, 0); }
};
//material tells us how rays interact with the surface

class lambertian :public material
{
public:
	lambertian(const vec3& a) :albedo(a) {}
	virtual bool scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered) const
	{
		vec3 target = rec.p + rec.normal + random_in_unit_sphere();
		scattered = ray(rec.p, target - rec.p);
		attenuation = albedo;
		return true;
	}

	vec3 albedo;
};

class metal :public material
{
public:
	metal(const vec3& a, float f) : albedo(a) { if (f < 1) fuzz = f; else fuzz = 1; }
	virtual bool scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered) const
	{
		vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
		scattered = ray(rec.p, reflected + fuzz * random_in_unit_sphere());
		attenuation = albedo;
		return (dot(scattered.direction(), rec.normal) > 0);
	}
	vec3 albedo;
	float fuzz;
};

class diffuse_light : public material
{
public:
	diffuse_light(vec3 color) : emit(color) {}

	virtual bool scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered) const { return false; }

	virtual vec3 emitted() const
	{
		return emit;
	}

	vec3 emit;
};

class dielectric : public material
{
public:
	dielectric(float ri) : ref_idx(ri) {} //ri -> refractive index of the material
	virtual bool scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered) const
	{
		vec3 outward_normal;
		vec3 reflected = reflect(r_in.direction(), rec.normal);
		float ni_over_nt;//incidence over transmission medium
		attenuation = vec3(1.0f, 1.0f, 1.0f);
		vec3 refracted;
		float reflect_prob;
		float cosine;
		if (dot(r_in.direction(), rec.normal) > 0)
		{
			//The ray origin is inside the medium and going outside.
			//So reverse normal direction, and also swap n1 and n2
			outward_normal = -rec.normal;
			ni_over_nt = ref_idx;
			//cosine = ref_idx * dot(r_in.direction(), rec.normal) / r_in.direction().length();
			cosine = ref_idx * dot(r_in.direction(), rec.normal) / r_in.DirLength();
		}
		else
		{
			outward_normal = rec.normal;
			ni_over_nt = 1.0 / ref_idx;
			//cosine = -dot(r_in.direction(), rec.normal) / r_in.direction().length();
			cosine = -dot(r_in.direction(), rec.normal) / r_in.DirLength();
		}

		if (refract(r_in.direction(), outward_normal, ni_over_nt, refracted))
		{
			//scattered = ray(rec.p, refracted);
			reflect_prob = schlick(cosine, ref_idx);
		}
		else
		{
			//scattered = ray(rec.p, reflected);
			reflect_prob = 1.0; //When TIR happens set to 1
		}

		//When a light ray hits the dielectric surface it splits into a reflected ray and a refracted (transmitted) ray. We'll handle that by randomly choosing between reflection or refraction and only generating one scattered ray per interaction.
		if (drand48() < reflect_prob)
		{
			scattered = ray(rec.p, reflected);
		}
		else
		{
			scattered = ray(rec.p, refracted);
		}

		return true;
	}

	float ref_idx;
};

vec3 reflect(const vec3& v, const vec3& n)
{
	return v - 2 * dot(v, n) * n;
}

vec3 random_in_unit_sphere()
{
	vec3 p;
	do
	{
		p = 2.0 * vec3(drand48(), drand48(), drand48()) - vec3(1, 1, 1);
	} while (p.squared_length() >= 1.0);
	return p;
}

bool refract(const vec3& v, const vec3& n, float ni_over_nt, vec3& refracted)
{
	vec3 uv = unit_vector(v);
	float dt = dot(uv, n);
	float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1 - dt * dt);
	if (discriminant > 0)
	{
		refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
		return true;
	}
	else
	{
		//TIR - Total Internal Reflection
		return false;
	}
}

float schlick(float cosine, float ref_idx)
{
	float r0 = (1 - ref_idx) / (1 + ref_idx);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}