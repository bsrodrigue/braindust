#version 330
// Tileset art (assets/tileset) is bright and saturated; pull it toward the game's palette:
// partial desaturation, then a warm multiply from the vertex tint.
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float uSat;
out vec4 finalColor;
void main() {
  vec4 c = texture(texture0, fragTexCoord);
  float l = dot(c.rgb, vec3(.299, .587, .114));
  finalColor = vec4(mix(vec3(l), c.rgb, uSat) * fragColor.rgb, c.a * fragColor.a);
}
