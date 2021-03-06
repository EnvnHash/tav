// basic texShader
#version 410 core
#pragma optimize(on)

layout( location = 0 ) in vec4 position;
uniform mat4 m_pvm;

void main()
{
    gl_Position = m_pvm * position;
}