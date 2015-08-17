
#version 330 core

uniform vec4  tri_color = vec4(1,1,0, 1);
uniform sampler2D tex_cicle;

in vec2 tex_coord; // name based matching

layout(location=0) out vec4 frag_color;

void main()
{
	//frag_color = tri_color * texture(tex_cicle, vec2(tex_coord.x/tex_coord.w, tex_coord.y/tex_coord.w));
	frag_color = tri_color * texture(tex_cicle, tex_coord);
}

