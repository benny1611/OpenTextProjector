#version 120
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;

void main()
{    
	color = vec4(textColor, 1.0) * texture2D(text, TexCoords).r;
}  