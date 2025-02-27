/*!
 * \file    fragment130.glsl
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
#version 130

// Ouput data
out vec3 outColor;

void main()
{
	vec3 white = vec3(1, 1, 1);
	outColor = white;
}
