// FlubberVisualizer.cpp — OpenGL raymarched audio-reactive blob
#include "FlubberVisualizer.h"
#include "VisceraLookAndFeel.h"
#include <cmath>
#include <iostream>

using namespace juce::gl;

// ============================================================
// Vertex shader — fullscreen quad
// ============================================================
static const char* vertexShaderSrc = R"VERT(#version 150
in vec2 aPosition;
void main() { gl_Position = vec4(aPosition, 0.0, 1.0); }
)VERT";

// ============================================================
// Fragment shader — LIGHT MODE (v7g)
// Transparent background, bright env, deeper blob colors
// ============================================================
static const char* fragmentShaderLight = R"FRAG(#version 150
uniform float iTime;
uniform vec3  iResolution;
uniform sampler2D iChannel0;
uniform vec3 uBgColor;
out vec4 fragColorOut;

#define MAX_STEPS 96
#define MAX_DIST 30.0
#define SURF_DIST 0.0008
#define PI 3.14159265

float getFreq(float f) { return texture(iChannel0, vec2(f, 0.0)).x; }
float getWave(float f) { return texture(iChannel0, vec2(f, 0.75)).x; }
float subBass() { return (getFreq(0.005) + getFreq(0.01)) * 0.5; }
float bass()    { return (getFreq(0.02) + getFreq(0.04) + getFreq(0.06)) * 0.333; }
float mid()     { return (getFreq(0.15) + getFreq(0.20) + getFreq(0.25)) * 0.333; }
float hiMid()   { return (getFreq(0.30) + getFreq(0.35)) * 0.5; }
float highs()   { return (getFreq(0.45) + getFreq(0.55) + getFreq(0.65)) * 0.333; }
float energy()  { return (subBass() + bass() + mid() + highs()) * 0.25; }
float waveSample(float pos) { return getWave(pos) * 2.0 - 1.0; }

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx) * 2.0 - 1.0;
}
float noise3D(vec3 p) {
    vec3 i = floor(p); vec3 f = fract(p);
    vec3 u = f*f*f*(f*(f*6.0-15.0)+10.0);
    return mix(mix(mix(dot(hash33(i),f),dot(hash33(i+vec3(1,0,0)),f-vec3(1,0,0)),u.x),mix(dot(hash33(i+vec3(0,1,0)),f-vec3(0,1,0)),dot(hash33(i+vec3(1,1,0)),f-vec3(1,1,0)),u.x),u.y),mix(mix(dot(hash33(i+vec3(0,0,1)),f-vec3(0,0,1)),dot(hash33(i+vec3(1,0,1)),f-vec3(1,0,1)),u.x),mix(dot(hash33(i+vec3(0,1,1)),f-vec3(0,1,1)),dot(hash33(i+vec3(1,1,1)),f-vec3(1,1,1)),u.x),u.y),u.z);
}
vec3 cheapCurl(vec3 p) { float n=noise3D(p*0.3); return vec3(sin(n*6.28+p.y),cos(n*6.28+p.z),sin(n*6.28+p.x))*0.4; }
float flowFBM3(vec3 p, float t, float am) {
    return 0.500*noise3D(p*1.0+cheapCurl(p)*t*(0.4+am*0.4))
         + 0.250*noise3D(p*2.2+cheapCurl(p+7.3)*t*(0.3+am*0.3))
         + 0.125*noise3D(p*4.8+cheapCurl(p+14.6)*t*(0.2+am*0.2));
}
float domainWarpFast(vec3 p, float t, float am) {
    vec3 q=vec3(flowFBM3(p,t*0.4,am),flowFBM3(p+vec3(5.2,1.3,2.8),t*0.4,am),flowFBM3(p+vec3(1.7,9.2,4.1),t*0.4,am));
    return flowFBM3(p+3.5*q,t*0.25,am);
}
float causticsFast(vec3 p, float t) {
    float n=noise3D(p*3.0+vec3(t*0.7,t*0.5,t*0.3));
    float pattern=sin(p.x*5.0+n*4.0+t)*sin(p.z*5.0+n*3.0-t*0.7);
    return pow(abs(pattern),0.7)*0.5+0.5;
}

float sdEllipsoid(vec3 p, vec3 r) { float k0=length(p/r); float k1=length(p/(r*r)); return k0*(k0-1.0)/k1; }
float sdSphere(vec3 p, float r) { return length(p)-r; }
float smin(float a, float b, float k) { float h=clamp(0.5+0.5*(b-a)/k,0.0,1.0); return mix(b,a,h)-k*h*(1.0-h); }

const float S = 0.55;
const float MAX_RADIUS = 1.4;
vec3 clampPos(vec3 p, float maxR) { float l=length(p); return l>maxR ? p*(maxR/l) : p; }

