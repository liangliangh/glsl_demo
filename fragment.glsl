
#version 330 core

uniform vec4  tri_color = vec4(1,1,0, 1);

in vec4 tex_coord; // name based matching

layout(location=0) out vec4 frag_color;

void main()
{
	frag_color = tri_color;
}

