// ============================================================
// Flubber WebGL Renderer
// Adapted from the JUCE OpenGL raymarched visualizer
// Mouse-interactive + click-to-distort
// ============================================================

(function () {
    'use strict';

    const canvas = document.getElementById('flubber');
    if (!canvas) return;

    const gl = canvas.getContext('webgl2', {
        alpha: true,
        premultipliedAlpha: true,
        antialias: false,
        powerPreference: 'high-performance'
    });

    if (!gl) {
        canvas.style.background = 'radial-gradient(ellipse at center, #3a5a2a 0%, transparent 70%)';
        return;
    }

    // Float texture filtering
    gl.getExtension('OES_texture_float_linear');

    // ---- Resolution scaling ----
    const MAX_DPR = 1.5;
    let width, height;

    function resize() {
        const rect = canvas.getBoundingClientRect();
        const dpr = Math.min(window.devicePixelRatio || 1, MAX_DPR);
        width = Math.floor(rect.width * dpr);
        height = Math.floor(rect.height * dpr);
        canvas.width = width;
        canvas.height = height;
        gl.viewport(0, 0, width, height);
    }

    resize();
    window.addEventListener('resize', resize);

    // ---- Mouse + click state ----
    const mouse = { x: 0.5, y: 0.5, vx: 0, vy: 0, inside: false, clicking: false };
    let prevMX = 0.5, prevMY = 0.5;

    function onMove(cx, cy) {
        const rect = canvas.getBoundingClientRect();
        mouse.x = (cx - rect.left) / rect.width;
        mouse.y = 1.0 - (cy - rect.top) / rect.height;
        mouse.inside = true;
    }

    // Mouse events — listen on the whole hero section, not just canvas
    const hero = canvas.parentElement;
    hero.addEventListener('mousemove', (e) => onMove(e.clientX, e.clientY));
    hero.addEventListener('mouseleave', () => { mouse.inside = false; });
    hero.addEventListener('mousedown', () => { mouse.clicking = true; });
    window.addEventListener('mouseup', () => { mouse.clicking = false; });

    // Touch
    hero.addEventListener('touchstart', (e) => {
        mouse.clicking = true;
        onMove(e.touches[0].clientX, e.touches[0].clientY);
    }, { passive: true });
    hero.addEventListener('touchmove', (e) => {
        onMove(e.touches[0].clientX, e.touches[0].clientY);
    }, { passive: true });
    hero.addEventListener('touchend', () => { mouse.clicking = false; mouse.inside = false; });

    // ---- Audio texture (synthetic) ----
    const TEX_W = 512;
    const audioData = new Float32Array(TEX_W * 2);

    const audioTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, audioTex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.R32F, TEX_W, 2, 0, gl.RED, gl.FLOAT, audioData);

    // Smoothed values
    let sEnergy = 0, sBass = 0, sMid = 0, sHigh = 0;
    let clickPulse = 0; // decaying pulse from clicks

    function updateAudioTexture(time) {
        // Velocity
        const vx = mouse.x - prevMX;
        const vy = mouse.y - prevMY;
        prevMX += (mouse.x - prevMX) * 0.12;
        prevMY += (mouse.y - prevMY) * 0.12;

        const speed = Math.sqrt(vx * vx + vy * vy);
        const dist = Math.sqrt((mouse.x - 0.5) ** 2 + (mouse.y - 0.5) ** 2);

        // Click pulse — spikes on click, decays
        if (mouse.clicking) {
            clickPulse = Math.min(clickPulse + 0.12, 1.0);
        } else {
            clickPulse *= 0.94;
        }

        // Target energy
        const proximity = mouse.inside ? Math.max(0, 1.0 - dist * 2.2) : 0;
        const velocity = Math.min(speed * 20, 1.0);
        const target = Math.min(proximity * 0.3 + velocity * 0.4 + clickPulse * 0.8, 1.0);

        // Smooth per band
        sEnergy += (target - sEnergy) * 0.06;
        sBass += (target * 0.9 + clickPulse * 0.5 - sBass) * 0.04;
        sMid += (target * 0.6 + clickPulse * 0.3 - sMid) * 0.06;
        sHigh += (velocity * 0.8 + clickPulse * 0.4 - sHigh) * 0.09;

        // Fill FFT row (row 0)
        for (let i = 0; i < TEX_W; i++) {
            const f = i / TEX_W;
            const bass = Math.exp(-f * 6) * sBass;
            const mid = Math.exp(-((f - 0.2) ** 2) * 40) * sMid;
            const high = f * f * sHigh * 0.5;
            const noise = (Math.sin(f * 137.1 + time * 4.7) * 0.5 + 0.5) * 0.015;
            audioData[i] = Math.max(0, bass + mid + high + noise + sEnergy * 0.015);
        }

        // Fill waveform row (row 1)
        for (let i = 0; i < TEX_W; i++) {
            const f = i / TEX_W;
            const wave = Math.sin(f * 25.1 + time * 2.5) * sEnergy * 0.3
                       + Math.sin(f * 44.0 + time * 4.1) * sHigh * 0.25
                       + Math.sin(f * 81.7 + time * 6.3) * sMid * 0.15
                       + Math.sin(f * 12.0 + time * 1.3) * clickPulse * 0.4;
            audioData[TEX_W + i] = wave * 0.5 + 0.5;
        }

        gl.bindTexture(gl.TEXTURE_2D, audioTex);
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, TEX_W, 2, gl.RED, gl.FLOAT, audioData);
    }

    // ---- Logo texture ----
    function loadLogoTex(src) {
        const tex = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, tex);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([0,0,0,0]));
        const img = new Image();
        img.onload = () => {
            gl.bindTexture(gl.TEXTURE_2D, tex);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        };
        img.src = src;
        return tex;
    }

    const darkLogoTex = loadLogoTex('assets/viscera_logo_dark.png');
    const lightLogoTex = loadLogoTex('assets/viscera_logo_light.png');
    let activeLogoTex = darkLogoTex;

    // ---- Shaders ----
    const VERT = `#version 300 es
in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
`;

    // Shared shader code — noise, SDF, PBR, blob geometry
    const SHARED = `#version 300 es
precision highp float;
out vec4 O;

uniform float iTime;
uniform vec2 iResolution;
uniform sampler2D iChannel0;
uniform sampler2D uLogo;

#define STEPS 64
#define FAR 30.0
#define EPS 0.002
#define PI 3.14159265

float gF(float f){return texture(iChannel0,vec2(f,0.0)).x;}
float gW(float f){return texture(iChannel0,vec2(f,0.75)).x;}

float subB(){return(gF(0.005)+gF(0.01))*0.5;}
float bass(){return(gF(0.02)+gF(0.04)+gF(0.06))*0.333;}
float mid(){return(gF(0.15)+gF(0.20)+gF(0.25))*0.333;}
float hiMid(){return(gF(0.30)+gF(0.35))*0.5;}
float highs(){return(gF(0.45)+gF(0.55)+gF(0.65))*0.333;}
float energy(){return(subB()+bass()+mid()+highs())*0.25;}
float wS(float p){return gW(p)*2.0-1.0;}

vec3 h33(vec3 p){p=fract(p*vec3(.1031,.103,.0973));p+=dot(p,p.yxz+33.33);return fract((p.xxy+p.yxx)*p.zyx)*2.0-1.0;}

float n3(vec3 p){
    vec3 i=floor(p),f=fract(p),u=f*f*f*(f*(f*6.0-15.0)+10.0);
    return mix(mix(mix(dot(h33(i),f),dot(h33(i+vec3(1,0,0)),f-vec3(1,0,0)),u.x),
        mix(dot(h33(i+vec3(0,1,0)),f-vec3(0,1,0)),dot(h33(i+vec3(1,1,0)),f-vec3(1,1,0)),u.x),u.y),
        mix(mix(dot(h33(i+vec3(0,0,1)),f-vec3(0,0,1)),dot(h33(i+vec3(1,0,1)),f-vec3(1,0,1)),u.x),
        mix(dot(h33(i+vec3(0,1,1)),f-vec3(0,1,1)),dot(h33(i+vec3(1,1,1)),f-vec3(1,1,1)),u.x),u.y),u.z);
}

vec3 curl(vec3 p){float n=n3(p*0.3);return vec3(sin(n*6.28+p.y),cos(n*6.28+p.z),sin(n*6.28+p.x))*0.4;}

float fbm3(vec3 p,float t,float a){
    float v=0.0;
    v+=0.500*n3(p*1.0+curl(p)*t*(0.4+a*0.4));
    v+=0.250*n3(p*2.2+curl(p+7.3)*t*(0.3+a*0.3));
    v+=0.125*n3(p*4.8+curl(p+14.6)*t*(0.2+a*0.2));
    return v;
}

float dwarp(vec3 p,float t,float a){
    vec3 q=vec3(fbm3(p,t*0.4,a),fbm3(p+vec3(5.2,1.3,2.8),t*0.4,a),fbm3(p+vec3(1.7,9.2,4.1),t*0.4,a));
    return fbm3(p+3.5*q,t*0.25,a);
}

float caust(vec3 p,float t){
    float n=n3(p*3.0+vec3(t*0.7,t*0.5,t*0.3));
    float pat=sin(p.x*5.0+n*4.0+t)*sin(p.z*5.0+n*3.0-t*0.7);
    return pow(abs(pat),0.7)*0.5+0.5;
}

float sdE(vec3 p,vec3 r){float k0=length(p/r);float k1=length(p/(r*r));return k0*(k0-1.0)/k1;}
float sdS(vec3 p,float r){return length(p)-r;}
float smin(float a,float b,float k){float h=clamp(0.5+0.5*(b-a)/k,0.0,1.0);return mix(b,a,h)-k*h*(1.0-h);}

const float SC=0.45;
const float MR=1.8;

vec3 clP(vec3 p,float m){float l=length(p);return l>m?p*(m/l):p;}

float map(vec3 p){
    float t=iTime,nrg=energy();
    float sb=subB()*2.5,bss=bass()*2.5,md=mid()*2.0,hm=hiMid()*1.7,hgs=highs()*1.7;
    float loud=clamp(nrg*4.0,0.0,1.0),exL=smoothstep(0.5,1.0,loud);

    float ib=sin(t*0.4)*0.04+sin(t*0.27)*0.03;
    float iwx=sin(t*0.31)*0.03+sin(t*0.53+1.0)*0.02;
    float iwy=cos(t*0.37)*0.025+sin(t*0.19+2.0)*0.02;
    float iwz=sin(t*0.43+3.0)*0.02;

    vec3 cr=vec3(1.3+sb*0.25+bss*0.12+ib+iwx,0.65-bss*0.06+md*0.16+ib*0.7+iwy,1.0+sb*0.16+hgs*0.04+ib*0.5+iwz)*SC;
    float core=sdE(p,cr);

    float id=0.15;
    float o1=(id+bss*1.0)*SC;
    vec3 p1=clP(vec3(sin(t*0.7+bss*2.0)*o1,cos(t*0.5)*o1*0.35,sin(t*0.9+1.0)*o1*0.7),MR*SC);
    float b1=sdE(p-p1,vec3(0.38+bss*0.24,0.26+bss*0.16,0.33+bss*0.20)*SC);

    float o2=(id+md*0.9)*SC;
    vec3 p2=clP(vec3(cos(t*0.9+2.0+md*3.0)*o2,sin(t*0.6+1.5+md*2.0)*o2*0.4,cos(t*0.7+md*1.5)*o2*0.8),MR*SC);
    float b2=sdE(p-p2,vec3(0.28+md*0.20,0.20+md*0.13,0.26+md*0.18)*SC);

    float o3=(id*0.8+hgs*0.75)*SC;
    vec3 p3=clP(vec3(sin(t*2.0+4.0+hgs*5.0)*o3,cos(t*1.8+3.0+hgs*4.0)*o3*0.35,sin(t*1.5+2.0)*o3*0.85),MR*SC);
    float b3=sdE(p-p3,vec3(0.20+hgs*0.16,0.15+hgs*0.11,0.18+hgs*0.14)*SC);

    float o4=(id+nrg*1.2)*SC;
    vec3 p4=clP(vec3(cos(t*0.5+5.0)*o4+wS(0.2)*nrg*0.4*SC,sin(t*1.2)*o4*0.3+wS(0.5)*nrg*0.25*SC,cos(t*0.9+1.5)*o4*0.8+wS(0.8)*nrg*0.3*SC),MR*SC);
    float b4=sdE(p-p4,vec3(0.23+nrg*0.20,0.16+nrg*0.12,0.20+nrg*0.18)*SC);

    float b5=FAR,b6=FAR,b7=FAR,b8=FAR;
    if(loud>0.35){
        vec3 p5=clP(vec3(sin(t*1.8+bss*4.0)*(0.4+bss*1.2)*SC,wS(0.3)*bss*0.65*SC,cos(t*2.1+bss*3.0)*(0.4+bss*1.0)*SC),MR*SC);
        b5=sdS(p-p5,(0.16+bss*0.22)*SC);
        vec3 p6=clP(vec3(wS(0.1)*md*1.0*SC,sin(t*1.5)*(0.25+md*0.65)*SC,wS(0.6)*md*0.8*SC),MR*SC);
        b6=sdS(p-p6,(0.14+md*0.18)*SC);
    }
    if(exL>0.15){
        vec3 p7=clP(vec3(sin(t*3.0+hgs*6.0)*(0.25+hgs*0.8)*SC,cos(t*2.5+hgs*5.0)*(0.18+hgs*0.5)*SC,wS(0.7)*hgs*0.65*SC),MR*SC);
        b7=sdS(p-p7,(0.11+hgs*0.15)*SC);
        vec3 p8=clP(vec3(wS(0.15)*nrg*1.2*SC,wS(0.45)*nrg*0.8*SC,wS(0.75)*nrg*1.0*SC),MR*SC);
        b8=sdS(p-p8,(0.12+nrg*0.16)*SC);
    }

    float k=(0.6+loud*0.9)*SC;
    float d=smin(core,b1,k);
    d=smin(d,b2,k);d=smin(d,b3,k*0.8);d=smin(d,b4,k);
    if(loud>0.35){d=smin(d,b5,k*0.7);d=smin(d,b6,k*0.7);}
    if(exL>0.15){d=smin(d,b7,k*0.65);d=smin(d,b8,k*0.65);}
    return d;
}

vec3 norm(vec3 p){float e=0.001,d=map(p);return normalize(vec3(map(p+vec3(e,0,0))-d,map(p+vec3(0,e,0))-d,map(p+vec3(0,0,e))-d));}

float march(vec3 ro,vec3 rd){
    float d=0.0;
    for(int i=0;i<STEPS;i++){float s=map(ro+rd*d);d+=s*0.8;if(s<EPS||d>FAR)break;}
    return d;
}

float shadow(vec3 ro,vec3 rd,float k){
    float r=1.0,t=0.05;
    for(int i=0;i<12;i++){float h=map(ro+rd*t);r=min(r,k*h/t);t+=clamp(h,0.05,0.4);if(h<0.002||t>6.0)break;}
    return clamp(r,0.0,1.0);
}

float ao(vec3 p,vec3 n){return clamp(1.0-4.0*((0.05-map(p+0.05*n))+(0.15-map(p+0.15*n))*0.5+(0.30-map(p+0.30*n))*0.25),0.0,1.0);}

float dGGX(vec3 N,vec3 H,float r){float a2=r*r*r*r;float d=max(dot(N,H),0.0);d=d*d*(a2-1.0)+1.0;return a2/(PI*d*d);}
float gSm(vec3 N,vec3 V,vec3 L,float r){float k=(r+1.0)*(r+1.0)/8.0;float nv=max(dot(N,V),0.0),nl=max(dot(N,L),0.0);return(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));}
vec3 fSch(float c,vec3 F0){return F0+(1.0-F0)*pow(1.0-c,5.0);}
vec3 pbr(vec3 N,vec3 V,vec3 L,vec3 alb,vec3 F0,float rgh,float met,vec3 lC){
    vec3 H=normalize(V+L);float nl=max(dot(N,L),0.0);
    if(nl<0.001)return vec3(0.0);
    float D=dGGX(N,H,rgh);float G=gSm(N,V,L,rgh);vec3 F=fSch(max(dot(H,V),0.0),F0);
    vec3 sp=(D*G*F)/(4.0*max(dot(N,V),0.0)*nl+0.001);
    return((1.0-F)*(1.0-met)*alb/PI+sp)*lC*nl;
}

mat3 cam(vec3 ro,vec3 ta){vec3 w=normalize(ta-ro),u=normalize(cross(w,vec3(0,1,0)));return mat3(u,cross(u,w),w);}
`;

    // ---- Dark mode tail ----
    const DARK = `
vec3 env(vec3 rd){
    vec3 kd=normalize(vec3(-1,1,1));float sun=max(dot(rd,kd),0.0);
    vec3 c=vec3(0.02,0.03,0.03);
    c+=vec3(0.05,0.12,0.08)*max(rd.y,0.0);
    c+=vec3(0.12,0.05,0.18)*pow(max(dot(rd,normalize(vec3(1,-1,-1))),0.0),3.0);
    c+=vec3(1.0,0.98,0.90)*pow(sun,256.0)*4.0;
    c+=vec3(0.8,0.9,0.5)*pow(sun,32.0)*0.6;
    c+=vec3(0.3,0.2,0.5)*pow(max(dot(rd,normalize(vec3(1,-0.5,-1))),0.0),64.0);
    c+=vec3(0.06,0.10,0.04)*pow(1.0-abs(rd.y),8.0);
    return c;
}

vec3 flCol(vec3 p,vec3 N,vec3 V,float t){
    float bss=bass(),md=mid(),hgs=highs(),nrg=energy(),loud=clamp(nrg*4.0,0.0,1.0);
    float w1=dwarp(p*0.5,t,nrg),w2=fbm3(p*0.3+vec3(10.0),t*0.35,md);
    float b1=smoothstep(-0.4,0.4,w1),b2=smoothstep(-0.3,0.5,w2);
    vec3 cool=mix(vec3(0.20,0.48,0.15),mix(vec3(0.42,0.72,0.25),vec3(0.28,0.58,0.42),b2),b1);
    vec3 warm=mix(vec3(0.545,0.765,0.290),mix(vec3(0.75,0.85,0.20),vec3(0.95,0.80,0.15),b2),b1);
    float eB=smoothstep(0.05,0.40,nrg+w1*0.15);
    vec3 c=mix(cool,warm,eB);
    c=mix(c,vec3(1.0,1.0,0.6),smoothstep(0.6,0.95,loud)*0.7);
    c+=mix(vec3(0.40,0.85,0.30),vec3(1.0,0.95,0.50),eB)*caust(p,t)*(0.06+loud*0.25);
    float fr=pow(1.0-max(dot(N,V),0.0),2.0);
    c=mix(c,mix(vec3(0.40,0.92,0.55),vec3(0.90,1.0,0.50),eB),fr*0.4);
    c=mix(c,c*vec3(0.55,0.35,0.75)*1.5,(1.0-max(dot(N,normalize(vec3(1,1,-1))),0.0))*0.10);
    float vn=pow(abs(fbm3(p*2.0,t*0.6,nrg)),0.35);
    c=mix(c,mix(c*1.35,c*0.55,vn),0.14+loud*0.22);
    float ir=dot(N,V)*6.0+t+hgs*4.0;
    c=mix(c,vec3(0.545+sin(ir)*0.15,0.765+sin(ir*1.3+2.0)*0.1,0.290+sin(ir*0.7+4.0)*0.2),hgs*0.2);
    return c;
}

vec3 sss(vec3 p,vec3 N,vec3 L,vec3 V,vec3 bc){
    float nrg=energy();
    vec3 sd=normalize(L+N*0.6);
    float vs=pow(clamp(dot(V,-sd),0.0,1.0),2.5);
    float bl=max(dot(-N,L),0.0)*0.4;
    float th=clamp(map(p-N*0.4)*2.5,0.0,1.0);
    vec3 sc=mix(bc*vec3(1.2,1.1,0.6),vec3(0.545,0.765,0.290)*1.5,0.3);
    return sc*(vs*(1.0-th)+bl)*(0.5+nrg*0.5);
}

vec3 shade(vec3 p,vec3 rd,vec3 N){
    float nrg=energy(),bss=bass(),hgs=highs(),loud=clamp(nrg*4.0,0.0,1.0);
    float t=iTime;vec3 V=-rd;
    vec3 alb=flCol(p,N,V,t);
    float rgh=mix(0.05,0.015,smoothstep(0.0,0.5,nrg));
    vec3 F0=vec3(0.06);
    vec3 L1=normalize(vec3(-1,1,0.8)),c1=vec3(0.95,1.0,0.88)*(3.0+loud*1.0);
    vec3 L2=normalize(vec3(1,-0.5,-0.8)),c2=vec3(0.30,0.20,0.50)*(0.8+loud*0.5);
    vec3 L3=normalize(vec3(0.3,0.5,-1)),c3=vec3(0.55,0.90,0.35)*(2.0+loud*1.0);
    vec3 L4=normalize(vec3(-0.8,1.2,1)),c4=vec3(1.0,1.0,0.95)*(1.5+loud*1.5);
    vec3 Lo=vec3(0.0);
    Lo+=pbr(N,V,L1,alb,F0,rgh,0.0,c1)*shadow(p+N*0.03,L1,12.0);
    Lo+=pbr(N,V,L2,alb,F0,rgh,0.0,c2);
    Lo+=pbr(N,V,L3,alb,F0,rgh,0.0,c3);
    Lo+=pbr(N,V,L4,alb,F0,rgh,0.0,c4);
    Lo+=sss(p,N,L1,V,alb)*c1*0.25;
    vec3 R=reflect(-V,N);float nv=max(dot(N,V),0.0);
    vec3 Fe=F0+(vec3(1.0)-F0)*pow(1.0-nv,5.0);
    Lo+=env(R)*Fe*1.3;
    vec3 rD=refract(-V,N,1.0/1.45);
    Lo=mix(Lo,env(rD)*exp(-vec3(0.8,0.2,1.3)*1.5)*alb*2.0,(1.0-pow(nv,0.5))*0.2);
    Lo+=vec3(1,1,0.95)*pow(max(dot(N,normalize(V+L1)),0.0),512.0)*3.0;
    Lo+=vec3(0.9,1,0.85)*pow(max(dot(N,normalize(V+L4)),0.0),256.0)*1.5;
    float ca=caust(p+N*0.1,t);
    Lo+=mix(vec3(0.4,0.9,0.3),vec3(0.9,0.85,0.3),bss)*pow(ca,3.0)*(0.05+hgs*0.12);
    Lo*=mix(0.82,1.0,ao(p,N));
    vec3 gc=mix(vec3(0.20,0.50,0.10),vec3(0.545,0.765,0.290),smoothstep(0.2,0.5,nrg));
    gc=mix(gc,vec3(1,0.95,0.5),smoothstep(0.6,0.9,loud)*0.6);
    Lo+=gc*pow(nrg,1.5)*(0.3+loud*0.8);
    return Lo;
}

void main(){
    vec2 uv=(gl_FragCoord.xy-0.5*iResolution.xy)/iResolution.y;
    float nrg=energy(),loud=clamp(nrg*4.0,0.0,1.0);
    float ang=iTime*0.15,cd=3.5-loud*0.35;
    vec3 ro=vec3(sin(ang)*cd,0.7+sin(iTime*0.12)*0.3,cos(ang)*cd);
    float sh=0.02+loud*0.06;
    ro.x+=n3(vec3(iTime*4.0))*sh;
    ro.y+=n3(vec3(iTime*4.0+100.0))*sh;
    ro.z+=n3(vec3(iTime*4.0+200.0))*sh*0.5;
    mat3 cm=cam(ro,vec3(0));
    vec3 rd=cm*normalize(vec3(uv,1.3));
    float d=march(ro,rd);
    if(d<FAR){
        vec3 hp=ro+rd*d;
        vec3 nh=norm(hp);
        vec3 col=shade(hp,rd,nh);
        // Logo overlay — distorted by blob surface
        vec3 vN=nh*cm;
        vec2 luv=gl_FragCoord.xy/iResolution.xy+vN.xy*0.12;
        vec2 sp=(luv-0.5)*vec2(iResolution.x/iResolution.y,1.0);
        float lW=0.21,lH=lW*514.0/1213.0;
        vec2 tc=sp/vec2(lW,lH)*0.5+0.5;
        tc.y=1.0-tc.y;
        vec4 lc=texture(uLogo,tc);
        float lM=step(0.0,tc.x)*step(tc.x,1.0)*step(0.0,tc.y)*step(tc.y,1.0);
        float facing=max(dot(nh,-rd),0.0);
        col=mix(col,lc.rgb,lc.a*lM*0.0*facing); // disabled — logo in HTML for now
        col=col/(col+1.0);
        col+=max(col-(0.55-loud*0.12),0.0)*(0.55+loud*0.4);
        float chr=length(gl_FragCoord.xy/iResolution.xy-0.5)*(0.001+loud*0.005)*28.0;
        col.r*=1.0-chr*0.5;col.g*=1.0+chr*0.3;col.b*=1.0-chr;
        col=pow(col,vec3(0.4545));
        float a=smoothstep(FAR,FAR*0.97,d);
        O=vec4(col*a,a);
    } else {
        O=vec4(0.0);
    }
}
`;

    // ---- Light mode tail ----
    const LIGHT = `
vec3 env(vec3 rd){
    vec3 kd=normalize(vec3(-1,1,1));float sun=max(dot(rd,kd),0.0);
    vec3 c=vec3(0.75,0.78,0.82);
    c+=vec3(0.15,0.15,0.18)*max(rd.y,0.0);
    c+=vec3(0.10,0.08,0.06)*max(-rd.y,0.0);
    c+=vec3(1.0,0.98,0.92)*pow(sun,128.0)*2.0;
    c+=vec3(0.8,0.85,0.5)*pow(sun,24.0)*0.4;
    c+=vec3(0.15,0.10,0.25)*pow(max(dot(rd,normalize(vec3(1,-0.5,-1))),0.0),32.0);
    c+=vec3(0.04,0.08,0.03)*pow(1.0-abs(rd.y),6.0);
    return c;
}

vec3 flCol(vec3 p,vec3 N,vec3 V,float t){
    float bss=bass(),md=mid(),hgs=highs(),nrg=energy(),loud=clamp(nrg*4.0,0.0,1.0);
    float w1=dwarp(p*0.5,t,nrg),w2=fbm3(p*0.3+vec3(10.0),t*0.35,md);
    float b1=smoothstep(-0.4,0.4,w1),b2=smoothstep(-0.3,0.5,w2);
    vec3 cool=mix(vec3(0.15,0.42,0.12),mix(vec3(0.35,0.65,0.20),vec3(0.22,0.52,0.38),b2),b1);
    vec3 warm=mix(vec3(0.50,0.72,0.26),mix(vec3(0.70,0.80,0.18),vec3(0.88,0.75,0.12),b2),b1);
    float eB=smoothstep(0.05,0.40,nrg+w1*0.15);
    vec3 c=mix(cool,warm,eB);
    c=mix(c,vec3(0.95,0.95,0.50),smoothstep(0.6,0.95,loud)*0.7);
    c+=mix(vec3(0.35,0.80,0.25),vec3(0.90,0.88,0.40),eB)*caust(p,t)*(0.05+loud*0.20);
    float fr=pow(1.0-max(dot(N,V),0.0),2.0);
    c=mix(c,mix(vec3(0.35,0.85,0.50),vec3(0.85,0.95,0.45),eB),fr*0.35);
    c=mix(c,c*vec3(0.50,0.35,0.70)*1.3,(1.0-max(dot(N,normalize(vec3(1,1,-1))),0.0))*0.15);
    float vn=pow(abs(fbm3(p*2.0,t*0.6,nrg)),0.35);
    c=mix(c,mix(c*1.30,c*0.50,vn),0.14+loud*0.20);
    float ir=dot(N,V)*6.0+t+hgs*4.0;
    c=mix(c,vec3(0.50+sin(ir)*0.15,0.72+sin(ir*1.3+2.0)*0.1,0.26+sin(ir*0.7+4.0)*0.2),hgs*0.18);
    return c;
}

vec3 sss(vec3 p,vec3 N,vec3 L,vec3 V,vec3 bc){
    float nrg=energy();
    vec3 sd=normalize(L+N*0.6);
    float vs=pow(clamp(dot(V,-sd),0.0,1.0),2.5);
    float bl=max(dot(-N,L),0.0)*0.4;
    float th=clamp(map(p-N*0.4)*2.5,0.0,1.0);
    vec3 sc=mix(bc*vec3(1.2,1.1,0.6),vec3(0.545,0.765,0.290)*1.5,0.3);
    return sc*(vs*(1.0-th)+bl)*(0.4+nrg*0.4);
}

vec3 shade(vec3 p,vec3 rd,vec3 N){
    float nrg=energy(),bss=bass(),hgs=highs(),loud=clamp(nrg*4.0,0.0,1.0);
    float t=iTime;vec3 V=-rd;
    vec3 alb=flCol(p,N,V,t);
    float rgh=mix(0.06,0.02,smoothstep(0.0,0.5,nrg));
    vec3 F0=vec3(0.06);
    vec3 L1=normalize(vec3(-1,1,0.8)),c1=vec3(0.92,0.96,0.85)*(2.5+loud*0.8);
    vec3 L2=normalize(vec3(1,-0.5,-0.8)),c2=vec3(0.25,0.18,0.42)*(0.7+loud*0.4);
    vec3 L3=normalize(vec3(0.3,0.5,-1)),c3=vec3(0.50,0.82,0.30)*(1.6+loud*0.8);
    vec3 L4=normalize(vec3(-0.8,1.2,1)),c4=vec3(0.95,0.95,0.90)*(1.2+loud*1.0);
    vec3 Lo=vec3(0.0);
    Lo+=pbr(N,V,L1,alb,F0,rgh,0.0,c1)*shadow(p+N*0.03,L1,12.0);
    Lo+=pbr(N,V,L2,alb,F0,rgh,0.0,c2);
    Lo+=pbr(N,V,L3,alb,F0,rgh,0.0,c3);
    Lo+=pbr(N,V,L4,alb,F0,rgh,0.0,c4);
    Lo+=sss(p,N,L1,V,alb)*c1*0.20;
    vec3 R=reflect(-V,N);float nv=max(dot(N,V),0.0);
    vec3 Fe=F0+(vec3(1.0)-F0)*pow(1.0-nv,5.0);
    Lo+=env(R)*Fe*1.0;
    vec3 rD=refract(-V,N,1.0/1.45);
    Lo=mix(Lo,env(rD)*vec3(0.85,0.95,0.80)*alb*1.5,(1.0-pow(nv,0.5))*0.15);
    Lo+=vec3(1,1,0.95)*pow(max(dot(N,normalize(V+L1)),0.0),512.0)*2.0;
    Lo+=vec3(0.9,1,0.85)*pow(max(dot(N,normalize(V+L4)),0.0),256.0)*1.0;
    float ca=caust(p+N*0.1,t);
    Lo+=mix(vec3(0.4,0.9,0.3),vec3(0.9,0.85,0.3),bss)*pow(ca,3.0)*(0.04+hgs*0.10);
    Lo*=mix(0.85,1.0,ao(p,N));
    vec3 gc=mix(vec3(0.20,0.50,0.10),vec3(0.545,0.765,0.290),smoothstep(0.2,0.5,nrg));
    Lo+=gc*pow(nrg,1.5)*(0.15+loud*0.3);
    return Lo;
}

void main(){
    vec2 uv=(gl_FragCoord.xy-0.5*iResolution.xy)/iResolution.y;
    float nrg=energy(),loud=clamp(nrg*4.0,0.0,1.0);
    float ang=iTime*0.15,cd=3.5-loud*0.35;
    vec3 ro=vec3(sin(ang)*cd,0.7+sin(iTime*0.12)*0.3,cos(ang)*cd);
    float sh=0.02+loud*0.06;
    ro.x+=n3(vec3(iTime*4.0))*sh;
    ro.y+=n3(vec3(iTime*4.0+100.0))*sh;
    ro.z+=n3(vec3(iTime*4.0+200.0))*sh*0.5;
    mat3 cm=cam(ro,vec3(0));
    vec3 rd=cm*normalize(vec3(uv,1.3));
    float d=march(ro,rd);
    if(d<FAR){
        vec3 hp=ro+rd*d;
        vec3 nh=norm(hp);
        vec3 col=shade(hp,rd,nh);
        // Logo overlay — distorted by blob surface
        vec3 vN=nh*cm;
        vec2 luv=gl_FragCoord.xy/iResolution.xy+vN.xy*0.12;
        vec2 sp=(luv-0.5)*vec2(iResolution.x/iResolution.y,1.0);
        float lW=0.21,lH=lW*514.0/1213.0;
        vec2 tc=sp/vec2(lW,lH)*0.5+0.5;
        tc.y=1.0-tc.y;
        vec4 lc=texture(uLogo,tc);
        float lM=step(0.0,tc.x)*step(tc.x,1.0)*step(0.0,tc.y)*step(tc.y,1.0);
        float facing=max(dot(nh,-rd),0.0);
        col=mix(col,lc.rgb,lc.a*lM*0.0*facing); // disabled — logo in HTML for now
        col=col/(col+0.9);
        col+=max(col-0.6,0.0)*0.3;
        float chr=length(gl_FragCoord.xy/iResolution.xy-0.5)*(0.001+loud*0.004)*22.0;
        col.r*=1.0-chr*0.4;col.g*=1.0+chr*0.2;col.b*=1.0-chr*0.8;
        col=pow(col,vec3(0.4545));
        float a=smoothstep(FAR,FAR*0.97,d);
        O=vec4(col*a,a);
    } else {
        O=vec4(0.0);
    }
}
`;

    // ---- Compile ----
    function compile(fragSrc) {
        const vs = gl.createShader(gl.VERTEX_SHADER);
        gl.shaderSource(vs, VERT);
        gl.compileShader(vs);
        if (!gl.getShaderParameter(vs, gl.COMPILE_STATUS)) {
            console.error('VS:', gl.getShaderInfoLog(vs));
            return null;
        }

        const fs = gl.createShader(gl.FRAGMENT_SHADER);
        gl.shaderSource(fs, fragSrc);
        gl.compileShader(fs);
        if (!gl.getShaderParameter(fs, gl.COMPILE_STATUS)) {
            console.error('FS:', gl.getShaderInfoLog(fs));
            return null;
        }

        const pg = gl.createProgram();
        gl.attachShader(pg, vs);
        gl.attachShader(pg, fs);
        gl.bindAttribLocation(pg, 0, 'aPos');
        gl.linkProgram(pg);
        if (!gl.getProgramParameter(pg, gl.LINK_STATUS)) {
            console.error('Link:', gl.getProgramInfoLog(pg));
            return null;
        }

        gl.deleteShader(vs);
        gl.deleteShader(fs);

        return {
            pg,
            uT: gl.getUniformLocation(pg, 'iTime'),
            uR: gl.getUniformLocation(pg, 'iResolution'),
            uC: gl.getUniformLocation(pg, 'iChannel0'),
            uL: gl.getUniformLocation(pg, 'uLogo')
        };
    }

    const darkP = compile(SHARED + DARK);
    const lightP = compile(SHARED + LIGHT);

    if (!darkP && !lightP) {
        // Both failed — show fallback gradient
        canvas.style.background = 'radial-gradient(ellipse at center, #3a5a2a 0%, transparent 70%)';
        console.error('Both shaders failed to compile');
        return;
    }

    let active = darkP || lightP;

    // ---- Fullscreen quad ----
    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);
    const vbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1,-1, 1,-1, -1,1, 1,1]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);

    // ---- Render ----
    const t0 = performance.now() / 1000;
    let raf;

    function render() {
        raf = requestAnimationFrame(render);
        const time = performance.now() / 1000 - t0;

        updateAudioTexture(time);

        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);

        gl.useProgram(active.pg);
        gl.uniform1f(active.uT, time);
        gl.uniform2f(active.uR, width, height);

        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, audioTex);
        gl.uniform1i(active.uC, 0);

        gl.activeTexture(gl.TEXTURE1);
        gl.bindTexture(gl.TEXTURE_2D, activeLogoTex);
        gl.uniform1i(active.uL, 1);

        gl.bindVertexArray(vao);
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    }

    render();

    // ---- Theme switch ----
    window.flubberSetTheme = function (theme) {
        if (theme === 'dark' && darkP) { active = darkP; activeLogoTex = darkLogoTex; }
        else if (theme === 'light' && lightP) { active = lightP; activeLogoTex = lightLogoTex; }
    };

    // ---- Pause when hidden ----
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) cancelAnimationFrame(raf);
        else render();
    });

})();
