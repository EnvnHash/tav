#version 330 core

uniform vec2      tcOffset[25]; // Texture coordinate offsets
uniform sampler2D tex;
in vec2 tex_coord;
in vec4 col;
layout (location = 0) out vec4 color;

void main()
{
    vec4 sample[25];
    
    for (int i = 0; i < 25; i++)
    {
        // Sample a grid around and including our texel
        sample[i] = texture(tex, tex_coord + tcOffset[i]);
    }
    
    // Gaussian weighting:
    // 1  4  7  4 1
    // 4 16 26 16 4
    // 7 26 41 26 7 / 273 (i.e. divide by total of weightings)
    // 4 16 26 16 4
    // 1  4  7  4 1
    
    color = (
             (1.0  * (sample[0] + sample[4]  + sample[20] + sample[24])) +
             (4.0  * (sample[1] + sample[3]  + sample[5]  + sample[9] + sample[15] + sample[19] + sample[21] + sample[23])) +
             (7.0  * (sample[2] + sample[10] + sample[14] + sample[22])) +
             (16.0 * (sample[6] + sample[8]  + sample[16] + sample[18])) +
             (26.0 * (sample[7] + sample[11] + sample[13] + sample[17])) +
             (41.0 * sample[12])
             ) / 273.0;
}