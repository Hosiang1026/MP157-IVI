#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float wx;
    float day;
    float wx2;
    float fade;
};

layout(binding = 1) uniform sampler2D wallpaper;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p *= 2.02;
        a *= 0.5;
    }
    return v;
}

float waterLine(float x, float amount, float t) {
    float n = fbm(vec2(x * 3.4 + t * 0.07, 1.6 + t * 0.05));
    float reach = 0.035 + amount * 0.1;
    return 1.0 - reach * (0.7 + 0.5 * n);
}

vec3 waterPool(vec3 baseCol, vec2 uv, float t, float amount) {
    float edge = waterLine(uv.x, amount, t);
    float water = smoothstep(edge, edge + 0.022, uv.y);
    baseCol = mix(baseCol, baseCol * vec3(0.58, 0.66, 0.74) + vec3(0.04, 0.06, 0.08), water * (0.28 + amount * 0.32));
    float wave = fbm(vec2(uv.x * 5.5 + t * 0.45, uv.y * 14.0 - t * 0.12));
    float wave2 = fbm(vec2(uv.x * 9.0 - t * 0.7, uv.y * 6.0 + t * 0.06));
    float stream = smoothstep(0.46, 0.84, wave) * smoothstep(0.4, 0.8, wave2);
    float ripple = sin((uv.x * 32.0 + t * 2.2) + wave * 6.28) * 0.5 + 0.5;
    baseCol += vec3(0.55, 0.68, 0.82) * stream * water * (0.1 + amount * 0.18);
    baseCol += vec3(0.42, 0.56, 0.7) * ripple * water * 0.06 * amount;
    return baseCol;
}

float rainDrop(vec2 uv, float t, float count, float speed, float len, float thick, float cover, float slant, float amount) {
    float col = floor(uv.x * count);
    float n = hash(vec2(col, 1.7));
    float n2 = hash(vec2(col, 8.3));
    float x = fract(uv.x * count);
    float cx = mix(0.28, 0.72, n2);
    float headY = fract(t * speed * (0.55 + n * 0.9) + n2 * 3.0);
    float dy = headY - uv.y;
    if (dy < 0.0)
        dy += 1.0;
    float along = clamp(dy / len, 0.0, 1.0);
    float dx = abs(x - cx - slant * dy);
    float width = thick * mix(1.05, 0.06, along);
    float tail = smoothstep(len, len * 0.02, dy) * smoothstep(width, 0.0, dx);
    float head = smoothstep(thick * 1.25, thick * 0.2, length(vec2(dx, dy * count * 0.38)));
    float ground = waterLine(uv.x, amount, t);
    return max(tail, head) * step(n, cover) * step(uv.y, ground) * step(dy, len);
}

float drizzleLayer(vec2 uv, float t, float count, float speed, float len, float width, float cover, float slant) {
    float col = floor(uv.x * count);
    float n = hash(vec2(col, 1.7));
    if (n > cover)
        return 0.0;
    float n2 = hash(vec2(col, 8.3));
    float x = fract(uv.x * count);
    float cx = mix(0.12, 0.88, n2);
    float headY = fract(t * speed * (0.35 + n * 0.45) + n2 * 6.0);
    float dy = headY - uv.y;
    if (dy < 0.0)
        dy += 1.0;
    if (dy > len)
        return 0.0;
    float dx = x - cx - slant * dy;
    float core = exp(-(dx * dx) / (width * width));
    float tip = smoothstep(len, len * 0.35, dy);
    float tail = smoothstep(0.0, len * 0.25, dy);
    return core * tip * tail;
}

float rainSplash(vec2 uv, float t, float count, float speed, float cover, float amount) {
    float base = floor(uv.x * count);
    float splash = 0.0;
    float ground = waterLine(uv.x, amount, t);
    for (int i = -1; i <= 1; ++i) {
        float col = base + float(i);
        float n = hash(vec2(col, 1.7));
        float n2 = hash(vec2(col, 8.3));
        float cx = mix(0.28, 0.72, n2);
        float headY = fract(t * speed * (0.55 + n * 0.9) + n2 * 3.0);
        float age = headY - ground;
        float rad = 0.006 + age * 0.5;
        vec2 p = vec2((uv.x - (col + cx) / count) * 2.4, (uv.y - ground) * 3.6);
        float ring = smoothstep(0.014, 0.0, abs(length(p) - rad));
        splash = max(splash, ring * (1.0 - age / 0.08) * step(0.0, age) * step(age, 0.08) * step(n, cover));
    }
    return splash;
}

