/*!
 * \file    fragment330.glsl
 * \brief   Very simple fragment shader.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#version 330 core

// Interpolated values from the vertex shaders
//in vec3 fColor;

// Ouput data
out vec3 outColor;

void main()
{
	vec3 white      = vec3(1, 1, 1);
	//vec3 darkBrown  = vec3(0.43f, 0.22f, 0.0f);
	//vec3 lightBrown = vec3(0.75f, 0.45f, 0.10f);

	outColor = white;

	// Output color = color specified in the vertex shader
	// interpolated between all 3 surrounding vertices
	//outColor = fColor;
}
