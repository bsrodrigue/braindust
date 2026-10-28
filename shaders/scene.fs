#version 330
// Underwater lighting: ambient + faint sun from above + up to 4 point lights (your cranium, universes in skulls),
// then exponential-squared fog toward the water colour.
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 ambient;
uniform vec3 sunDir;
uniform vec3 sunCol;
uniform vec3 lPos[4];
uniform vec3 lCol[4];
uniform float lRange[4];
uniform vec3 fogColor;
uniform float fogDensity;
out vec4 finalColor;

void main() {
  vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
  vec3 n = normalize(fragNormal);
  if (!gl_FrontFacing) n = -n;
  vec3 light = ambient + sunCol * max(dot(n, -sunDir), 0.0);
  for (int i = 0; i < 4; i++) {
    vec3 d = lPos[i] - fragPosition;
    float dist = length(d);
    float att = clamp(1.0 - dist / max(lRange[i], 1e-3), 0.0, 1.0);
    light += lCol[i] * att * att * max(dot(n, d / max(dist, 1e-4)), 0.0);
  }
  vec3 c = base.rgb * light;
  float dist = length(viewPos - fragPosition);
  float f = 1.0 - exp(-pow(fogDensity * dist, 2.0));
  finalColor = vec4(mix(c, fogColor, f), 1.0);
}
