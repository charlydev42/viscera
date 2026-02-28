// ============================================================
// FLUBBER V7g — LIGHT MODE
// ============================================================
// Background: transparent (alpha=0) — composited over white UI
// Env map reflects bright environment
// Refraction transmits light (not absorbs to black)
// Reduced glow/bloom (already bright background)
// Blob stays within bounds + idle animation
// ============================================================

#define MAX_STEPS 64
#define MAX_DIST 30.0
#define SURF_DIST 0.002
#define PI 3.14159265

// ============================================================
// AUDIO
// ============================================================
float getFreq(float f) { return texture(iChannel0, vec2(f, 0.0)).x; }
float getWave(float f) { return texture(iChannel0, vec2(f, 0.75)).x; }

float subBass() { return (getFreq(0.005) + getFreq(0.01)) * 0.5; }
float bass()    { return (getFreq(0.02) + getFreq(0.04) + getFreq(0.06)) * 0.333; }
float mid()     { return (getFreq(0.15) + getFreq(0.20) + getFreq(0.25)) * 0.333; }
float hiMid()   { return (getFreq(0.30) + getFreq(0.35)) * 0.5; }
float highs()   { return (getFreq(0.45) + getFreq(0.55) + getFreq(0.65)) * 0.333; }
float energy()  { return (subBass() + bass() + mid() + highs()) * 0.25; }

float waveSample(float pos) { return getWave(pos) * 2.0 - 1.0; }

// ============================================================
// NOISE (color only)
// ============================================================
vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx) * 2.0 - 1.0;
}

float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f*f*f*(f*(f*6.0-15.0)+10.0);
    return mix(
        mix(mix(dot(hash33(i), f),
                dot(hash33(i+vec3(1,0,0)), f-vec3(1,0,0)), u.x),
            mix(dot(hash33(i+vec3(0,1,0)), f-vec3(0,1,0)),
                dot(hash33(i+vec3(1,1,0)), f-vec3(1,1,0)), u.x), u.y),
        mix(mix(dot(hash33(i+vec3(0,0,1)), f-vec3(0,0,1)),
                dot(hash33(i+vec3(1,0,1)), f-vec3(1,0,1)), u.x),
            mix(dot(hash33(i+vec3(0,1,1)), f-vec3(0,1,1)),
                dot(hash33(i+vec3(1,1,1)), f-vec3(1,1,1)), u.x), u.y),
        u.z
    );
}

vec3 cheapCurl(vec3 p) {
    float n = noise3D(p * 0.3);
    return vec3(sin(n*6.28+p.y), cos(n*6.28+p.z), sin(n*6.28+p.x)) * 0.4;
}

float flowFBM3(vec3 p, float t, float audioMod) {
    float val = 0.0;
    val += 0.500 * noise3D(p * 1.0 + cheapCurl(p)        * t * (0.4 + audioMod * 0.4));
    val += 0.250 * noise3D(p * 2.2 + cheapCurl(p + 7.3)  * t * (0.3 + audioMod * 0.3));
    val += 0.125 * noise3D(p * 4.8 + cheapCurl(p + 14.6) * t * (0.2 + audioMod * 0.2));
    return val;
}

float domainWarpFast(vec3 p, float t, float audioMod) {
    vec3 q = vec3(
        flowFBM3(p, t * 0.4, audioMod),
        flowFBM3(p + vec3(5.2, 1.3, 2.8), t * 0.4, audioMod),
        flowFBM3(p + vec3(1.7, 9.2, 4.1), t * 0.4, audioMod)
    );
    return flowFBM3(p + 3.5 * q, t * 0.25, audioMod);
}

float causticsFast(vec3 p, float t) {
    float n = noise3D(p * 3.0 + vec3(t * 0.7, t * 0.5, t * 0.3));
    float pattern = sin(p.x * 5.0 + n * 4.0 + t) * sin(p.z * 5.0 + n * 3.0 - t * 0.7);
    return pow(abs(pattern), 0.7) * 0.5 + 0.5;
}

// ============================================================
// SDF
// ============================================================
float sdEllipsoid(vec3 p, vec3 radii) {
    float k0 = length(p / radii);
    float k1 = length(p / (radii * radii));
    return k0 * (k0 - 1.0) / k1;
}

float sdSphere(vec3 p, float r) { return length(p) - r; }

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

const float S = 0.72;
const float MAX_RADIUS = 1.8;

vec3 clampPos(vec3 p, float maxR) {
    float len = length(p);
    return len > maxR ? p * (maxR / len) : p;
}

