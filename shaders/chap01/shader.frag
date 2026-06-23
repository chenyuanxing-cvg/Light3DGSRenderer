#version 430 core

in vec3 outColor;
in float opacity;
in vec3 conic;
in vec2 coordxy;

out vec4 FragColor;

void main()
{
    //float power = -0.5 * (conic.x * coordxy.x * coordxy.x +
    //                      conic.z * coordxy.y * coordxy.y) -
    //              conic.y * coordxy.x * coordxy.y;

    float power = -0.5 * (conic.x * coordxy.x * coordxy.x +2.0 * conic.y * coordxy.x * coordxy.y +conic.z * coordxy.y * coordxy.y);

    if (power > 0.0)
        discard;

    float alpha = min(0.99, opacity * exp(power));

    if (alpha < 1.0 / 255.0)
        discard;

    FragColor = vec4(outColor, alpha);
    //FragColor = vec4(1,1,1,alpha*0.1);
}
