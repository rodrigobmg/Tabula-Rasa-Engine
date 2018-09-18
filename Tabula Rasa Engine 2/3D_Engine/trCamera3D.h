#ifndef __trCAMERA3D_H__
#define __trCAMERA3D_H__

#include "Globals.h" 
#include "glmath.h"

#include "trApp.h"

class trCamera3D : public trModule
{
public:
	trCamera3D();
	~trCamera3D();

	bool Start();
	bool Update(float dt);
	bool CleanUp();

	void Look(const vec3 &Position, const vec3 &Reference, bool RotateAroundReference = false);
	void LookAt(const vec3 &Spot);
	void Move(const vec3 &Movement);
	float* GetViewMatrix();

private:

	void CalculateViewMatrix();

public:

	vec3 X, Y, Z, Position, Reference;

private:

	mat4x4 ViewMatrix, ViewMatrixInverse;
};

#endif