float map(vec3 p) {
    float t=iTime; float nrg=energy();
    float sb=subBass()*2.5, bss=bass()*2.5, md=mid()*2.0, hm=hiMid()*1.7, hgs=highs()*1.7;
    float loudness=clamp(nrg*4.0,0.0,1.0); float extremeLoud=smoothstep(0.5,1.0,loudness);
    float idleB=sin(t*0.4)*0.18+sin(t*0.27)*0.14+sin(t*0.17+0.5)*0.10+cos(t*0.09+3.0)*0.08;
    float idleX=sin(t*0.31)*0.16+sin(t*0.53+1.0)*0.12+cos(t*0.13+2.0)*0.09+sin(t*0.07+5.0)*0.07;
    float idleY=cos(t*0.37)*0.15+sin(t*0.19+2.0)*0.11+sin(t*0.11+4.0)*0.08+cos(t*0.06+1.0)*0.06;
    float idleZ=sin(t*0.43+3.0)*0.13+cos(t*0.23+1.5)*0.10+sin(t*0.08+6.0)*0.07;
    vec3 coreR=vec3(1.1+sb*0.25+bss*0.12+idleB+idleX, 0.95-bss*0.06+md*0.16+idleB*0.7+idleY, 1.0+sb*0.16+hgs*0.04+idleB*0.5+idleZ)*S;
    float core=sdEllipsoid(p,coreR);
    // Lava lamp surface noise — organic bumps that slowly crawl
    float surfNoise=noise3D(p*2.0+vec3(t*0.2,t*0.15,t*0.18))*0.08
                   +noise3D(p*1.0+vec3(t*0.1,t*0.08,t*0.12))*0.12;
    core-=surfNoise*S;
    float idD=0.40;
    float b1O=(idD+bss*1.0)*S;
    vec3 b1P=clampPos(vec3(sin(t*0.7+bss*2.0)*b1O,cos(t*0.5)*b1O*0.35,sin(t*0.9+1.0)*b1O*0.7),MAX_RADIUS*S);
    float blob1=sdEllipsoid(p-b1P,vec3(0.55+bss*0.28,0.42+bss*0.20,0.48+bss*0.24)*S);
    float b2O=(idD+md*0.9)*S;
    vec3 b2P=clampPos(vec3(cos(t*0.9+2.0+md*3.0)*b2O,sin(t*0.6+1.5+md*2.0)*b2O*0.4,cos(t*0.7+md*1.5)*b2O*0.8),MAX_RADIUS*S);
    float blob2=sdEllipsoid(p-b2P,vec3(0.45+md*0.24,0.36+md*0.16,0.40+md*0.20)*S);
    float b3O=(idD*0.8+hgs*0.75)*S;
    vec3 b3P=clampPos(vec3(sin(t*2.0+4.0+hgs*5.0)*b3O,cos(t*1.8+3.0+hgs*4.0)*b3O*0.35,sin(t*1.5+2.0)*b3O*0.85),MAX_RADIUS*S);
    float blob3=sdEllipsoid(p-b3P,vec3(0.38+hgs*0.20,0.30+hgs*0.14,0.34+hgs*0.18)*S);
    float b4O=(idD+nrg*1.2)*S;
    vec3 b4P=clampPos(vec3(cos(t*0.5+5.0)*b4O+waveSample(0.2)*nrg*0.4*S,sin(t*1.2)*b4O*0.3+waveSample(0.5)*nrg*0.25*S,cos(t*0.9+1.5)*b4O*0.8+waveSample(0.8)*nrg*0.3*S),MAX_RADIUS*S);
    float blob4=sdEllipsoid(p-b4P,vec3(0.40+nrg*0.24,0.32+nrg*0.16,0.36+nrg*0.20)*S);
    float blob5=MAX_DIST,blob6=MAX_DIST,blob7=MAX_DIST,blob8=MAX_DIST;
    if(loudness>0.35){
        vec3 b5P=clampPos(vec3(sin(t*1.8+bss*4.0)*(0.4+bss*1.2)*S,waveSample(0.3)*bss*0.65*S,cos(t*2.1+bss*3.0)*(0.4+bss*1.0)*S),MAX_RADIUS*S);
        blob5=sdSphere(p-b5P,(0.16+bss*0.22)*S);
        vec3 b6P=clampPos(vec3(waveSample(0.1)*md*1.0*S,sin(t*1.5)*(0.25+md*0.65)*S,waveSample(0.6)*md*0.8*S),MAX_RADIUS*S);
        blob6=sdSphere(p-b6P,(0.14+md*0.18)*S);
    }
    if(extremeLoud>0.15){
        vec3 b7P=clampPos(vec3(sin(t*3.0+hgs*6.0)*(0.25+hgs*0.8)*S,cos(t*2.5+hgs*5.0)*(0.18+hgs*0.5)*S,waveSample(0.7)*hgs*0.65*S),MAX_RADIUS*S);
        blob7=sdSphere(p-b7P,(0.11+hgs*0.15)*S);
        vec3 b8P=clampPos(vec3(waveSample(0.15)*nrg*1.2*S,waveSample(0.45)*nrg*0.8*S,waveSample(0.75)*nrg*1.0*S),MAX_RADIUS*S);
        blob8=sdSphere(p-b8P,(0.12+nrg*0.16)*S);
    }
    float k=(0.30+loudness*0.5)*S;
    float d=smin(core,blob1,k); d=smin(d,blob2,k); d=smin(d,blob3,k*0.8); d=smin(d,blob4,k);
    if(loudness>0.35){d=smin(d,blob5,k*0.7);d=smin(d,blob6,k*0.7);}
    if(extremeLoud>0.15){d=smin(d,blob7,k*0.65);d=smin(d,blob8,k*0.65);}
    return d;
}

vec3 calcNormal(vec3 p){float e=0.001;float d=map(p);return normalize(vec3(map(p+vec3(e,0,0))-d,map(p+vec3(0,e,0))-d,map(p+vec3(0,0,e))-d));}
vec2 rayMarch(vec3 ro,vec3 rd){float d=0.0;float minD=1e10;for(int i=0;i<MAX_STEPS;i++){float ds=map(ro+rd*d);minD=min(minD,ds);d+=ds*0.8;if(ds<SURF_DIST||d>MAX_DIST)break;}return vec2(d,minD);}
float softShadow(vec3 ro,vec3 rd,float k){float res=1.0,t=0.05;for(int i=0;i<12;i++){float h=map(ro+rd*t);res=min(res,k*h/t);t+=clamp(h,0.05,0.4);if(h<0.002||t>6.0)break;}return clamp(res,0.0,1.0);}
float calcAO(vec3 p,vec3 n){return clamp(1.0-4.0*((0.05-map(p+0.05*n))+(0.15-map(p+0.15*n))*0.5+(0.30-map(p+0.30*n))*0.25),0.0,1.0);}

float D_GGX(vec3 N,vec3 H,float r){float a2=r*r*r*r;float d=max(dot(N,H),0.0);d=d*d*(a2-1.0)+1.0;return a2/(PI*d*d);}
float G_Smith(vec3 N,vec3 V,vec3 L,float r){float k=(r+1.0)*(r+1.0)/8.0;float NV=max(dot(N,V),0.0),NL=max(dot(N,L),0.0);return(NV/(NV*(1.0-k)+k))*(NL/(NL*(1.0-k)+k));}
vec3 F_Schlick(float ct,vec3 F0){return F0+(1.0-F0)*pow(1.0-ct,5.0);}
vec3 pbrLight(vec3 N,vec3 V,vec3 L,vec3 albedo,vec3 F0,float rough,float metal,vec3 lCol){
    vec3 H=normalize(V+L);float NdotL=max(dot(N,L),0.0);if(NdotL<0.001)return vec3(0.0);
    float NDF=D_GGX(N,H,rough);float G=G_Smith(N,V,L,rough);vec3 F=F_Schlick(max(dot(H,V),0.0),F0);
    vec3 spec=(NDF*G*F)/(4.0*max(dot(N,V),0.0)*NdotL+0.001);
    return((1.0-F)*(1.0-metal)*albedo/PI+spec)*lCol*NdotL;
}

vec3 envMap(vec3 rd){
    vec3 keyDir=normalize(vec3(-1.0,1.0,1.0));float sun=max(dot(rd,keyDir),0.0);
    vec3 col=vec3(0.88,0.90,0.94);
    col+=vec3(0.20,0.20,0.24)*max(rd.y,0.0);
    col+=vec3(0.14,0.12,0.10)*max(-rd.y,0.0);
    col+=vec3(1.0,0.98,0.92)*pow(sun,128.0)*3.0;
    col+=vec3(0.88,0.92,0.58)*pow(sun,24.0)*0.6;
    col+=vec3(0.15,0.10,0.25)*pow(max(dot(rd,normalize(vec3(1,-0.5,-1))),0.0),32.0);
    col+=vec3(0.04,0.08,0.03)*pow(1.0-abs(rd.y),6.0);
    return col;
}