float snowFlakes(vec2 uv, float t, float count, float speed, float radius, float cover, float sway) {
    float col = floor(uv.x * count);
    float n = hash(vec2(col, 4.4));
    if (n > cover)
        return 0.0;
    float n2 = hash(vec2(col, 12.0));
    float head = fract(t * speed * (0.35 + n * 0.8) + n2);
    float drift = sin(t * 0.9 + n * 12.0) * sway;
    float x = (fract(uv.x * count) - mix(0.2, 0.8, n2) - drift) / count;
    float y = head - uv.y;
    if (y > 0.5)
        y -= 1.0;
    if (y < -0.5)
        y += 1.0;
    float d = length(vec2(x * 1.7, y));
    return smoothstep(radius, radius * 0.22, d);
}

float hailStones(vec2 uv, float t, float count, float speed, float cover) {
    float col = floor(uv.x * count);
    float n = hash(vec2(col, 6.6));
    if (n > cover)
        return 0.0;
    float n2 = hash(vec2(col, 2.2));
    float head = fract(t * speed * (0.8 + n * 0.6) + n2);
    float x = (fract(uv.x * count) - mix(0.25, 0.75, n2)) / count;
    float y = head - uv.y;
    if (y < 0.0)
        y += 1.0;
    float stone = smoothstep(0.011, 0.003, length(vec2(x * 1.7, y)));
    float tail = smoothstep(0.045, 0.0, y) * smoothstep(0.004, 0.0, abs(x));
    return max(stone, tail * 0.65);
}

float cloudBand(vec2 uv, float t, float top, float bottom, float speed, float scale) {
    float y = smoothstep(top, top + 0.08, uv.y) * smoothstep(bottom, bottom - 0.12, uv.y);
    float n = fbm(vec2(uv.x * scale + t * speed, uv.y * 1.6));
    return y * smoothstep(0.32, 0.66, n);
}

float cloudyStack(vec2 uv, float t, float epoch) {
    float layers = 2.0 + floor(hash(vec2(epoch, 0.3)) * 2.0);
    float c = 0.0;
    for (int i = 0; i < 5; ++i) {
        if (float(i) >= layers)
            break;
        float fi = float(i);
        float top = -0.06 + hash(vec2(epoch, fi + 0.1)) * 0.08;
        float bottom = mix(0.2, 0.36, hash(vec2(epoch, fi + 1.1)));
        float speed = mix(0.016, 0.045, hash(vec2(epoch, fi + 2.1)));
        float scale = mix(1.05, 2.35, hash(vec2(epoch, fi + 3.1)));
        float w = mix(0.32, 0.68, hash(vec2(epoch, fi + 4.1)));
        c += cloudBand(uv, t + fi * 13.0 + epoch * 2.0, top, bottom, speed, scale) * w;
    }
    return c * 0.82;
}