float map(vec3 p) {
    float t = iTime;
    float nrg = energy();

    float sb  = subBass() * 2.5;
    float bss = bass() * 2.5;
    float md  = mid() * 2.0;
    float hm  = hiMid() * 1.7;
    float hgs = highs() * 1.7;

    float loudness = clamp(nrg * 4.0, 0.0, 1.0);
    float extremeLoud = smoothstep(0.5, 1.0, loudness);

    // IDLE — slow breathing + wobble
    float idleBreath = sin(t * 0.4) * 0.04 + sin(t * 0.27) * 0.03;
    float idleWX = sin(t * 0.31) * 0.03 + sin(t * 0.53 + 1.0) * 0.02;
    float idleWY = cos(t * 0.37) * 0.025 + sin(t * 0.19 + 2.0) * 0.02;
    float idleWZ = sin(t * 0.43 + 3.0) * 0.02;

    vec3 coreRadii = vec3(
        1.3 + sb*0.25 + bss*0.12 + idleBreath + idleWX,
        0.65 - bss*0.06 + md*0.16 + idleBreath*0.7 + idleWY,
        1.0 + sb*0.16 + hgs*0.04 + idleBreath*0.5 + idleWZ
    ) * S;
    float core = sdEllipsoid(p, coreRadii);

    float idleDrift = 0.15;

    float b1Orbit = (idleDrift + bss * 1.0) * S;
    vec3 b1Pos = clampPos(vec3(sin(t*0.7+bss*2.0)*b1Orbit, cos(t*0.5)*b1Orbit*0.35, sin(t*0.9+1.0)*b1Orbit*0.7), MAX_RADIUS*S);
    float blob1 = sdEllipsoid(p-b1Pos, vec3(0.38+bss*0.24, 0.26+bss*0.16, 0.33+bss*0.20)*S);

    float b2Orbit = (idleDrift + md * 0.9) * S;
    vec3 b2Pos = clampPos(vec3(cos(t*0.9+2.0+md*3.0)*b2Orbit, sin(t*0.6+1.5+md*2.0)*b2Orbit*0.4, cos(t*0.7+md*1.5)*b2Orbit*0.8), MAX_RADIUS*S);
    float blob2 = sdEllipsoid(p-b2Pos, vec3(0.28+md*0.20, 0.20+md*0.13, 0.26+md*0.18)*S);

    float b3Orbit = (idleDrift*0.8 + hgs * 0.75) * S;
    vec3 b3Pos = clampPos(vec3(sin(t*2.0+4.0+hgs*5.0)*b3Orbit, cos(t*1.8+3.0+hgs*4.0)*b3Orbit*0.35, sin(t*1.5+2.0)*b3Orbit*0.85), MAX_RADIUS*S);
    float blob3 = sdEllipsoid(p-b3Pos, vec3(0.20+hgs*0.16, 0.15+hgs*0.11, 0.18+hgs*0.14)*S);

    float b4Orbit = (idleDrift + nrg * 1.2) * S;
    vec3 b4Pos = clampPos(vec3(
        cos(t*0.5+5.0)*b4Orbit + waveSample(0.2)*nrg*0.4*S,
        sin(t*1.2)*b4Orbit*0.3 + waveSample(0.5)*nrg*0.25*S,
        cos(t*0.9+1.5)*b4Orbit*0.8 + waveSample(0.8)*nrg*0.3*S
    ), MAX_RADIUS*S);
    float blob4 = sdEllipsoid(p-b4Pos, vec3(0.23+nrg*0.20, 0.16+nrg*0.12, 0.20+nrg*0.18)*S);

    float blob5=MAX_DIST, blob6=MAX_DIST, blob7=MAX_DIST, blob8=MAX_DIST;

    if (loudness > 0.35) {
        vec3 b5Pos = clampPos(vec3(sin(t*1.8+bss*4.0)*(0.4+bss*1.2)*S, waveSample(0.3)*bss*0.65*S, cos(t*2.1+bss*3.0)*(0.4+bss*1.0)*S), MAX_RADIUS*S);
        blob5 = sdSphere(p-b5Pos, (0.16+bss*0.22)*S);
        vec3 b6Pos = clampPos(vec3(waveSample(0.1)*md*1.0*S, sin(t*1.5)*(0.25+md*0.65)*S, waveSample(0.6)*md*0.8*S), MAX_RADIUS*S);
        blob6 = sdSphere(p-b6Pos, (0.14+md*0.18)*S);
    }
    if (extremeLoud > 0.15) {
        vec3 b7Pos = clampPos(vec3(sin(t*3.0+hgs*6.0)*(0.25+hgs*0.8)*S, cos(t*2.5+hgs*5.0)*(0.18+hgs*0.5)*S, waveSample(0.7)*hgs*0.65*S), MAX_RADIUS*S);
        blob7 = sdSphere(p-b7Pos, (0.11+hgs*0.15)*S);
        vec3 b8Pos = clampPos(vec3(waveSample(0.15)*nrg*1.2*S, waveSample(0.45)*nrg*0.8*S, waveSample(0.75)*nrg*1.0*S), MAX_RADIUS*S);
        blob8 = sdSphere(p-b8Pos, (0.12+nrg*0.16)*S);
    }

    float k = (0.6 + loudness * 0.9) * S;
    float d = smin(core, blob1, k);
    d = smin(d, blob2, k);
    d = smin(d, blob3, k * 0.8);
    d = smin(d, blob4, k);
    if (loudness > 0.35) { d = smin(d, blob5, k*0.7); d = smin(d, blob6, k*0.7); }
    if (extremeLoud > 0.15) { d = smin(d, blob7, k*0.65); d = smin(d, blob8, k*0.65); }

    return d;
}