vec3 fluidColor(vec3 p,vec3 N,vec3 V,float t){
    float bss=bass();float md=mid();float hgs=highs();float nrg=energy();float loud=clamp(nrg*4.0,0.0,1.0);
    float warp=domainWarpFast(p*0.5,t,nrg);float warp2=flowFBM3(p*0.3+vec3(10.0),t*0.35,md);
    float b1=smoothstep(-0.4,0.4,warp);float b2=smoothstep(-0.3,0.5,warp2);
    vec3 coolA=vec3(0.42,0.68,0.35),coolB=vec3(0.58,0.84,0.42),coolC=vec3(0.48,0.74,0.55);
    vec3 warmA=vec3(0.70,0.88,0.45),warmB=vec3(0.84,0.92,0.38),warmC=vec3(0.95,0.87,0.30);
    vec3 extremeA=vec3(0.98,0.98,0.60);
    vec3 cool=mix(coolA,mix(coolB,coolC,b2),b1);vec3 warm=mix(warmA,mix(warmB,warmC,b2),b1);
    float eBlend=smoothstep(0.02,0.35,nrg+warp*0.15);
    vec3 col=mix(cool,warm,eBlend);col=mix(col,extremeA,smoothstep(0.6,0.95,loud)*0.7);
    float caust=causticsFast(p,t);col+=mix(vec3(0.45,0.88,0.35),vec3(0.95,0.92,0.50),eBlend)*caust*(0.07+loud*0.22);
    float fres=pow(1.0-max(dot(N,V),0.0),2.0);col=mix(col,mix(vec3(0.52,0.92,0.60),vec3(0.92,0.98,0.55),eBlend),fres*0.35);
    float shadowZone=1.0-max(dot(N,normalize(vec3(1.0,1.0,-1.0))),0.0);col=mix(col,col*vec3(0.60,0.45,0.78)*1.2,shadowZone*0.10);
    float veins=pow(abs(flowFBM3(p*2.0,t*0.6,nrg)),0.35);col=mix(col,mix(col*1.30,col*0.55,veins),0.12+loud*0.18);
    float iri=dot(N,V)*6.0+t+hgs*4.0;col=mix(col,vec3(0.55+sin(iri)*0.15,0.78+sin(iri*1.3+2.0)*0.1,0.32+sin(iri*0.7+4.0)*0.2),hgs*0.18);
    return col;
}

vec3 fluidSSS(vec3 p,vec3 N,vec3 L,vec3 V,vec3 baseCol){
    float nrg=energy();vec3 sd=normalize(L+N*0.6);float VdS=pow(clamp(dot(V,-sd),0.0,1.0),2.5);
    float bl=max(dot(-N,L),0.0)*0.4;float th=clamp(map(p-N*0.4)*2.5,0.0,1.0);
    vec3 sc=mix(baseCol*vec3(1.2,1.1,0.6),vec3(0.545,0.765,0.290)*1.5,0.3);
    return sc*(VdS*(1.0-th)+bl)*(0.4+nrg*0.4);
}

vec3 shade(vec3 p,vec3 rd,vec3 N){
    float nrg=energy();float bss=bass();float hgs=highs();float loud=clamp(nrg*4.0,0.0,1.0);
    float t=iTime;vec3 V=-rd;vec3 albedo=fluidColor(p,N,V,t);
    float roughness=mix(0.06,0.02,smoothstep(0.0,0.5,nrg));vec3 F0=vec3(0.06);
    vec3 L1=normalize(vec3(-1,1,0.8)),lC1=vec3(0.92,0.96,0.85)*(2.5+loud*0.8);
    vec3 L2=normalize(vec3(1,-0.5,-0.8)),lC2=vec3(0.25,0.18,0.42)*(0.7+loud*0.4);
    vec3 L3=normalize(vec3(0.3,0.5,-1)),lC3=vec3(0.50,0.82,0.30)*(1.6+loud*0.8);
    vec3 L4=normalize(vec3(-0.8,1.2,1)),lC4=vec3(0.95,0.95,0.90)*(1.2+loud*1.0);
    vec3 Lo=vec3(0);float shadow=softShadow(p+N*0.03,L1,12.0);
    Lo+=pbrLight(N,V,L1,albedo,F0,roughness,0.0,lC1)*shadow;
    Lo+=pbrLight(N,V,L2,albedo,F0,roughness,0.0,lC2);
    Lo+=pbrLight(N,V,L3,albedo,F0,roughness,0.0,lC3);
    Lo+=pbrLight(N,V,L4,albedo,F0,roughness,0.0,lC4);
    Lo+=fluidSSS(p,N,L1,V,albedo)*lC1*0.20;
    vec3 R=reflect(-V,N);float NdV=max(dot(N,V),0.0);
    vec3 Fe=F0+(vec3(1)-F0)*pow(1.0-NdV,5.0);Lo+=envMap(R)*Fe*1.0;
    vec3 refDir=refract(-V,N,1.0/1.45);vec3 refCol=envMap(refDir)*vec3(0.85,0.95,0.80);
    Lo=mix(Lo,refCol*albedo*1.5,(1.0-pow(NdV,0.5))*0.15);
    Lo+=vec3(1,1,0.95)*pow(max(dot(N,normalize(V+L1)),0.0),512.0)*2.0;
    Lo+=vec3(0.9,1,0.85)*pow(max(dot(N,normalize(V+L4)),0.0),256.0)*1.0;
    float caust=causticsFast(p+N*0.1,t);Lo+=mix(vec3(0.4,0.9,0.3),vec3(0.9,0.85,0.3),bss)*pow(caust,3.0)*(0.04+hgs*0.10);
    Lo*=mix(0.85,1.0,calcAO(p,N));
    vec3 glowCol=mix(vec3(0.20,0.50,0.10),vec3(0.545,0.765,0.290),smoothstep(0.2,0.5,nrg));
    Lo+=glowCol*pow(nrg,1.5)*(0.15+loud*0.3);
    return Lo;
}

mat3 camera(vec3 ro,vec3 ta){vec3 cw=normalize(ta-ro);vec3 cu=normalize(cross(cw,vec3(0,1,0)));return mat3(cu,cross(cu,cw),cw);}