vec2 fogStack(vec2 uv, float t, float epoch) {
    vec2 drift = vec2(t * 0.018 + epoch * 0.04, t * 0.005);
    vec2 drift2 = vec2(-t * 0.012, t * 0.003 - epoch * 0.02);
    float global = mix(0.35, 0.8, hash(vec2(epoch, 1.7)));
    float fogClump = fbm(uv * vec2(2.4, 1.6) + drift);
    float fogClump2 = fbm(uv * vec2(4.2, 2.8) + drift2);
    float local = mix(0.3, 1.1, smoothstep(0.2, 0.76, fogClump));
    local *= mix(0.45, 1.0, smoothstep(0.25, 0.72, fogClump2));
    float n = fbm(vec2(uv.x * 1.5, uv.y * 0.85) + drift);
    float n2 = fbm(vec2(uv.x * 2.0, uv.y * 1.05) + drift2 + vec2(3.0, 1.0));
    float low = smoothstep(0.12, 0.92, uv.y);
    float mist = max((0.26 + n * 0.74) * mix(0.45, 1.0, low), (0.2 + n2 * 0.7) * mix(0.4, 0.95, low));
    mist *= global * local;
    float layers = 1.0 + floor(hash(vec2(epoch, 4.4)) * 3.0);
    float wisps = 0.0;
    for (int i = 0; i < 4; ++i) {
        if (float(i) >= layers)
            break;
        float fi = float(i);
        float top = -0.04 + hash(vec2(epoch, fi + 5.0)) * 0.08;
        float bottom = mix(0.36, 0.62, hash(vec2(epoch, fi + 6.0)));
        float speed = mix(0.004, 0.01, hash(vec2(epoch, fi + 7.0)));
        float scale = mix(1.2, 2.0, hash(vec2(epoch, fi + 8.0)));
        float w = mix(0.35, 0.9, hash(vec2(epoch, fi + 9.0)));
        wisps += cloudBand(uv, t + fi * 11.0 + epoch, top, bottom, speed, scale) * w;
    }
    wisps *= mix(0.55, 1.15, hash(vec2(epoch, 10.0)));
    return vec2(mist, wisps);
}

vec3 hazeSmog(vec3 rgb, vec2 uv, float t) {
    float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
    vec2 drift = vec2(t * 0.006, t * 0.0015);
    float fine = fbm(uv * vec2(5.0, 3.2) + drift);
    float horiz = mix(0.78, 1.0, smoothstep(0.08, 0.72, uv.y));
    float veil = horiz * (0.82 + fine * 0.12);
    vec3 smog = mix(vec3(luma), rgb, 0.55);
    smog = smog * vec3(0.88, 0.82, 0.68) + vec3(0.14, 0.11, 0.05);
    rgb = mix(rgb, smog, veil * 0.68);
    rgb = mix(vec3(luma) * 0.92 + vec3(0.06, 0.05, 0.03), rgb, 0.82);
    return rgb;
}

float lineSeg(vec2 uv, vec2 a, vec2 b, float width) {
    vec2 pa = uv - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba + 1e-6), 0.0, 1.0);
    float d = length(pa - ba * h);
    return smoothstep(width, width * 0.25, d);
}

float jagPath(vec2 uv, vec2 a, vec2 b, float seed, float width, int steps) {
    float m = 0.0;
    vec2 prev = a;
    for (int i = 1; i <= 12; ++i) {
        if (i > steps)
            break;
        float u = float(i) / float(steps);
        vec2 p = mix(a, b, u);
        p.x += (hash(vec2(seed, float(i))) - 0.5) * 0.05 * (1.0 - u);
        m = max(m, lineSeg(uv, prev, p, width * mix(1.0, 0.55, u)));
        prev = p;
    }
    return m;
}

vec2 boltTop(float seed) {
    return vec2(mix(0.1, 0.9, hash(vec2(seed, 0.1))), mix(0.03, 0.11, hash(vec2(seed, 0.2))));
}

vec2 boltBottom(float seed) {
    vec2 top = boltTop(seed);
    float x = top.x + (hash(vec2(seed, 9.9)) - 0.5) * 0.38;
    x = clamp(x, 0.06, 0.94);
    return vec2(x, mix(0.66, 0.84, hash(vec2(seed, 9.8))));
}

float trunkX(float y, float seed) {
    vec2 top = boltTop(seed);
    vec2 bot = boltBottom(seed);
    float t = clamp((y - top.y) / (bot.y - top.y), 0.0, 1.0);
    float prevU = 0.0;
    float prevX = top.x;
    for (int i = 1; i <= 12; ++i) {
        float u = float(i) / 12.0;
        vec2 p = mix(top, bot, u);
        p.x += (hash(vec2(seed, float(i))) - 0.5) * 0.05 * (1.0 - u);
        if (t <= u) {
            float k = (t - prevU) / (u - prevU + 1e-5);
            float x0 = prevX;
            float x1 = p.x;
            return mix(x0, x1, k);
        }
        prevU = u;
        prevX = p.x;
    }
    return bot.x;
}

