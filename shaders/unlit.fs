#version 330
// Unlit 3D (the universe inside a skull, a leviathan's pupil): the texture times the colours, no light, no fog.
// Paired with scene.vs so 3D positions keep full precision on OpenGL ES (raylib's default shader asks for mediump).
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;

void main() { finalColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor; }