void mainImage(out vec4 fragColor,in vec2 fragCoord){
    // Elliptical mask: clip to oval inscribed in viewport
    vec2 ndc=(fragCoord/iResolution.xy)*2.0-1.0; // -1..1
    float ovalDist=length(ndc); // circle in normalized coords (aspect handled by viewport)
    float ovalFw=fwidth(ovalDist);
    float ovalMask=1.0-smoothstep(0.92-ovalFw,0.92+ovalFw,ovalDist);
    if(ovalMask<0.001){ fragColor=vec4(uBgColor,1.0); return; }

    vec2 uv=(fragCoord-0.5*iResolution.xy)/iResolution.y;

    // X-Y axes with tick marks
    float lineW=1.2/iResolution.y;
    vec3 axisCol=mix(uBgColor,vec3(0.5),0.16);
    vec3 tickCol=mix(uBgColor,vec3(0.5),0.10);
    float axX=1.0-smoothstep(0.0,lineW,abs(uv.x));
    float axY=1.0-smoothstep(0.0,lineW,abs(uv.y));
    // Tick marks every 0.1 units along each axis
    float tickSp=0.1;
    float tickLen=8.0/iResolution.y;
    float tickW=1.0/iResolution.y;
    // Ticks on X axis (short vertical dashes along y=0)
    float onTickX=1.0-smoothstep(0.0,tickW,abs(mod(uv.x+tickSp*0.5,tickSp)-tickSp*0.5));
    float nearAxisY=1.0-smoothstep(0.0,tickLen,abs(uv.y));
    float ticksX=onTickX*nearAxisY;
    // Ticks on Y axis (short horizontal dashes along x=0)
    float onTickY=1.0-smoothstep(0.0,tickW,abs(mod(uv.y+tickSp*0.5,tickSp)-tickSp*0.5));
    float nearAxisX=1.0-smoothstep(0.0,tickLen,abs(uv.x));
    float ticksY=onTickY*nearAxisX;
    vec3 bg=uBgColor;
    bg=mix(bg,tickCol,max(ticksX,ticksY));
    bg=mix(bg,axisCol,max(axX,axY));

    float nrg=energy();float loud=clamp(nrg*4.0,0.0,1.0);
    float ang=iTime*0.15;float camDist=3.5-loud*0.35;
    vec3 ro=vec3(sin(ang)*camDist,0.7+sin(iTime*0.12)*0.3,cos(ang)*camDist);
    float sh=0.02+loud*0.06;
    ro.x+=noise3D(vec3(iTime*4.0))*sh;ro.y+=noise3D(vec3(iTime*4.0+100.0))*sh;ro.z+=noise3D(vec3(iTime*4.0+200.0))*sh*0.5;
    mat3 cam=camera(ro,vec3(0));vec3 rd=cam*normalize(vec3(uv,1.3));
    vec2 rm=rayMarch(ro,rd);float d=rm.x;
    vec3 col=bg;
    if(d<MAX_DIST){
        vec3 hitP=ro+rd*d;vec3 N=calcNormal(hitP);
        vec3 sc=shade(hitP,rd,N);
        sc=sc/(sc+0.9);sc+=max(sc-0.6,0.0)*0.3;
        float chr=length(fragCoord/iResolution.xy-0.5)*(0.001+loud*0.004)*22.0;
        sc.r*=1.0-chr*0.4;sc.g*=1.0+chr*0.2;sc.b*=1.0-chr*0.8;
        sc=pow(sc,vec3(0.4545));
        // Fresnel silhouette fade — softens edges naturally
        float NdV=max(dot(N,-rd),0.0);
        float silhouette=smoothstep(0.0,0.1,NdV);
        // fwidth edge AA
        float fw=fwidth(d);
        float edgeAA=1.0-smoothstep(0.0,max(fw*1.0,0.015),rm.y);
        col=mix(bg,sc,silhouette*edgeAA);
    }
    col=mix(uBgColor,col,ovalMask);
    fragColor=vec4(col,1.0);
}
void main(){mainImage(fragColorOut,gl_FragCoord.xy);}
)FRAG";

// ============================================================
// Fragment shader — DARK MODE (v7g)
// Transparent background, dark env, rich reflections
// ============================================================
static const char* fragmentShaderDark = R"FRAG(#version 150
uniform float iTime;
uniform vec3  iResolution;
uniform sampler2D iChannel0;
uniform vec3 uBgColor;
out vec4 fragColorOut;

#define MAX_STEPS 128
#define MAX_DIST 30.0
#define SURF_DIST 0.0005
#define PI 3.14159265

float getFreq(float f) { return texture(iChannel0, vec2(f, 0.0)).x; }
float getWave(float f) { return texture(iChannel0, vec2(f, 0.75)).x; }
float subBass() { return (getFreq(0.005) + getFreq(0.01)) * 0.5; }
float bass()    { return (getFreq(0.02) + getFreq(0.04) + getFreq(0.06)) * 0.333; }
float mid()     { return (getFreq(0.15) + getFreq(0.20) + getFreq(0.25)) * 0.333; }
float hiMid()   { return (getFreq(0.30) + getFreq(0.35)) * 0.5; }
float highs()   { return (getFreq(0.45) + getFreq(0.55) + getFreq(0.65)) * 0.333; }
float energy()  { return (subBass() + bass() + mid() + highs()) * 0.25; }
float waveSample(float pos) { return getWave(pos) * 2.0 - 1.0; }

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx) * 2.0 - 1.0;
}
float noise3D(vec3 p) {
    vec3 i = floor(p); vec3 f = fract(p);
    vec3 u = f*f*f*(f*(f*6.0-15.0)+10.0);
    return mix(mix(mix(dot(hash33(i),f),dot(hash33(i+vec3(1,0,0)),f-vec3(1,0,0)),u.x),mix(dot(hash33(i+vec3(0,1,0)),f-vec3(0,1,0)),dot(hash33(i+vec3(1,1,0)),f-vec3(1,1,0)),u.x),u.y),mix(mix(dot(hash33(i+vec3(0,0,1)),f-vec3(0,0,1)),dot(hash33(i+vec3(1,0,1)),f-vec3(1,0,1)),u.x),mix(dot(hash33(i+vec3(0,1,1)),f-vec3(0,1,1)),dot(hash33(i+vec3(1,1,1)),f-vec3(1,1,1)),u.x),u.y),u.z);
}
vec3 cheapCurl(vec3 p) { float n=noise3D(p*0.3); return vec3(sin(n*6.28+p.y),cos(n*6.28+p.z),sin(n*6.28+p.x))*0.4; }
float flowFBM3(vec3 p, float t, float am) {
    return 0.500*noise3D(p*1.0+cheapCurl(p)*t*(0.4+am*0.4))
         + 0.250*noise3D(p*2.2+cheapCurl(p+7.3)*t*(0.3+am*0.3))
         + 0.125*noise3D(p*4.8+cheapCurl(p+14.6)*t*(0.2+am*0.2));
}
float domainWarpFast(vec3 p, float t, float am) {
    vec3 q=vec3(flowFBM3(p,t*0.4,am),flowFBM3(p+vec3(5.2,1.3,2.8),t*0.4,am),flowFBM3(p+vec3(1.7,9.2,4.1),t*0.4,am));
    return flowFBM3(p+3.5*q,t*0.25,am);
}
float causticsFast(vec3 p, float t) {
    float n=noise3D(p*3.0+vec3(t*0.7,t*0.5,t*0.3));
    float pattern=sin(p.x*5.0+n*4.0+t)*sin(p.z*5.0+n*3.0-t*0.7);
    return pow(abs(pattern),0.7)*0.5+0.5;
}

