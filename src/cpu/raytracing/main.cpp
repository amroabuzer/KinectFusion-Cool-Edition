#define _USE_MATH_DEFINES  // to access M_PI macro from math.h

#include <iostream>
#include <vector>
#include <math.h>
#include <Eigen/Dense>
#include "ImplicitSurface.h"
#include "Volume.h"
#include "Raycasting.h"

// TODO: choose optimal truncation value
#define TRUNCATION 1.0
// #define MAX_MARCHING_STEPS 10000
#define MAX_MARCHING_STEPS 500

/*
struct Vertex
{
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	Eigen::Vector3f position;
	Eigen::Vector3f normal;
};*/

void writePointCloud(const std::string& filename, const std::vector<Vertex>& _vertices, bool includeNormals=false)
{
	std::ofstream file(filename);

	if (!includeNormals)
	{
		file << "OFF" << std::endl;
		file << _vertices.size() << " 0 0" << std::endl;

		for (unsigned int i = 0; i < _vertices.size(); ++i)
			file << _vertices[i].position[0] << " " << _vertices[i].position[1] << " " << _vertices[i].position[2] << std::endl;
	}
	else
	{
		for (unsigned int i = 0; i < _vertices.size(); ++i)
		{
			file << "v " << _vertices[i].position[0] << " " << _vertices[i].position[1] << " " << _vertices[i].position[2] << std::endl;
			file << "vn " << _vertices[i].normal[0] << " " << _vertices[i].normal[1] << " " << _vertices[i].normal[2] << std::endl;
		}
	}
}
float trilinearInterpolation(const Eigen::Vector3f& point,
	Volume& volume,
	const int voxel_grid_dim_x,
	const int voxel_grid_dim_y,
	const int voxel_grid_dim_z) {

	Eigen::Vector3i point_in_grid = point.cast<int>();

	const float vx = (static_cast<float>(point_in_grid[0]) + 0.5f);
	const float vy = (static_cast<float>(point_in_grid[1]) + 0.5f);
	const float vz = (static_cast<float>(point_in_grid[2]) + 0.5f);

	point_in_grid[0] = (point[0] < vx) ? (point_in_grid[0] - 1) : point_in_grid[0];
	point_in_grid[1] = (point[1] < vy) ? (point_in_grid[1] - 1) : point_in_grid[1];
	point_in_grid[2] = (point[2] < vz) ? (point_in_grid[2] - 1) : point_in_grid[2];

	const float a = (point.x() - (static_cast<float>(point_in_grid[0]) + 0.5f));
	const float b = (point.y() - (static_cast<float>(point_in_grid[0]) + 0.5f));
	const float c = (point.z() - (static_cast<float>(point_in_grid[0]) + 0.5f));

	const int xd = point_in_grid[0];
	const int yd = point_in_grid[1];
	const int zd = point_in_grid[2];
	//std::cout << "Volume" << volume;
	std::cout << "X,y,z Values:" << xd << " " << yd << " " << zd << "\n";
	std::cout << "Volume values: " << volume.getDimX() << " " << volume.getDimY() << " " << volume.getDimZ() << "\n ";
	std::cout << "Voxel_grid values: " << voxel_grid_dim_x << " " << voxel_grid_dim_x << " " << voxel_grid_dim_x << "\n ";
	const float c000 = volume.get((xd), (yd)*voxel_grid_dim_x, (zd)*voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c000 << "\n";
	const float c001 = volume.get((xd), (yd)*voxel_grid_dim_x, (zd + 1) * voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c001 << "\n";
	const float c010 = volume.get((xd), (yd + 1) * voxel_grid_dim_x, (zd)*voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c010 << "\n";
	const float c011 = volume.get((xd), (yd + 1) * voxel_grid_dim_x, (zd + 1) * voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c011 << "\n";
	const float c100 = volume.get((xd + 1), (yd)*voxel_grid_dim_x, (zd)*voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c100 << "\n";
	const float c101 = volume.get((xd + 1), (yd)*voxel_grid_dim_x, (zd + 1) * voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c101 << "\n";
	const float c110 = volume.get((xd + 1), (yd + 1) * voxel_grid_dim_x, (zd)*voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c110 << "\n";
	const float c111 = volume.get((xd + 1), (yd + 1) * voxel_grid_dim_x, (zd + 1) * voxel_grid_dim_x * voxel_grid_dim_y);
	std::cout << c111 << "\n";
	return c000 * (1 - a) * (1 - b) * (1 - c) +
		c001 * (1 - a) * (1 - b) * c +
		c010 * (1 - a) * b * (1 - c) +
		c011 * (1 - a) * b * c +
		c100 * a * (1 - b) * (1 - c) +
		c101 * a * (1 - b) * c +
		c110 * a * b * (1 - c) +
		c111 * a * b * c;
}

int main()
{
	// Eigen::Vector3f cameraCenter(2.0f, 2.0f, -2.0f);
	Eigen::Vector3f cameraCenter(1.5f, 1.5f, -1.5f);

	// Define rotation with Euler angles
	float alpha = 30 * (M_PI / 180);  // x
	float beta = -30 * (M_PI / 180);   // y
	float gamma = 0 * (M_PI / 180);  // z
	Eigen::Matrix3f rotationX;
	Eigen::Matrix3f rotationY;
	Eigen::Matrix3f rotationZ;

	rotationX << 1.0f, 0.0f, 0.0f, 
				 0.0f, cos(alpha), -sin(alpha), 
				 0.0f, sin(alpha), cos(alpha);
	rotationY << cos(beta), 0.0f, sin(beta),
				 0.0f, 1.0f, 0.0f, 
				 -sin(beta), 0.0f, cos(beta);
	rotationZ << cos(gamma), -sin(gamma), 0.0f,
				 sin(gamma), cos(gamma), 0.0f,
				 0.0f, 0.0f, 1.0f;

	Eigen::Matrix3f rotation = rotationZ * rotationY * rotationX;

	// std::vector<Eigen::Vector3f> normals;

	// Init implicit surface
	Sphere implicit = Sphere(Eigen::Vector3d(0.5, 0.5, 0.5), 0.4);
	// Fill spatial grid with distance to the implicit surface
	// unsigned int mc_res = 600;
	unsigned int mc_res = 50;
	Volume vol(Eigen::Vector3d(-0.1, -0.1, -0.1), Eigen::Vector3d(1.1, 1.1, 1.1), mc_res, mc_res, mc_res, 1);
	for (unsigned int x = 0; x < vol.getDimX(); x++)
	{
		for (unsigned int y = 0; y < vol.getDimY(); y++)
		{
			for (unsigned int z = 0; z < vol.getDimZ(); z++)
			{
				Eigen::Vector3d p = vol.pos(x, y, z);
				double val = implicit.Eval(p);

				if (val < TRUNCATION)
					vol.set(x, y, z, val);
				else
					vol.set(x, y, z, TRUNCATION);
			}
		}
	}
	

	std::vector<Vertex> vertices;

	Raycasting r(vol, rotation, cameraCenter);
	
	r.castAll();

	vertices = r.getVertices();

	writePointCloud("pointcloud.obj", vertices, true);

	return 0;
}