vec3 calcNormal(vec3 p) {
    float e = 0.001; float d = map(p);
    return normalize(vec3(map(p+vec3(e,0,0))-d, map(p+vec3(0,e,0))-d, map(p+vec3(0,0,e))-d));
}

float rayMarch(vec3 ro, vec3 rd) {
    float d = 0.0;
    for (int i = 0; i < MAX_STEPS; i++) {
        float ds = map(ro + rd * d);
        d += ds * 0.8;
        if (ds < SURF_DIST || d > MAX_DIST) break;
    }
    return d;
}

float softShadow(vec3 ro, vec3 rd, float k) {
    float res = 1.0, t = 0.05;
    for (int i = 0; i < 12; i++) {
        float h = map(ro + rd * t);
        res = min(res, k * h / t);
        t += clamp(h, 0.05, 0.4);
        if (h < 0.002 || t > 6.0) break;
    }
    return clamp(res, 0.0, 1.0);
}

float calcAO(vec3 p, vec3 n) {
    float occ = (0.05-map(p+0.05*n)) + (0.15-map(p+0.15*n))*0.5 + (0.30-map(p+0.30*n))*0.25;
    return clamp(1.0 - 4.0*occ, 0.0, 1.0);
}

float D_GGX(vec3 N, vec3 H, float r) { float a2=r*r*r*r; float d=max(dot(N,H),0.0); d=d*d*(a2-1.0)+1.0; return a2/(PI*d*d); }
float G_Smith(vec3 N, vec3 V, vec3 L, float r) { float k=(r+1.0)*(r+1.0)/8.0; float NV=max(dot(N,V),0.0),NL=max(dot(N,L),0.0); return (NV/(NV*(1.0-k)+k))*(NL/(NL*(1.0-k)+k)); }
vec3 F_Schlick(float cos, vec3 F0) { return F0+(1.0-F0)*pow(1.0-cos,5.0); }
vec3 pbrLight(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 F0, float rough, float metal, vec3 lCol) {
    vec3 H=normalize(V+L); float NdotL=max(dot(N,L),0.0);
    if(NdotL<0.001) return vec3(0.0);
    float NDF=D_GGX(N,H,rough); float G=G_Smith(N,V,L,rough);
    vec3 F=F_Schlick(max(dot(H,V),0.0),F0);
    vec3 spec=(NDF*G*F)/(4.0*max(dot(N,V),0.0)*NdotL+0.001);
    return ((1.0-F)*(1.0-metal)*albedo/PI + spec)*lCol*NdotL;
}

// ============================================================
// ENVIRONMENT — LIGHT MODE
// Bright, airy — reflects a white/pale studio
// ============================================================
vec3 envMap(vec3 rd) {
    vec3 keyDir = normalize(vec3(-1.0, 1.0, 1.0));
    float sun = max(dot(rd, keyDir), 0.0);

    // Base: bright warm white (studio environment)
    vec3 col = vec3(0.75, 0.78, 0.82);

    // Sky dome — brighter up top
    col += vec3(0.15, 0.15, 0.18) * max(rd.y, 0.0);

    // Ground bounce — warm off-white
    col += vec3(0.10, 0.08, 0.06) * max(-rd.y, 0.0);

    // Key light — still bright but less contrasty
    col += vec3(1.0, 0.98, 0.92) * pow(sun, 128.0) * 2.0;
    col += vec3(0.8, 0.85, 0.5) * pow(sun, 24.0) * 0.4;

    // Subtle purple fill
    col += vec3(0.15, 0.10, 0.25) * pow(max(dot(rd, normalize(vec3(1,-0.5,-1))),0.0), 32.0);

    // Green horizon tint
    col += vec3(0.04, 0.08, 0.03) * pow(1.0-abs(rd.y), 6.0);

    return col;
}