float sdEllipsoid(vec3 p, vec3 r) { float k0=length(p/r); float k1=length(p/(r*r)); return k0*(k0-1.0)/k1; }
float sdSphere(vec3 p, float r) { return length(p)-r; }
float smin(float a, float b, float k) { float h=clamp(0.5+0.5*(b-a)/k,0.0,1.0); return mix(b,a,h)-k*h*(1.0-h); }

const float S = 0.55;
const float MAX_RADIUS = 1.4;
vec3 clampPos(vec3 p, float maxR) { float l=length(p); return l>maxR ? p*(maxR/l) : p; }

float map(vec3 p) {
    float t=iTime; float nrg=energy();
    float sb=subBass()*2.5, bss=bass()*2.5, md=mid()*2.0, hm=hiMid()*1.7, hgs=highs()*1.7;
    float loudness=clamp(nrg*4.0,0.0,1.0); float extremeLoud=smoothstep(0.5,1.0,loudness);
    float idleB=sin(t*0.4)*0.18+sin(t*0.27)*0.14+sin(t*0.17+0.5)*0.10+cos(t*0.09+3.0)*0.08;
    float idleX=sin(t*0.31)*0.16+sin(t*0.53+1.0)*0.12+cos(t*0.13+2.0)*0.09+sin(t*0.07+5.0)*0.07;
    float idleY=cos(t*0.37)*0.15+sin(t*0.19+2.0)*0.11+sin(t*0.11+4.0)*0.08+cos(t*0.06+1.0)*0.06;
    float idleZ=sin(t*0.43+3.0)*0.13+cos(t*0.23+1.5)*0.10+sin(t*0.08+6.0)*0.07;
    vec3 coreR=vec3(1.1+sb*0.25+bss*0.12+idleB+idleX, 0.95-bss*0.06+md*0.16+idleB*0.7+idleY, 1.0+sb*0.16+hgs*0.04+idleB*0.5+idleZ)*S;
    float core=sdEllipsoid(p,coreR);
    // Lava lamp surface noise — organic bumps that slowly crawl
    float surfNoise=noise3D(p*2.0+vec3(t*0.2,t*0.15,t*0.18))*0.08
                   +noise3D(p*1.0+vec3(t*0.1,t*0.08,t*0.12))*0.12;
    core-=surfNoise*S;
    float idD=0.40;
    float b1O=(idD+bss*1.0)*S;
    vec3 b1P=clampPos(vec3(sin(t*0.7+bss*2.0)*b1O,cos(t*0.5)*b1O*0.35,sin(t*0.9+1.0)*b1O*0.7),MAX_RADIUS*S);
    float blob1=sdEllipsoid(p-b1P,vec3(0.55+bss*0.28,0.42+bss*0.20,0.48+bss*0.24)*S);
    float b2O=(idD+md*0.9)*S;
    vec3 b2P=clampPos(vec3(cos(t*0.9+2.0+md*3.0)*b2O,sin(t*0.6+1.5+md*2.0)*b2O*0.4,cos(t*0.7+md*1.5)*b2O*0.8),MAX_RADIUS*S);
    float blob2=sdEllipsoid(p-b2P,vec3(0.45+md*0.24,0.36+md*0.16,0.40+md*0.20)*S);
    float b3O=(idD*0.8+hgs*0.75)*S;
    vec3 b3P=clampPos(vec3(sin(t*2.0+4.0+hgs*5.0)*b3O,cos(t*1.8+3.0+hgs*4.0)*b3O*0.35,sin(t*1.5+2.0)*b3O*0.85),MAX_RADIUS*S);
    float blob3=sdEllipsoid(p-b3P,vec3(0.38+hgs*0.20,0.30+hgs*0.14,0.34+hgs*0.18)*S);
    float b4O=(idD+nrg*1.2)*S;
    vec3 b4P=clampPos(vec3(cos(t*0.5+5.0)*b4O+waveSample(0.2)*nrg*0.4*S,sin(t*1.2)*b4O*0.3+waveSample(0.5)*nrg*0.25*S,cos(t*0.9+1.5)*b4O*0.8+waveSample(0.8)*nrg*0.3*S),MAX_RADIUS*S);
    float blob4=sdEllipsoid(p-b4P,vec3(0.40+nrg*0.24,0.32+nrg*0.16,0.36+nrg*0.20)*S);
    float blob5=MAX_DIST,blob6=MAX_DIST,blob7=MAX_DIST,blob8=MAX_DIST;
    if(loudness>0.35){
        vec3 b5P=clampPos(vec3(sin(t*1.8+bss*4.0)*(0.4+bss*1.2)*S,waveSample(0.3)*bss*0.65*S,cos(t*2.1+bss*3.0)*(0.4+bss*1.0)*S),MAX_RADIUS*S);
        blob5=sdSphere(p-b5P,(0.16+bss*0.22)*S);
        vec3 b6P=clampPos(vec3(waveSample(0.1)*md*1.0*S,sin(t*1.5)*(0.25+md*0.65)*S,waveSample(0.6)*md*0.8*S),MAX_RADIUS*S);
        blob6=sdSphere(p-b6P,(0.14+md*0.18)*S);
    }
    if(extremeLoud>0.15){
        vec3 b7P=clampPos(vec3(sin(t*3.0+hgs*6.0)*(0.25+hgs*0.8)*S,cos(t*2.5+hgs*5.0)*(0.18+hgs*0.5)*S,waveSample(0.7)*hgs*0.65*S),MAX_RADIUS*S);
        blob7=sdSphere(p-b7P,(0.11+hgs*0.15)*S);
        vec3 b8P=clampPos(vec3(waveSample(0.15)*nrg*1.2*S,waveSample(0.45)*nrg*0.8*S,waveSample(0.75)*nrg*1.0*S),MAX_RADIUS*S);
        blob8=sdSphere(p-b8P,(0.12+nrg*0.16)*S);
    }
    float k=(0.30+loudness*0.5)*S;
    float d=smin(core,blob1,k); d=smin(d,blob2,k); d=smin(d,blob3,k*0.8); d=smin(d,blob4,k);
    if(loudness>0.35){d=smin(d,blob5,k*0.7);d=smin(d,blob6,k*0.7);}
    if(extremeLoud>0.15){d=smin(d,blob7,k*0.65);d=smin(d,blob8,k*0.65);}
    return d;
}

