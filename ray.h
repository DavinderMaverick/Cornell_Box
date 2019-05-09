#pragma once

#include "vec3.h"

class ray
{
public:
	ray() {}
	
	ray(const vec3& a, const vec3& b) 
	{
		A = a; B = b;
		INV_B = vec3(1.0f / B.x(), 1.0f / B.y(), 1.0f / B.z());
		B_length = B.length();
	}

	vec3 origin() const { return A; }
	vec3 direction() const { return B; }
	vec3 InvDir() const { return INV_B; }
	float DirLength() const { return B_length; }
	vec3 point_at_parameter(float t) const { return A + t * B; }

	vec3 A;
	vec3 B;
	vec3 INV_B;
	float B_length;
};