float rootLightning(vec2 uv, float seed, float width) {
    if (uv.y < 0.04 || uv.y > 0.8)
        return 0.0;
    vec2 top = boltTop(seed);
    vec2 bot = boltBottom(seed);
    float m = jagPath(uv, top, bot, seed, width, 12);
    for (int b = 0; b < 8; ++b) {
        float fb = float(b);
        if (hash(vec2(seed, fb + 20.0)) < 0.28)
            continue;
        float y0 = mix(0.1, 0.58, hash(vec2(seed, fb + 30.0)));
        float side = hash(vec2(seed, fb + 40.0)) > 0.5 ? 1.0 : -1.0;
        vec2 p0 = vec2(trunkX(y0, seed), y0);
        vec2 p1 = vec2(p0.x + side * mix(0.06, 0.24, hash(vec2(seed, fb + 50.0))),
                       y0 + mix(0.1, 0.32, hash(vec2(seed, fb + 60.0))));
        m = max(m, jagPath(uv, p0, p1, seed + fb * 1.7, width * 0.72, 6));
        if (hash(vec2(seed, fb + 70.0)) > 0.42) {
            vec2 p2 = mix(p0, p1, 0.5 + (hash(vec2(seed, fb + 80.0)) - 0.5) * 0.2);
            vec2 p3 = vec2(p2.x + side * mix(0.04, 0.14, hash(vec2(seed, fb + 90.0))),
                           p2.y + mix(0.06, 0.16, hash(vec2(seed, fb + 100.0))));
            m = max(m, jagPath(uv, p2, p3, seed + fb * 2.3, width * 0.48, 4));
        }
    }
    return m;
}

float roundGrain(vec2 uv, vec2 g, float cutoff) {
    vec2 id = floor(uv * g);
    vec2 f = fract(uv * g) - vec2(hash(id + 1.1), hash(id + 4.4));
    f.x *= g.y / g.x;
    float h = hash(id);
    return smoothstep(0.038, 0.0, length(f)) * step(cutoff, h);
}

float sandDrift(vec2 uv, float t) {
    vec2 wind = vec2(t * 0.058, t * 0.01);
    vec2 q = uv + vec2(wind.x * 0.2, wind.y * 0.35);
    float roll = fbm(q * vec2(2.0, 1.4));
    float roll2 = fbm(q * vec2(3.8, 2.6) + vec2(1.1, 0.35));
    float roll3 = fbm(q * vec2(6.5, 4.2) - vec2(wind.x * 0.25, 0.0));
    float sheet = smoothstep(0.18, 0.52, roll) * smoothstep(0.14, 0.48, roll2);
    sheet = max(sheet, roll3 * 0.55);
    sheet = mix(0.42, 1.0, sheet);
    vec2 scroll = vec2(t * 0.028, t * 0.005);
    float dots = roundGrain(uv + scroll, vec2(28.0, 20.0), 0.22);
    dots += roundGrain(uv + scroll * 1.22 + vec2(0.04, 0.012), vec2(44.0, 30.0), 0.28) * 0.9;
    dots += roundGrain(uv + scroll * 0.88 + vec2(0.1, -0.006), vec2(62.0, 40.0), 0.34) * 0.72;
    dots += roundGrain(uv + scroll * 1.55 + vec2(0.16, 0.008), vec2(86.0, 52.0), 0.4) * 0.55;
    return clamp(sheet * 0.48 + dots * 0.58, 0.0, 1.0);
}

float snowEdgeY(float x, float depth) {
    float y = 1.0 - depth * (0.5 + 0.38 * fbm(vec2(x * 3.8, 0.7)));
    y += (fbm(vec2(x * 11.0, 1.9)) - 0.5) * depth * 0.07;
    return y;
}

vec3 snowGround(vec3 baseCol, vec2 uv, float depth) {
    float edge = snowEdgeY(uv.x, depth);
    float m = smoothstep(edge + 0.03, edge - 0.006, uv.y);
    m *= 0.7 + 0.3 * fbm(vec2(uv.x * 16.0, uv.y * 20.0));
    vec3 frost = baseCol * vec3(0.9, 0.93, 0.97) + vec3(0.05, 0.06, 0.08) * depth;
    baseCol = mix(baseCol, frost, m * (0.28 + depth * 1.1));
    float speck = roundGrain(uv, vec2(140.0, 90.0), 0.86) * m;
    baseCol += vec3(0.08, 0.09, 0.11) * speck * depth;
    return baseCol;
}