vec3 calcNormal(vec3 p){float e=0.001;float d=map(p);return normalize(vec3(map(p+vec3(e,0,0))-d,map(p+vec3(0,e,0))-d,map(p+vec3(0,0,e))-d));}
vec2 rayMarch(vec3 ro,vec3 rd){float d=0.0;float minD=1e10;for(int i=0;i<MAX_STEPS;i++){float ds=map(ro+rd*d);minD=min(minD,ds);d+=ds*0.8;if(ds<SURF_DIST||d>MAX_DIST)break;}return vec2(d,minD);}
float softShadow(vec3 ro,vec3 rd,float k){float res=1.0,t=0.05;for(int i=0;i<12;i++){float h=map(ro+rd*t);res=min(res,k*h/t);t+=clamp(h,0.05,0.4);if(h<0.002||t>6.0)break;}return clamp(res,0.0,1.0);}
float calcAO(vec3 p,vec3 n){return clamp(1.0-4.0*((0.05-map(p+0.05*n))+(0.15-map(p+0.15*n))*0.5+(0.30-map(p+0.30*n))*0.25),0.0,1.0);}

float D_GGX(vec3 N,vec3 H,float r){float a2=r*r*r*r;float d=max(dot(N,H),0.0);d=d*d*(a2-1.0)+1.0;return a2/(PI*d*d);}
float G_Smith(vec3 N,vec3 V,vec3 L,float r){float k=(r+1.0)*(r+1.0)/8.0;float NV=max(dot(N,V),0.0),NL=max(dot(N,L),0.0);return(NV/(NV*(1.0-k)+k))*(NL/(NL*(1.0-k)+k));}
vec3 F_Schlick(float ct,vec3 F0){return F0+(1.0-F0)*pow(1.0-ct,5.0);}
vec3 pbrLight(vec3 N,vec3 V,vec3 L,vec3 albedo,vec3 F0,float rough,float metal,vec3 lCol){
    vec3 H=normalize(V+L);float NdotL=max(dot(N,L),0.0);if(NdotL<0.001)return vec3(0.0);
    float NDF=D_GGX(N,H,rough);float G=G_Smith(N,V,L,rough);vec3 F=F_Schlick(max(dot(H,V),0.0),F0);
    vec3 spec=(NDF*G*F)/(4.0*max(dot(N,V),0.0)*NdotL+0.001);
    return((1.0-F)*(1.0-metal)*albedo/PI+spec)*lCol*NdotL;
}

vec3 envMap(vec3 rd){
    vec3 keyDir=normalize(vec3(-1.0,1.0,1.0));float sun=max(dot(rd,keyDir),0.0);
    vec3 col=vec3(0.06,0.07,0.08);
    col+=vec3(0.10,0.18,0.14)*max(rd.y,0.0);
    col+=vec3(0.15,0.08,0.22)*pow(max(dot(rd,normalize(vec3(1,-1,-1))),0.0),3.0);
    col+=vec3(1.0,0.98,0.90)*pow(sun,256.0)*5.0;
    col+=vec3(0.85,0.92,0.55)*pow(sun,32.0)*0.8;
    col+=vec3(0.35,0.25,0.55)*pow(max(dot(rd,normalize(vec3(1,-0.5,-1))),0.0),64.0);
    col+=vec3(0.08,0.14,0.06)*pow(1.0-abs(rd.y),8.0);
    return col;
}

vec3 fluidColor(vec3 p,vec3 N,vec3 V,float t){
    float bss=bass();float md=mid();float hgs=highs();float nrg=energy();float loud=clamp(nrg*4.0,0.0,1.0);
    float warp=domainWarpFast(p*0.5,t,nrg);float warp2=flowFBM3(p*0.3+vec3(10.0),t*0.35,md);
    float b1=smoothstep(-0.4,0.4,warp);float b2=smoothstep(-0.3,0.5,warp2);
    vec3 coolA=vec3(0.35,0.60,0.28),coolB=vec3(0.52,0.80,0.35),coolC=vec3(0.40,0.68,0.50);
    vec3 warmA=vec3(0.62,0.82,0.38),warmB=vec3(0.82,0.90,0.30),warmC=vec3(0.98,0.86,0.22);
    vec3 extremeA=vec3(1.0,1.0,0.65);
    vec3 cool=mix(coolA,mix(coolB,coolC,b2),b1);vec3 warm=mix(warmA,mix(warmB,warmC,b2),b1);
    float eBlend=smoothstep(0.05,0.40,nrg+warp*0.15);
    vec3 col=mix(cool,warm,eBlend);col=mix(col,extremeA,smoothstep(0.6,0.95,loud)*0.7);
    float caust=causticsFast(p,t);col+=mix(vec3(0.40,0.85,0.30),vec3(1.0,0.95,0.50),eBlend)*caust*(0.06+loud*0.25);
    float fres=pow(1.0-max(dot(N,V),0.0),2.0);col=mix(col,mix(vec3(0.40,0.92,0.55),vec3(0.90,1.0,0.50),eBlend),fres*0.4);
    float shadowZone=1.0-max(dot(N,normalize(vec3(1.0,1.0,-1.0))),0.0);col=mix(col,col*vec3(0.55,0.35,0.75)*1.5,shadowZone*0.10);
    float veins=pow(abs(flowFBM3(p*2.0,t*0.6,nrg)),0.35);col=mix(col,mix(col*1.35,col*0.55,veins),0.14+loud*0.22);
    float iri=dot(N,V)*6.0+t+hgs*4.0;col=mix(col,vec3(0.545+sin(iri)*0.15,0.765+sin(iri*1.3+2.0)*0.1,0.290+sin(iri*0.7+4.0)*0.2),hgs*0.2);
    return col;
}

vec3 fluidSSS(vec3 p,vec3 N,vec3 L,vec3 V,vec3 baseCol){
    float nrg=energy();vec3 sd=normalize(L+N*0.6);float VdS=pow(clamp(dot(V,-sd),0.0,1.0),2.5);
    float bl=max(dot(-N,L),0.0)*0.4;float th=clamp(map(p-N*0.4)*2.5,0.0,1.0);
    vec3 sc=mix(baseCol*vec3(1.2,1.1,0.6),vec3(0.545,0.765,0.290)*1.5,0.3);
    return sc*(VdS*(1.0-th)+bl)*(0.5+nrg*0.5);
}

