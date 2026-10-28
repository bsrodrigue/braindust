#version 330
// The three layers into one, once per pixel: 3D background (texture0, may be smaller), 2D world with darkness
// (premultiplied alpha), 3D near pass. Alpha keeps the 2D world's coverage: post.fs tells solid rock from open water by it.
uniform sampler2D texture0;
uniform sampler2D texWorld;
uniform sampler2D texFg;
uniform vec2 uRes;
out vec4 finalColor;

void main() {
  vec2 uv = gl_FragCoord.xy / uRes;
  vec4 w = texture(texWorld, uv), f = texture(texFg, uv);
  vec3 c = w.rgb + texture(texture0, uv).rgb * (1.0 - w.a);
  finalColor = vec4(mix(c, f.rgb, f.a), w.a);
}