vec3 iceGround(vec3 baseCol, vec2 uv, float t, float depth) {
    float edge = snowEdgeY(uv.x, depth);
    float m = smoothstep(edge + 0.028, edge - 0.005, uv.y);
    m *= 0.72 + 0.28 * fbm(vec2(uv.x * 13.0, 1.4));
    vec3 iceCol = baseCol * vec3(0.76, 0.82, 0.9) + vec3(0.05, 0.07, 0.1);
    baseCol = mix(baseCol, iceCol, m * (0.32 + depth * 0.85));
    float glint = smoothstep(0.6, 0.86, fbm(vec2(uv.x * 10.0 + t * 0.04, edge * 8.0 + t * 0.02)));
    baseCol += vec3(0.68, 0.8, 0.94) * glint * m * 0.13;
    return baseCol;
}

vec3 windGust(vec3 rgb, vec2 uv, float t, float amt) {
    float g = fbm(vec2(uv.x * 5.5 - t * 0.32, uv.y * 3.8 + t * 0.035));
    rgb = mix(rgb, rgb * vec3(0.9, 0.92, 0.95) + vec3(0.05, 0.06, 0.07), g * amt * 0.38);
    float streak = fbm(vec2(uv.x * 12.0 - t * 0.48, uv.y * 8.5));
    rgb += vec3(0.88, 0.91, 0.94) * smoothstep(0.52, 0.8, streak) * amt * 0.14;
    return rgb;
}

vec3 frostRime(vec3 rgb, vec2 uv, float t) {
    float rim = smoothstep(0.04, 0.38, uv.y) * smoothstep(0.99, 0.62, uv.y);
    float cry = fbm(vec2(uv.x * 16.0 + t * 0.006, uv.y * 20.0));
    rgb = mix(rgb, rgb * vec3(0.86, 0.9, 0.96) + vec3(0.09, 0.11, 0.13), rim * (0.32 + cry * 0.42));
    rgb += vec3(0.94, 0.97, 1.0) * smoothstep(0.68, 0.88, cry) * rim * 0.11;
    return rgb;
}

vec3 wetSheen(vec3 rgb, vec2 uv, float t) {
    rgb = waterPool(rgb, uv, t, 0.38);
    float edge = waterLine(uv.x, 0.42, t);
    float wet = smoothstep(edge, edge + 0.045, uv.y);
    float shine = sin(uv.x * 38.0 + t * 1.25) * 0.5 + 0.5;
    rgb += vec3(0.52, 0.6, 0.7) * wet * shine * 0.09;
    return rgb;
}

vec3 dustLayer(vec3 rgb, vec2 uv, float t, float strength) {
    vec2 wind = vec2(t * 0.055, t * 0.006);
    float baseVeil = fbm(vec2(uv.x * 1.6 + wind.x, uv.y * 1.2 + wind.y * 0.5));
    baseVeil = mix(0.62, 1.0, baseVeil) * mix(0.75, 1.0, smoothstep(0.05, 0.92, uv.y));
    rgb = mix(rgb, rgb * vec3(0.58, 0.44, 0.28) + vec3(0.32, 0.22, 0.11), baseVeil * 0.78 * strength);
    float drift = sandDrift(uv, t);
    rgb += vec3(0.82, 0.62, 0.34) * drift * 0.42 * strength;
    return rgb;
}

float starField(vec2 uv, float t) {
    vec2 g = vec2(70.0, 32.0);
    vec2 id = floor(uv * g);
    vec2 f = fract(uv * g) - vec2(hash(id + 1.1), hash(id + 4.4));
    f.x *= g.y / g.x;
    float h = hash(id);
    float tw = 0.45 + 0.55 * sin(t * 2.2 + h * 40.0);
    return smoothstep(0.045, 0.0, length(f)) * tw * step(0.94, h);
}

