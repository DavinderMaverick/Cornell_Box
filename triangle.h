#pragma once

#include "hitable.h"
#include "hitablelist.h"
#include <iostream>
class triangle : public hitable
{
public:
	triangle() {}
	triangle(const vec3& vert0, const vec3& vert1, const vec3& vert2, material *ptr);
	virtual bool hit(const ray& r, float t_min, float t_max, hit_record& rec) const;
	virtual bool bounding_box(aabb& box) const
	{
		box = aabb(vec3(pmin[0], pmin[1], pmin[2] - 0.0001), vec3(pmax[0], pmax[1], pmax[2] + 0.0001));
		//box = aabb(pmin, pmax);
		return true;
	}

	bool geometricSolution(const ray& r, float t_min, float t_max, hit_record& rec) const;
	bool MTAlgo(const ray& r, float t_min, float t_max, hit_record& rec) const;

	vec3 v0, v1, v2;
	vec3 pmin, pmax;
	material *mat_ptr;
	vec3 N;//Normal
};

triangle::triangle(const vec3& vert0, const vec3& vert1, const vec3& vert2, material *ptr)
{
	v0 = vert0;
	v1 = vert1;
	v2 = vert2;
	mat_ptr = ptr;

	for (int i = 0; i < 3; i++)
	{
		pmin[i] = std::min(std::min(v0[i], v1[i]), v2[i]);
		pmax[i] = std::max(std::max(v0[i], v1[i]), v2[i]);
	}

	//Computing Normal
	vec3 v0v1 = v1 - v0;
	vec3 v0v2 = v2 - v0;
	N = cross(v0v1, v0v2);
	float area2 = N.length();
	N = N / area2;
}

bool triangle::hit(const ray& r, float t_min, float t_max, hit_record& rec) const
{
	return MTAlgo(r, t_min, t_max, rec);
}


/*
First we will compute the triangle's normal, then test if the ray and the triangle are parallel. If they are, the intersection test fails. If they are not parallel, we compute t from which we can compute the intersection point P. If the inside-out test succeeds (we test if P is on the left side of each one of the triangle's edges) then the ray intersects the triangle and P is inside the triangle's boundaries
*/
bool triangle::geometricSolution(const ray& r, float t_min, float t_max, hit_record& rec) const
{
	float t;
	//compute plane's normal
	vec3 v0v1 = v1 - v0;
	vec3 v0v2 = v2 - v0;
	
	//Step 1: finding P

	//Check if ray and plane are parallel
	float RayNDot = dot(r.direction(), N);
	if (RayNDot <= 0)
		return false; //they are parallel so don't intersect

	//compute d of plane
	float d = dot(N, v0);

	//compute t
	t = -(dot(N, r.origin()) - d) / RayNDot;

	//check if the triangle is behind the ray
	if (t < 0) return false; //the triangle is behind

	if (t < t_min || t > t_max)
		return false;

	//compute the intersection point
	vec3 P = r.origin() + t * r.direction();

	//Step 2: Inside-Outside Test
	vec3 C;//vector perpendicular to triangle's plane

	//edge 0 
	vec3 e0 = v1 - v0;
	vec3 vp0 = P - v0;
	C = cross(e0, vp0);
	if (dot(N, C) < 0) return false; //P is on the right side

	//edge 1 
	vec3 e1 = v2 - v1;
	vec3 vp1 = P - v1;
	C = cross(e1, vp1);
	if (dot(N, C) < 0) return false; //P is on the right side

	//edge 2 
	vec3 e2 = v0 - v2;
	vec3 vp2 = P - v2;
	C = cross(e2, vp2);
	if (dot(N, C) < 0) return false; //P is on the right side

	rec.mat_ptr = mat_ptr;
	rec.t = t;
	rec.normal = N;
	rec.p = r.point_at_parameter(t);

	return true; //this ray hits the triangle
}

bool triangle::MTAlgo(const ray& r, float t_min, float t_max, hit_record& rec) const
{
	vec3 v0v1 = v1 - v0;
	vec3 v0v2 = v2 - v0;
	float t, u, v;

	vec3 pvec = cross(r.direction(), v0v2);
	float det = dot(v0v1, pvec);

	// if the determinant is close to 0, the ray misses the triangle
	if (abs(det) < 0.001)
		return false;

	// if the determinant is negative the triangle is backfacing
	//if (det < 0)
		//return false;
	float RayNDot = dot(r.direction(), N);
	if (RayNDot < 0)
		return false;

	float invDet = 1.0f / det;

	vec3 tvec = r.origin() - v0;

	u = dot(tvec, pvec) * invDet;

	if (u < 0 || u > 1) return false;

	vec3 qvec = cross(tvec, v0v1);

	v = dot(r.direction(), qvec) * invDet;

	if (v < 0 || u + v > 1) return false;

	t = dot(v0v2, qvec) * invDet;

	if (t < t_min || t > t_max)
		return false;
	rec.t = t;
	rec.mat_ptr = mat_ptr;
	rec.normal = N;
	rec.p = r.point_at_parameter(t);
	//std::cout << rec.p << std::endl;
	return true;
}