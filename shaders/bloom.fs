#version 330
// Bloom, drawn at a quarter of the resolution and stretched by post.fs: the bright parts of the merged scene (texture0)
// gathered over two rings, 7 and 18 px out at 700 px tall.
uniform sampler2D texture0;
uniform vec2 uRes;  // the scene's size, px
uniform vec2 uOut;  // this target's size, px
out vec4 finalColor;

void main() {
  vec2 uv = gl_FragCoord.xy / uOut, px = vec2(1.0) / uRes * (uRes.y / 700.0);
  vec3 bl = vec3(0.0);
  for (int i = 0; i < 8; i++) {
    float a = float(i) * .785 + .3;
    vec2 o = vec2(cos(a), sin(a));
    bl += max(texture(texture0, uv + o * px * 7.0).rgb - .3, 0.0) + .6 * max(texture(texture0, uv + o * px * 18.0).rgb - .28, 0.0);
  }
  finalColor = vec4(bl * .16, 1.0);
}