vec3 paintSky(vec3 rgb, vec2 uv, float t, float code) {
    if (code < 0.5)
        return rgb;

    if (code < 1.5) {
        vec2 s = (uv - vec2(0.78, 0.2)) * vec2(1.7, 1.0);
        float d = length(s);
        float disc = smoothstep(0.055, 0.02, d);
        float glow = exp(-d * d * 18.0);
        float ang = atan(s.y, s.x) + t * 0.15;
        float rays = pow(max(sin(ang * 8.0), 0.0), 12.0) * smoothstep(0.42, 0.05, d);
        rgb += vec3(1.0, 0.93, 0.7) * (disc * 0.85 + glow * 0.28 + rays * 0.22);
        rgb = mix(rgb, rgb * vec3(1.05, 1.02, 0.96), 0.25);
    } else if (code < 2.5) {
        vec2 m = (uv - vec2(0.76, 0.18)) * vec2(1.7, 1.0);
        float d = length(m);
        float disc = smoothstep(0.045, 0.02, d);
        float glow = exp(-d * d * 28.0);
        rgb += vec3(0.9, 0.93, 1.0) * (disc * 0.95 + glow * 0.3);
        float sky = smoothstep(0.48, 0.02, uv.y);
        rgb += vec3(0.95, 0.97, 1.0) * starField(uv, t) * sky;
    } else if (code < 3.5) {
        float epoch = floor(t * 0.05);
        float blend = smoothstep(0.0, 0.22, fract(t * 0.05));
        float c = mix(cloudyStack(uv, t, epoch - 1.0), cloudyStack(uv, t, epoch), blend);
        c = clamp(c, 0.0, 1.0);
        rgb = mix(rgb, vec3(0.93, 0.94, 0.96), c * 0.44);
        rgb = mix(rgb, rgb * vec3(0.9, 0.91, 0.93), c * 0.1);
    } else if (code < 4.5) {
        float c = cloudBand(uv, t, -0.08, 0.34, 0.012, 1.8);
        c += cloudBand(uv, t + 40.0, 0.02, 0.42, 0.02, 2.6) * 0.65;
        rgb = mix(rgb, vec3(0.78, 0.82, 0.86), clamp(c, 0.0, 1.0) * 0.34);
    } else if (code < 5.5) {
        float epoch = floor(t * 0.045);
        float blend = smoothstep(0.0, 0.2, fract(t * 0.045));
        vec2 fog = mix(fogStack(uv, t, epoch - 1.0), fogStack(uv, t, epoch), blend);
        rgb = mix(rgb, vec3(0.93, 0.94, 0.96), fog.x * 0.5);
        rgb = mix(rgb, vec3(0.9, 0.92, 0.95), clamp(fog.y, 0.0, 1.0) * 0.42);
        float floorMist = smoothstep(0.42, 0.96, uv.y) * fog.x;
        rgb = mix(rgb, vec3(0.96, 0.97, 0.99), floorMist * 0.38);
    } else if (code < 9.5) {
        float amount = code < 6.5 ? 0.28 : (code < 7.5 ? 0.58 : 0.95);
        float r = 0.0;
        float splash = 0.0;
        if (code < 6.5) {
            r = drizzleLayer(uv, t, 72.0, 0.55, 0.028, 0.011, 0.62, 0.012);
            r += drizzleLayer(uv + vec2(0.013, 0.0), t, 96.0, 0.42, 0.022, 0.008, 0.48, 0.018) * 0.85;
            r += drizzleLayer(uv + vec2(0.007, 0.0), t, 58.0, 0.68, 0.034, 0.014, 0.38, 0.008) * 0.55;
            splash = rainSplash(uv, t, 72.0, 0.55, 0.62, amount) * 0.35;
        } else if (code < 7.5) {
            r = rainDrop(uv, t, 52.0, 1.05, 0.062, 0.16, 0.62, 2.6, amount);
            r += rainDrop(uv + vec2(0.017, 0.0), t, 40.0, 1.35, 0.085, 0.13, 0.45, 3.2, amount) * 0.75;
            splash = rainSplash(uv, t, 52.0, 1.05, 0.62, amount);
            splash += rainSplash(uv + vec2(0.017, 0.0), t, 40.0, 1.35, 0.45, amount) * 0.7;
        } else {
            r = rainDrop(uv, t, 78.0, 1.45, 0.09, 0.13, 0.82, 3.4, amount);
            r += rainDrop(uv + vec2(0.01, 0.0), t, 56.0, 1.85, 0.12, 0.11, 0.7, 4.0, amount);
            splash = rainSplash(uv, t, 78.0, 1.45, 0.82, amount);
            splash += rainSplash(uv + vec2(0.01, 0.0), t, 56.0, 1.85, 0.7, amount);
        }
        r = clamp(r, 0.0, 1.0);
        rgb += vec3(0.82, 0.88, 0.95) * r * (code < 6.5 ? 0.38 : (0.5 + amount * 0.45));
        float haze = cloudBand(uv, t, -0.06, 0.26, 0.028, 2.2);
        haze += cloudBand(uv, t + 16.0, 0.0, 0.34, 0.016, 1.5) * 0.65;
        rgb = mix(rgb, vec3(0.9, 0.92, 0.95), clamp(haze, 0.0, 1.0) * (0.12 + amount * 0.14));
        rgb = waterPool(rgb, uv, t, amount);
        rgb += vec3(0.86, 0.91, 0.96) * clamp(splash, 0.0, 1.0) * (0.45 + amount * 0.55);
        if (code > 8.5) {
            float cycle = fract(t * 0.11);
            float flash = smoothstep(0.0, 0.015, cycle) * smoothstep(0.07, 0.02, cycle);
            float seed = floor(t * 3.0);
            float bolt = rootLightning(uv, seed, 0.0032);
            bolt += rootLightning(uv, seed + 3.1, 0.0024) * 0.55;
            bolt *= smoothstep(0.08, 0.0, cycle);
            rgb += vec3(0.85, 0.92, 1.0) * bolt;
            rgb += vec3(0.75, 0.82, 0.95) * flash * 0.5;
        }
    } else if (code < 13.5) {
        float depth = code < 10.5 ? 0.04 : (code < 11.5 ? 0.075 : (code < 12.5 ? 0.13 : 0.0));
        float snowTop = snowEdgeY(uv.x, depth);
        float s = 0.0;
        if (code < 10.5) {
            s = snowFlakes(uv, t, 16.0, 0.18, 0.016, 0.55, 0.08);
            s += snowFlakes(uv + vec2(0.2, 0.0), t, 24.0, 0.28, 0.009, 0.35, 0.1) * 0.8;
        } else if (code < 11.5) {
            s = snowFlakes(uv, t, 28.0, 0.26, 0.014, 0.6, 0.07);
            s += snowFlakes(uv + vec2(0.1, 0.0), t, 40.0, 0.38, 0.008, 0.5, 0.09);
        } else if (code < 12.5) {
            s = snowFlakes(uv, t, 36.0, 0.34, 0.013, 0.7, 0.05);
            s += snowFlakes(uv + vec2(0.08, 0.0), t, 58.0, 0.5, 0.007, 0.62, 0.07);
            s += snowFlakes(uv + vec2(0.3, 0.0), t, 80.0, 0.7, 0.005, 0.45, 0.04);
        } else {
            s = hailStones(uv, t, 48.0, 1.6, 0.55);
            s += hailStones(uv + vec2(0.03, 0.0), t, 72.0, 2.1, 0.4);
        }
        s *= code > 12.5 ? 1.0 : smoothstep(snowTop, snowTop - 0.035, uv.y);
        vec3 flake = code > 12.5 ? vec3(0.92, 0.95, 0.98) : vec3(1.0);
        rgb += flake * clamp(s, 0.0, 1.0) * (code > 12.5 ? 0.95 : 0.8);
        if (depth > 0.0)
            rgb = snowGround(rgb, uv, depth);
    } else if (code < 14.5) {
        rgb = hazeSmog(rgb, uv, t);
    } else if (code < 15.5) {
        rgb = mix(rgb, vec3(0.76, 0.58, 0.34), 0.18);
        rgb = dustLayer(rgb, uv, t, 1.0);
    } else if (code < 16.5) {
        float r = drizzleLayer(uv, t, 58.0, 0.62, 0.032, 0.01, 0.55, 0.01);
        float s = snowFlakes(uv, t, 22.0, 0.22, 0.012, 0.5, 0.07);
        s += snowFlakes(uv + vec2(0.15, 0.0), t, 32.0, 0.3, 0.009, 0.38, 0.09) * 0.75;
        rgb += vec3(0.85, 0.88, 0.94) * r * 0.32;
        rgb += vec3(1.0) * clamp(s, 0.0, 1.0) * 0.55;
        rgb = waterPool(rgb, uv, t, 0.22);
    } else if (code < 17.5) {
        float r = drizzleLayer(uv, t, 64.0, 0.5, 0.028, 0.01, 0.5, 0.012);
        r += drizzleLayer(uv + vec2(0.011, 0.0), t, 46.0, 0.62, 0.022, 0.008, 0.38, 0.015) * 0.65;
        rgb += vec3(0.78, 0.86, 0.96) * r * 0.36;
        rgb = mix(rgb, rgb * vec3(0.93, 0.95, 0.98) + vec3(0.01, 0.02, 0.04), 0.07);
        rgb = iceGround(rgb, uv, t, 0.07);
    } else if (code < 18.5) {
        float c = cloudBand(uv, t, -0.04, 0.38, 0.028, 1.6);
        rgb = mix(rgb, vec3(0.86, 0.88, 0.9), c * 0.22);
        rgb = windGust(rgb, uv, t, 0.95);
    } else if (code < 19.5) {
        float s = snowFlakes(uv, t, 42.0, 0.55, 0.011, 0.72, 0.04);
        s += snowFlakes(uv + vec2(0.12, 0.0), t, 68.0, 0.78, 0.008, 0.58, 0.05);
        rgb += vec3(1.0) * clamp(s, 0.0, 1.0) * 0.9;
        rgb = snowGround(rgb, uv, 0.11);
        float epoch = floor(t * 0.05);
        float blend = smoothstep(0.0, 0.2, fract(t * 0.05));
        vec2 fog = mix(fogStack(uv, t, epoch - 1.0), fogStack(uv, t, epoch), blend);
        rgb = mix(rgb, vec3(0.92, 0.93, 0.95), fog.x * 0.35);
        rgb = windGust(rgb, uv, t, 1.15);
    } else if (code < 20.5) {
        rgb = frostRime(rgb, uv, t);
    } else if (code < 21.5) {
        rgb = mix(rgb, vec3(0.84, 0.74, 0.58), 0.1);
        rgb = dustLayer(rgb, uv, t, 0.48);
    } else if (code < 22.5) {
        float amount = 0.95;
        float r = rainDrop(uv, t, 78.0, 1.45, 0.09, 0.13, 0.82, 3.4, amount);
        r += rainDrop(uv + vec2(0.01, 0.0), t, 56.0, 1.85, 0.12, 0.11, 0.7, 4.0, amount);
        rgb += vec3(0.78, 0.86, 0.95) * clamp(r, 0.0, 1.0) * 0.55;
        rgb = waterPool(rgb, uv, t, 0.92);
    } else if (code < 23.5) {
        float c = cloudBand(uv, t, -0.02, 0.32, 0.012, 1.4);
        rgb = mix(rgb, vec3(0.88, 0.9, 0.92), c * 0.12);
        rgb = wetSheen(rgb, uv, t);
    } else if (code < 24.5) {
        rgb = mix(rgb, rgb * vec3(0.68, 0.7, 0.74), 0.28);
        float amount = 0.88;
        float r = rainDrop(uv, t, 72.0, 1.55, 0.1, 0.14, 0.78, 4.2, amount);
        r += rainDrop(uv + vec2(0.015, 0.0), t, 52.0, 1.95, 0.13, 0.12, 0.62, 5.0, amount);
        rgb += vec3(0.75, 0.84, 0.94) * clamp(r, 0.0, 1.0) * 0.58;
        rgb = waterPool(rgb, uv, t, 0.55);
        rgb = windGust(rgb, uv, t, 1.25);
    }
    return rgb;
}

void main() {
    vec2 uv = qt_TexCoord0;
    vec3 base = texture(wallpaper, uv).rgb;
    vec3 fromSky = paintSky(base, uv, time, wx2);
    vec3 toSky = paintSky(base, uv, time, wx);
    vec3 rgb = mix(fromSky, toSky, clamp(fade, 0.0, 1.0));
    rgb += vec3(qt_Matrix[0][0] + day + wx2) * 0.0;
    fragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0) * qt_Opacity;
}