vec3 shade(vec3 p,vec3 rd,vec3 N){
    float nrg=energy();float bss=bass();float hgs=highs();float loud=clamp(nrg*4.0,0.0,1.0);
    float t=iTime;vec3 V=-rd;vec3 albedo=fluidColor(p,N,V,t);
    float roughness=mix(0.05,0.015,smoothstep(0.0,0.5,nrg));vec3 F0=vec3(0.06);
    vec3 L1=normalize(vec3(-1,1,0.8)),lC1=vec3(0.95,1.0,0.88)*(3.0+loud*1.0);
    vec3 L2=normalize(vec3(1,-0.5,-0.8)),lC2=vec3(0.30,0.20,0.50)*(0.8+loud*0.5);
    vec3 L3=normalize(vec3(0.3,0.5,-1)),lC3=vec3(0.55,0.90,0.35)*(2.0+loud*1.0);
    vec3 L4=normalize(vec3(-0.8,1.2,1)),lC4=vec3(1.0,1.0,0.95)*(1.5+loud*1.5);
    vec3 Lo=vec3(0);float shadow=softShadow(p+N*0.03,L1,12.0);
    Lo+=pbrLight(N,V,L1,albedo,F0,roughness,0.0,lC1)*shadow;
    Lo+=pbrLight(N,V,L2,albedo,F0,roughness,0.0,lC2);
    Lo+=pbrLight(N,V,L3,albedo,F0,roughness,0.0,lC3);
    Lo+=pbrLight(N,V,L4,albedo,F0,roughness,0.0,lC4);
    Lo+=fluidSSS(p,N,L1,V,albedo)*lC1*0.25;
    vec3 R=reflect(-V,N);float NdV=max(dot(N,V),0.0);
    vec3 Fe=F0+(vec3(1)-F0)*pow(1.0-NdV,5.0);Lo+=envMap(R)*Fe*1.3;
    vec3 refDir=refract(-V,N,1.0/1.45);vec3 refCol=envMap(refDir)*exp(-vec3(0.8,0.2,1.3)*1.5);
    Lo=mix(Lo,refCol*albedo*2.0,(1.0-pow(NdV,0.5))*0.2);
    Lo+=vec3(1,1,0.95)*pow(max(dot(N,normalize(V+L1)),0.0),512.0)*3.0;
    Lo+=vec3(0.9,1,0.85)*pow(max(dot(N,normalize(V+L4)),0.0),256.0)*1.5;
    float caust=causticsFast(p+N*0.1,t);Lo+=mix(vec3(0.4,0.9,0.3),vec3(0.9,0.85,0.3),bss)*pow(caust,3.0)*(0.05+hgs*0.12);
    Lo*=mix(0.82,1.0,calcAO(p,N));
    vec3 glowCol=mix(vec3(0.20,0.50,0.10),vec3(0.545,0.765,0.290),smoothstep(0.2,0.5,nrg));
    glowCol=mix(glowCol,vec3(1.0,0.95,0.5),smoothstep(0.6,0.9,loud)*0.6);
    Lo+=glowCol*pow(nrg,1.5)*(0.3+loud*0.8);
    return Lo;
}

mat3 camera(vec3 ro,vec3 ta){vec3 cw=normalize(ta-ro);vec3 cu=normalize(cross(cw,vec3(0,1,0)));return mat3(cu,cross(cu,cw),cw);}

void mainImage(out vec4 fragColor,in vec2 fragCoord){
    // Elliptical mask: clip to oval inscribed in viewport
    vec2 ndc=(fragCoord/iResolution.xy)*2.0-1.0;
    float ovalDist=length(ndc);
    float ovalFw=fwidth(ovalDist);
    float ovalMask=1.0-smoothstep(0.92-ovalFw,0.92+ovalFw,ovalDist);
    if(ovalMask<0.001){ fragColor=vec4(uBgColor,1.0); return; }

    vec2 uv=(fragCoord-0.5*iResolution.xy)/iResolution.y;

    // X-Y axes with tick marks
    float lineW=1.2/iResolution.y;
    vec3 axisCol=mix(uBgColor,vec3(0.5),0.16);
    vec3 tickCol=mix(uBgColor,vec3(0.5),0.10);
    float axX=1.0-smoothstep(0.0,lineW,abs(uv.x));
    float axY=1.0-smoothstep(0.0,lineW,abs(uv.y));
    // Tick marks every 0.1 units along each axis
    float tickSp=0.1;
    float tickLen=8.0/iResolution.y;
    float tickW=1.0/iResolution.y;
    // Ticks on X axis (short vertical dashes along y=0)
    float onTickX=1.0-smoothstep(0.0,tickW,abs(mod(uv.x+tickSp*0.5,tickSp)-tickSp*0.5));
    float nearAxisY=1.0-smoothstep(0.0,tickLen,abs(uv.y));
    float ticksX=onTickX*nearAxisY;
    // Ticks on Y axis (short horizontal dashes along x=0)
    float onTickY=1.0-smoothstep(0.0,tickW,abs(mod(uv.y+tickSp*0.5,tickSp)-tickSp*0.5));
    float nearAxisX=1.0-smoothstep(0.0,tickLen,abs(uv.x));
    float ticksY=onTickY*nearAxisX;
    vec3 bg=uBgColor;
    bg=mix(bg,tickCol,max(ticksX,ticksY));
    bg=mix(bg,axisCol,max(axX,axY));

    float nrg=energy();float loud=clamp(nrg*4.0,0.0,1.0);
    float ang=iTime*0.15;float camDist=3.5-loud*0.35;
    vec3 ro=vec3(sin(ang)*camDist,0.7+sin(iTime*0.12)*0.3,cos(ang)*camDist);
    float sh=0.02+loud*0.06;
    ro.x+=noise3D(vec3(iTime*4.0))*sh;ro.y+=noise3D(vec3(iTime*4.0+100.0))*sh;ro.z+=noise3D(vec3(iTime*4.0+200.0))*sh*0.5;
    mat3 cam=camera(ro,vec3(0));vec3 rd=cam*normalize(vec3(uv,1.3));
    vec2 rm=rayMarch(ro,rd);float d=rm.x;
    vec3 col=bg;
    if(d<MAX_DIST){
        vec3 hitP=ro+rd*d;vec3 N=calcNormal(hitP);
        vec3 sc=shade(hitP,rd,N);
        sc=sc/(sc+1.0);sc+=max(sc-(0.55-loud*0.12),0.0)*(0.55+loud*0.4);
        float chr=length(fragCoord/iResolution.xy-0.5)*(0.001+loud*0.005)*28.0;
        sc.r*=1.0-chr*0.5;sc.g*=1.0+chr*0.3;sc.b*=1.0-chr;
        sc=pow(sc,vec3(0.4545));
        // Fresnel silhouette fade
        float NdV=max(dot(N,-rd),0.0);
        float silhouette=smoothstep(0.0,0.1,NdV);
        float edgeFade=smoothstep(MAX_DIST,MAX_DIST*0.85,d);
        float fw=fwidth(d);
        float edgeAA=1.0-smoothstep(0.0,max(fw*1.5,0.025),rm.y);
        col=mix(bg,sc,silhouette*edgeAA*edgeFade);
    }
    col=mix(uBgColor,col,ovalMask);
    fragColor=vec4(col,1.0);
}
void main(){mainImage(fragColorOut,gl_FragCoord.xy);}
)FRAG";

