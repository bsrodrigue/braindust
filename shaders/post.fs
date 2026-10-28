#version 330
// Final composite: 3D background (texture0), 2D world with darkness (premultiplied alpha), 3D near pass,
// then refraction, chromatic aberration, bloom, god rays, caustics, grade, vignette and grain.
// All layers share one orientation, so everything samples at gl_FragCoord / uRes.
uniform sampler2D texture0;
uniform sampler2D texWorld;
uniform sampler2D texFg;
uniform vec2 uRes;
uniform vec2 uCam;
uniform float uS, uTime, uShallow, uDeep, uPocket, uWarp;
uniform float uWaterY;  // screen px from the top where the sea begins; above it is sky and land
uniform float uDark;    // darkness overlay alpha this frame: lets us tell solid rock (alpha 1) from open water
out vec4 finalColor;

vec3 sceneAt(vec2 uv) {
  vec4 w = texture(texWorld, uv), f = texture(texFg, uv);
  vec3 c = w.rgb + texture(texture0, uv).rgb * (1.0 - w.a);
  return mix(c, f.rgb, f.a);
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float caustic(vec2 p, float time) {  // after the classic tileable water caustic
  vec2 q = mod(p * 6.2831, 6.2831) - 250.0, i = q;
  float c = 1.0, inten = .005;
  for (int n = 0; n < 4; n++) {
    float t = time * (1.0 - (3.5 / float(n + 1)));
    i = q + vec2(cos(t - i.x) + sin(t + i.y), sin(t - i.y) + cos(t + i.x));
    c += 1.0 / length(vec2(q.x / (sin(i.x + t) / inten), q.y / (cos(i.y + t) / inten)));
  }
  c /= 4.0;
  c = 1.17 - pow(c, 1.4);
  return clamp(pow(abs(c), 8.0), 0.0, 1.0);
}

void main() {
  vec2 uv = gl_FragCoord.xy / uRes, dc = uv - .5;
  float sy = uRes.y - gl_FragCoord.y;
  float sea = (1.0 - uPocket) * smoothstep(uWaterY - 2.0, uWaterY + 8.0, sy);  // 1 under the water, 0 in the air
  // refraction; places that burst a mage's skull bend the water much harder
  float wob = (.0011 + uDeep * .0007) * sea + uWarp * .006;
  uv += vec2(sin(uv.y * 38.0 + uTime * 1.7) + .5 * sin(uv.y * 71.0 - uTime * 2.3), cos(uv.x * 29.0 + uTime * 1.3)) * wob;
  float ca = dot(dc, dc) * (.007 + uPocket * .012 + uWarp * .03);
  vec3 col = vec3(sceneAt(uv + dc * ca).r, sceneAt(uv).g, sceneAt(uv - dc * ca).b);
  // bloom
  vec3 bl = vec3(0.0);
  vec2 px = vec2(1.0) / uRes * (uRes.y / 700.0);
  for (int i = 0; i < 8; i++) {
    float a = float(i) * .785 + .3;
    vec2 o = vec2(cos(a), sin(a));
    bl += max(sceneAt(uv + o * px * 7.0) - .3, 0.0) + .6 * max(sceneAt(uv + o * px * 18.0) - .28, 0.0);
  }
  col += bl * .16;
  // solid rock gets no water light: god rays, caustics and the teal lift belong to open water
  float rock = clamp((texture(texWorld, uv).a - uDark) / max(1.0 - uDark, .05), 0.0, 1.0);
  // god rays from the surface, broken by the shaft walls
  if (uShallow > .01 && sea > .5) {
    vec2 lp = vec2(.35 + sin(uTime * .05) * .15, 1.3), d = (uv - lp) / 16.0, p = uv;
    float ill = 0.0, decay = 1.0;
    for (int i = 0; i < 16; i++) {
      p -= d;
      float open = (1.0 - p.y) * uRes.y < uWaterY ? 1.0 : 1.0 - texture(texWorld, p).a;  // the sky lets light in
      ill += open * (.55 + .45 * sin(p.x * 46.0 + uTime * .4) * sin(p.x * 17.0 - uTime * .25)) * decay;
      decay *= .9;
    }
    col += vec3(.32, .5, .55) * ill * .05 * uShallow * sea * (1.0 - rock);
  }
  // world-anchored caustics
  vec2 wp = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y) / uS + uCam;
  float cs = caustic(wp / 150.0, uTime * .55) * uShallow * sea * (1.0 - rock);
  col += col * cs * .45 + vec3(.03, .06, .07) * cs * .12;
  // grade: teal floor in the shadows, slight desaturation
  float lum = dot(col, vec3(.299, .587, .114));
  col += vec3(.025, .07, .085) * (1.0 - smoothstep(0.0, .25, lum)) * sea * (.6 + .4 * uDeep) * (1.0 - rock * .8);
  col = mix(vec3(lum), col, .86) * mix(vec3(1.05, 1.0, .93), vec3(.95, 1.02, 1.04), sea);  // warm dusk above, cool sea below
  col += vec3(.05, .0, .08) * uWarp * (.5 + .5 * sin(uTime * 3.0 + uv.y * 20.0));
  // vignette and grain
  col *= mix(.55, 1.0, smoothstep(.95, .3, length(dc * vec2(1.15, 1.0))));
  col += (hash(gl_FragCoord.xy + fract(uTime) * 91.0) - .5) * .045;
  finalColor = vec4(col, 1.0);
}