// ============================================================
// FLUID COLOR — Slightly deeper for contrast on white
// ============================================================
vec3 fluidColor(vec3 p, vec3 N, vec3 V, float t) {
    float bss = bass(); float md = mid(); float hgs = highs();
    float nrg = energy(); float loud = clamp(nrg * 4.0, 0.0, 1.0);

    float warp = domainWarpFast(p * 0.5, t, nrg);
    float warp2 = flowFBM3(p * 0.3 + vec3(10.0), t * 0.35, md);
    float b1 = smoothstep(-0.4, 0.4, warp);
    float b2 = smoothstep(-0.3, 0.5, warp2);

    // Slightly more saturated rest colors for white bg contrast
    vec3 coolA = vec3(0.15, 0.42, 0.12);
    vec3 coolB = vec3(0.35, 0.65, 0.20);
    vec3 coolC = vec3(0.22, 0.52, 0.38);
    vec3 warmA = vec3(0.50, 0.72, 0.26);
    vec3 warmB = vec3(0.70, 0.80, 0.18);
    vec3 warmC = vec3(0.88, 0.75, 0.12);
    vec3 extremeA = vec3(0.95, 0.95, 0.50);

    vec3 cool = mix(coolA, mix(coolB, coolC, b2), b1);
    vec3 warm = mix(warmA, mix(warmB, warmC, b2), b1);

    float eBlend = smoothstep(0.05, 0.40, nrg + warp * 0.15);
    vec3 col = mix(cool, warm, eBlend);
    col = mix(col, extremeA, smoothstep(0.6, 0.95, loud) * 0.7);

    float caust = causticsFast(p, t);
    col += mix(vec3(0.35, 0.80, 0.25), vec3(0.90, 0.88, 0.40), eBlend) * caust * (0.05 + loud * 0.20);

    float fres = pow(1.0 - max(dot(N, V), 0.0), 2.0);
    col = mix(col, mix(vec3(0.35, 0.85, 0.50), vec3(0.85, 0.95, 0.45), eBlend), fres * 0.35);

    // Shadow tint — more visible on light bg
    float shadowZone = 1.0 - max(dot(N, normalize(vec3(1.0, 1.0, -1.0))), 0.0);
    col = mix(col, col * vec3(0.50, 0.35, 0.70) * 1.3, shadowZone * 0.15);

    float veins = pow(abs(flowFBM3(p * 2.0, t * 0.6, nrg)), 0.35);
    col = mix(col, mix(col * 1.30, col * 0.50, veins), 0.14 + loud * 0.20);

    float iri = dot(N, V) * 6.0 + t + hgs * 4.0;
    col = mix(col, vec3(0.50+sin(iri)*0.15, 0.72+sin(iri*1.3+2.0)*0.1, 0.26+sin(iri*0.7+4.0)*0.2), hgs * 0.18);

    return col;
}

vec3 fluidSSS(vec3 p, vec3 N, vec3 L, vec3 V, vec3 baseCol) {
    float nrg = energy();
    vec3 scatterDir = normalize(L + N * 0.6);
    float VdotS = pow(clamp(dot(V, -scatterDir), 0.0, 1.0), 2.5);
    float backLight = max(dot(-N, L), 0.0) * 0.4;
    float thickness = clamp(map(p - N * 0.4) * 2.5, 0.0, 1.0);
    vec3 sssCol = mix(baseCol * vec3(1.2, 1.1, 0.6), vec3(0.545, 0.765, 0.290) * 1.5, 0.3);
    return sssCol * (VdotS*(1.0-thickness) + backLight) * (0.4 + nrg * 0.4);
}