// ============================================================
// C++ implementation
// ============================================================

FlubberVisualizer::FlubberVisualizer(bb::AudioVisualBuffer& bufL,
                                     bb::AudioVisualBuffer& bufR)
    : audioL(bufL), audioR(bufR)
{
    smoothedFFT.fill(0.0f);

    glContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    glContext.setRenderer(this);
    glContext.setContinuousRepainting(true);
    glContext.setComponentPaintingEnabled(false);
    glContext.attachTo(*this);
}

FlubberVisualizer::~FlubberVisualizer()
{
    glContext.detach();
}

void FlubberVisualizer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
}

bool FlubberVisualizer::compileShader(ShaderSet& ss, const char* fragSrc)
{
    ss.program = std::make_unique<juce::OpenGLShaderProgram>(glContext);

    if (!ss.program->addVertexShader(vertexShaderSrc))
    {
        std::cerr << "[Flubber] Vertex shader error: " << ss.program->getLastError() << std::endl;
        ss.program.reset();
        return false;
    }
    if (!ss.program->addFragmentShader(fragSrc))
    {
        std::cerr << "[Flubber] Fragment shader error: " << ss.program->getLastError() << std::endl;
        ss.program.reset();
        return false;
    }
    if (!ss.program->link())
    {
        std::cerr << "[Flubber] Shader link error: " << ss.program->getLastError() << std::endl;
        ss.program.reset();
        return false;
    }

    auto pid = ss.program->getProgramID();
    ss.uTime       = glGetUniformLocation(pid, "iTime");
    ss.uResolution = glGetUniformLocation(pid, "iResolution");
    ss.uChannel0   = glGetUniformLocation(pid, "iChannel0");
    ss.uBgColor    = glGetUniformLocation(pid, "uBgColor");
    return true;
}

void FlubberVisualizer::newOpenGLContextCreated()
{
    if (!compileShader(lightShader, fragmentShaderLight))
        std::cerr << "[Flubber] Light shader FAILED" << std::endl;
    if (!compileShader(darkShader, fragmentShaderDark))
        std::cerr << "[Flubber] Dark shader FAILED" << std::endl;

    // Audio texture (512 x 2, single-channel float)
    glGenTextures(1, &audioTex);
    glBindTexture(GL_TEXTURE_2D, audioTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kTexW, 2, 0, GL_RED, GL_FLOAT, nullptr);

    // Fullscreen quad — use light shader for VAO setup (both share same vertex layout)
    float verts[] = { -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,  1.0f, 1.0f };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    // Setup vertex attrib for both programs
    auto setupAttrib = [&](ShaderSet& ss) {
        if (!ss.program) return;
        ss.program->use();
        auto posAttr = (GLuint)glGetAttribLocation(ss.program->getProgramID(), "aPosition");
        glEnableVertexAttribArray(posAttr);
        glVertexAttribPointer(posAttr, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    };
    setupAttrib(lightShader);
    setupAttrib(darkShader);

    glBindVertexArray(0);

    startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void FlubberVisualizer::updateAudioTexture()
{
    std::array<float, bb::AudioVisualBuffer::kSize> rawL, rawR;
    audioL.copyTo(rawL.data(), bb::AudioVisualBuffer::kSize);
    audioR.copyTo(rawR.data(), bb::AudioVisualBuffer::kSize);

    int offset = bb::AudioVisualBuffer::kSize - kTexW;

    // Waveform row (row 1): map -1..1 to 0..1
    for (int i = 0; i < kTexW; ++i)
    {
        float mono = (rawL[offset + i] + rawR[offset + i]) * 0.5f;
        waveRow[i] = mono * 0.5f + 0.5f;
    }

    // FFT row (row 0)
    fftData.fill(0.0f);
    for (int i = 0; i < kFFTSize; ++i)
    {
        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                 * static_cast<float>(i) / static_cast<float>(kFFTSize)));
        float mono = (rawL[offset + i] + rawR[offset + i]) * 0.5f;
        fftData[i] = mono * window;
    }
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (int i = 0; i < kTexW; ++i)
    {
        float mag = fftData[i] / static_cast<float>(kFFTSize);
        float db = 20.0f * std::log10(std::max(mag, 1e-10f));
        float norm = (db + 100.0f) / 70.0f;
        norm = juce::jlimit(0.0f, 1.0f, norm);
        smoothedFFT[i] = smoothedFFT[i] * 0.65f + norm * 0.35f;
        fftRow[i] = smoothedFFT[i];
    }

    glBindTexture(GL_TEXTURE_2D, audioTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexW, 1, GL_RED, GL_FLOAT, fftRow.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, kTexW, 1, GL_RED, GL_FLOAT, waveRow.data());
}

void FlubberVisualizer::renderOpenGL()
{
    // Pick shader based on current dark mode
    auto& ss = VisceraLookAndFeel::darkMode ? darkShader : lightShader;
    if (!ss.program) return;

    updateAudioTexture();

    auto desktopScale = static_cast<float>(glContext.getRenderingScale());
    auto w = static_cast<int>(getWidth() * desktopScale);
    auto h = static_cast<int>(getHeight() * desktopScale);

    glViewport(0, 0, w, h);

    // Extract bg color once — used for clear AND uniform
    uint32_t bg = VisceraLookAndFeel::kBgColor;
    float bgR = static_cast<float>((bg >> 16) & 0xFF) / 255.0f;
    float bgG = static_cast<float>((bg >> 8)  & 0xFF) / 255.0f;
    float bgB = static_cast<float>((bg)       & 0xFF) / 255.0f;

    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ss.program->use();

    float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime);
    glUniform1f(ss.uTime, t);
    glUniform3f(ss.uResolution, static_cast<float>(w), static_cast<float>(h), 1.0f);

    glUniform3f(ss.uBgColor, bgR, bgG, bgB);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, audioTex);
    glUniform1i(ss.uChannel0, 0);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

}

void FlubberVisualizer::openGLContextClosing()
{
    lightShader.program.reset();
    darkShader.program.reset();

    if (audioTex) { glDeleteTextures(1, &audioTex); audioTex = 0; }
    if (vbo)      { glDeleteBuffers(1, &vbo);       vbo = 0; }
    if (vao)      { glDeleteVertexArrays(1, &vao);  vao = 0; }
}
