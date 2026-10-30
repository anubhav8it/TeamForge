"use strict";
// Animated background for the TeamForge web pages (sign-in, app loading screen): slow glows in
// the app's colours (red, violet, blue) drifting over graphite, warped by noise, with a soft
// light that follows the pointer and a little film grain. One WebGL fragment shader, no library.
//
// Kept light on purpose: it renders at a fraction of the screen's resolution (the image is soft
// anyway), at most 30 frames a second, stops while the tab is hidden, draws a single still frame
// for people who ask for reduced motion, and leaves the page's CSS gradient in place when WebGL
// is unavailable. window.TFBackdrop.stop() releases the GPU (the app loader calls it once the
// TeamForge app has started, so the two never render at the same time).
(function () {
  const canvas = document.getElementById("backdrop");
  if (!canvas) return;

  const reduceMotion = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const coarse = window.matchMedia && window.matchMedia("(pointer: coarse)").matches;
  const gl = canvas.getContext("webgl", { antialias: false, alpha: false, depth: false, stencil: false,
                                          powerPreference: "low-power", preserveDrawingBuffer: false });
  if (!gl) { canvas.classList.add("off"); return; }

  const vertexSource = "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }";
  const fragmentSource = [
    // highp where available: the hash() noise bands visibly at mediump on some phones.
    "#ifdef GL_FRAGMENT_PRECISION_HIGH",
    "precision highp float;",
    "#else",
    "precision mediump float;",
    "#endif",
    "uniform vec2 res; uniform float t; uniform vec2 pointer; uniform float pointerOn;",
    "float hash(vec2 v) { return fract(sin(dot(v, vec2(127.1, 311.7))) * 43758.5453); }",
    "float noise(vec2 v) {",
    "  vec2 i = floor(v); vec2 f = fract(v); vec2 u = f * f * (3.0 - 2.0 * f);",
    "  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);",
    "}",
    "float fbm(vec2 v) { float s = 0.0; float a = 0.5; for (int k = 0; k < 4; k++) { s += a * noise(v); v *= 2.03; a *= 0.5; } return s; }",
    "float glow(vec2 uv, vec2 c, float r) { float d = length(uv - c); return exp(-d * d / (r * r)); }",
    "void main() {",
    "  vec2 uv = gl_FragCoord.xy / res; float aspect = res.x / res.y;",
    "  vec2 q = vec2(uv.x * aspect, uv.y);",
    // Domain warp: the glows bend through slowly moving noise, like smoke under a lamp.
    "  vec2 w = vec2(fbm(q * 1.6 + vec2(0.0, t * 0.05)), fbm(q * 1.6 + vec2(5.2, -t * 0.04)));",
    "  vec2 p = q + (w - 0.5) * 0.55;",
    "  vec3 base = vec3(0.106, 0.122, 0.149);",                                       // #1B1F26
    "  vec3 red = vec3(0.941, 0.263, 0.290);",                                        // #F0434A
    "  vec3 violet = vec3(0.655, 0.545, 0.980);",                                     // #A78BFA
    "  vec3 blue = vec3(0.353, 0.690, 1.0);",                                         // #5AB0FF
    "  vec2 c1 = vec2(0.18 * aspect + 0.10 * sin(t * 0.11), 0.86 + 0.07 * cos(t * 0.13));",
    "  vec2 c2 = vec2(0.92 * aspect + 0.08 * cos(t * 0.09), 0.12 + 0.08 * sin(t * 0.12));",
    "  vec2 c3 = vec2(0.70 * aspect + 0.12 * sin(t * 0.07 + 1.3), 0.95 + 0.05 * sin(t * 0.1));",
    "  vec3 col = base;",
    "  col += red * glow(p, c1, 0.55) * 0.42;",
    "  col += violet * glow(p, c2, 0.60) * 0.30;",
    "  col += blue * glow(p, c3, 0.40) * 0.16;",
    // Fine smoke texture inside the glows.
    "  float smoke = fbm(p * 3.2 + vec2(t * 0.03, 0.0));",
    "  col += (smoke - 0.5) * 0.06 * (col - base) * 4.0;",
    // The pointer's own light, warm red.
    "  vec2 m = vec2(pointer.x * aspect, pointer.y);",
    "  col += red * glow(q, m, 0.22) * 0.20 * pointerOn;",
    // Vignette and grain.
    "  float v = smoothstep(1.25, 0.25, length((uv - 0.5) * vec2(aspect * 0.9, 1.0)));",
    "  col *= mix(0.72, 1.0, v);",
    "  col += (hash(gl_FragCoord.xy + fract(t) * 37.0) - 0.5) * 0.025;",
    "  gl_FragColor = vec4(col, 1.0);",
    "}",
  ].join("\n");

  function compile(type, source) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader) || "shader");
    return shader;
  }
  let program;
  try {
    program = gl.createProgram();
    gl.attachShader(program, compile(gl.VERTEX_SHADER, vertexSource));
    gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragmentSource));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error("link");
  } catch (e) {
    canvas.classList.add("off");
    return;
  }
  gl.useProgram(program);
  const buffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  const position = gl.getAttribLocation(program, "p");
  gl.enableVertexAttribArray(position);
  gl.vertexAttribPointer(position, 2, gl.FLOAT, false, 0, 0);
  const u = {
    res: gl.getUniformLocation(program, "res"),
    t: gl.getUniformLocation(program, "t"),
    pointer: gl.getUniformLocation(program, "pointer"),
    pointerOn: gl.getUniformLocation(program, "pointerOn"),
  };

  // A soft image needs few pixels: about half resolution on desktops, less on phones.
  const scale = coarse ? 0.35 : 0.5;
  function resize() {
    const w = Math.max(1, Math.round(canvas.clientWidth * scale));
    const h = Math.max(1, Math.round(canvas.clientHeight * scale));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
      gl.viewport(0, 0, w, h);
    }
  }

  const pointer = { x: 0.5, y: 0.5, tx: 0.5, ty: 0.5, on: 0, target: 0 };
  window.addEventListener("pointermove", e => {
    pointer.tx = e.clientX / window.innerWidth;
    pointer.ty = 1 - e.clientY / window.innerHeight;
    pointer.target = coarse ? 0 : 1;
  }, { passive: true });
  document.addEventListener("pointerleave", () => { pointer.target = 0; });

  let running = !reduceMotion;
  let stopped = false;
  let frame = 0;
  let last = 0;
  const start = performance.now();
  const seed = (Date.now() % 100000) / 1000; // a different moment of the loop on each visit

  function draw(now) {
    resize();
    pointer.x += (pointer.tx - pointer.x) * 0.06;
    pointer.y += (pointer.ty - pointer.y) * 0.06;
    pointer.on += (pointer.target - pointer.on) * 0.05;
    gl.uniform2f(u.res, canvas.width, canvas.height);
    gl.uniform1f(u.t, seed + (now - start) / 1000);
    gl.uniform2f(u.pointer, pointer.x, pointer.y);
    gl.uniform1f(u.pointerOn, pointer.on);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
  function loop(now) {
    if (!running || stopped) return;
    frame = requestAnimationFrame(loop);
    if (now - last < 33) return; // ~30 fps is plenty for slow motion
    last = now;
    draw(now);
  }

  draw(performance.now());
  canvas.classList.add("on");
  if (running) frame = requestAnimationFrame(loop);
  else window.addEventListener("resize", () => draw(performance.now()));

  document.addEventListener("visibilitychange", () => {
    if (stopped || reduceMotion) return;
    if (document.hidden) {
      running = false;
      cancelAnimationFrame(frame);
    } else if (!running) {
      running = true;
      frame = requestAnimationFrame(loop);
    }
  });

  window.TFBackdrop = {
    stop() {
      if (stopped) return;
      stopped = true;
      cancelAnimationFrame(frame);
      canvas.classList.add("off");
      const lose = gl.getExtension("WEBGL_lose_context");
      if (lose) lose.loseContext();
    },
  };
})();