vec3 shade(vec3 p, vec3 rd, vec3 N) {
    float nrg = energy(); float bss = bass(); float hgs = highs();
    float loud = clamp(nrg * 4.0, 0.0, 1.0);
    float t = iTime; vec3 V = -rd;

    vec3 albedo = fluidColor(p, N, V, t);

    // Slightly higher base roughness on light — shows more shape definition
    float roughness = mix(0.06, 0.02, smoothstep(0.0, 0.5, nrg));
    vec3 F0 = vec3(0.06);

    // Lights — slightly softer for light mode, less contrast needed
    vec3 L1 = normalize(vec3(-1.0, 1.0, 0.8));
    vec3 lCol1 = vec3(0.92, 0.96, 0.85) * (2.5 + loud * 0.8);
    vec3 L2 = normalize(vec3(1.0, -0.5, -0.8));
    vec3 lCol2 = vec3(0.25, 0.18, 0.42) * (0.7 + loud * 0.4);
    vec3 L3 = normalize(vec3(0.3, 0.5, -1.0));
    vec3 lCol3 = vec3(0.50, 0.82, 0.30) * (1.6 + loud * 0.8);
    vec3 L4 = normalize(vec3(-0.8, 1.2, 1.0));
    vec3 lCol4 = vec3(0.95, 0.95, 0.90) * (1.2 + loud * 1.0);

    vec3 Lo = vec3(0.0);
    float shadow = softShadow(p + N*0.03, L1, 12.0);
    Lo += pbrLight(N, V, L1, albedo, F0, roughness, 0.0, lCol1) * shadow;
    Lo += pbrLight(N, V, L2, albedo, F0, roughness, 0.0, lCol2);
    Lo += pbrLight(N, V, L3, albedo, F0, roughness, 0.0, lCol3);
    Lo += pbrLight(N, V, L4, albedo, F0, roughness, 0.0, lCol4);
    Lo += fluidSSS(p, N, L1, V, albedo) * lCol1 * 0.20;

    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F_env = F0 + (vec3(1.0)-F0) * pow(1.0-NdotV, 5.0);
    Lo += envMap(R) * F_env * 1.0; // Less env reflection — already bright

    // Refraction — light transmits through gel, tinted green
    // No dark absorption — bright environment shines through
    vec3 refDir = refract(-V, N, 1.0/1.45);
    vec3 refCol = envMap(refDir) * vec3(0.85, 0.95, 0.80); // Green tint, no darkening
    Lo = mix(Lo, refCol * albedo * 1.5, (1.0-pow(NdotV,0.5)) * 0.15);

    // Specular pops — slightly reduced for light mode
    Lo += vec3(1.0,1.0,0.95) * pow(max(dot(N, normalize(V+L1)), 0.0), 512.0) * 2.0;
    Lo += vec3(0.9,1.0,0.85) * pow(max(dot(N, normalize(V+L4)), 0.0), 256.0) * 1.0;

    float caust = causticsFast(p + N*0.1, t);
    Lo += mix(vec3(0.4,0.9,0.3), vec3(0.9,0.85,0.3), bss) * pow(caust,3.0) * (0.04+hgs*0.10);

    Lo *= mix(0.85, 1.0, calcAO(p, N));

    // Glow — much reduced on light background
    vec3 glowCol = mix(vec3(0.20,0.50,0.10), vec3(0.545,0.765,0.290), smoothstep(0.2,0.5,nrg));
    Lo += glowCol * pow(nrg, 1.5) * (0.15 + loud * 0.3);

    return Lo;
}

mat3 camera(vec3 ro, vec3 ta) {
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    return mat3(cu, cross(cu, cw), cw);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float nrg = energy();
    float loud = clamp(nrg * 4.0, 0.0, 1.0);

    float ang = iTime * 0.15;
    float camDist = 3.5 - loud * 0.35;
    vec3 ro = vec3(sin(ang)*camDist, 0.7+sin(iTime*0.12)*0.3, cos(ang)*camDist);

    float shakeAmt = 0.02 + loud * 0.06;
    ro.x += noise3D(vec3(iTime*4.0)) * shakeAmt;
    ro.y += noise3D(vec3(iTime*4.0+100.0)) * shakeAmt;
    ro.z += noise3D(vec3(iTime*4.0+200.0)) * shakeAmt * 0.5;

    mat3 cam = camera(ro, vec3(0.0));
    vec3 rd = cam * normalize(vec3(uv, 1.3));

    // TRANSPARENT BACKGROUND
    float d = rayMarch(ro, rd);

    if (d < MAX_DIST) {
        vec3 hitP = ro + rd * d;
        vec3 col = shade(hitP, rd, calcNormal(hitP));

        // Tonemap — lighter curve for light mode
        col = col / (col + 0.9);
        // Less bloom on light bg
        col += max(col - 0.6, 0.0) * 0.3;

        // Subtle chromatic aberration
        float chr = length(fragCoord/iResolution.xy - 0.5) * (0.001 + loud*0.004) * 22.0;
        col.r *= 1.0 - chr * 0.4;
        col.g *= 1.0 + chr * 0.2;
        col.b *= 1.0 - chr * 0.8;

        col = pow(col, vec3(0.4545));

        fragColor = vec4(col, 1.0);
    } else {
        // Fully transparent — shows white UI behind
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
}
