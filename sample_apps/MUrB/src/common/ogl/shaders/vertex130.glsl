/*!
 * \file    vertex130.glsl
 * \brief   This shader apply Model View Projection model on each vertex.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#version 130

// Input vertex data, different for all executions of this shader.
in vec3	 modelPerVertex;
in float positionXPerVertex;
in float positionYPerVertex;
in float positionZPerVertex;

// Input colors
in float radiusPerVertex;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;

void main()
{
	float scale = radiusPerVertex * 1.0e-8f;

	 // trick in order to avoid start point at the center of the sphere
	if(modelPerVertex.x == 0.0f && modelPerVertex.y == 0.0f && modelPerVertex.z == 0.0f)
		gl_Position = MVP * vec4(1.0e-8f * positionXPerVertex + (scale * 0),
		                         1.0e-8f * positionYPerVertex + (scale * 1),
		                         1.0e-8f * positionZPerVertex + (scale * 0),
								 1);
	else
		gl_Position = MVP * vec4(1.0e-8f * positionXPerVertex + (scale * modelPerVertex.x),
		                         1.0e-8f * positionYPerVertex + (scale * modelPerVertex.y),
		                         1.0e-8f * positionZPerVertex + (scale * modelPerVertex.z),
		                         1);
}
