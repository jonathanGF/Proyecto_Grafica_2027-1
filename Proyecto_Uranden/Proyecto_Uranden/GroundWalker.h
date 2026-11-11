#pragma once
#include "TerrainSurface.h"

// Control de apoyo en el suelo (no es un motor de colision de paredes).
// El recorrido se divide en pasos pequenos para no saltarse bordes/huecos.
class GroundWalker {
    glm::vec3 feet{0,-0.2f,0};
    bool ready=false;
    bool support(const TerrainSurface& ground,float x,float z,float reference,float& y) const {
        float base;
        bool hasBase=ground.sampleFoundation(x,z,base);
        if(hasBase&&base>reference+maxStepUp+1e-5f)return false;
        bool found=false;float highest=-std::numeric_limits<float>::infinity();
        const glm::vec2 probes[]={{0,0},{footRadius,0},{-footRadius,0},{0,footRadius},{0,-footRadius}};
        for(const auto& p:probes) {
            float h;
            if(ground.sampleBelow(x+p.x,z+p.y,reference+maxStepUp,h)&&h>=reference-maxStepDown-1e-5f) {
                highest=(std::max)(highest,h);found=true;
            }
        }
        if(found&&hasBase)highest=(std::max)(highest,base);
        if(found)y=highest;
        return found;
    }
public:
    float eyeHeight=0.28f;
    float maxStepUp=0.12f;
    float maxStepDown=0.30f;
    float footRadius=0.045f;
    float sweepStep=0.02f;
    bool initialized() const{return ready;}
    const glm::vec3& position() const{return feet;}
    glm::vec3 eye() const{return feet+glm::vec3(0,eyeHeight,0);}
    bool place(const TerrainSurface& ground,float x,float z,float reference) {
        float h;
        if(!ground.sampleNearest(x,z,reference,h))return false;
        float base;
        if(ground.sampleFoundation(x,z,base))h=(std::max)(h,base);
        feet={x,h,z};ready=true;return true;
    }
    void move(const TerrainSurface& ground,float x,float z) {
        if(!ready||!std::isfinite(x)||!std::isfinite(z))return;
        glm::vec2 delta(x-feet.x,z-feet.z);
        int steps=(std::max)(1,int(std::ceil(glm::length(delta)/(std::max)(sweepStep,0.001f))));
        if(steps>10000)return; // Un teletransporte debe usar place(), no atravesar la escena.
        delta/=float(steps);
        for(int i=0;i<steps;++i) {
            float h;glm::vec2 next=glm::vec2(feet.x,feet.z)+delta;
            if(support(ground,next.x,next.y,feet.y,h)){feet={next.x,h,next.y};continue;}
            // Deslizamiento por el limite real del terreno.
            bool moved=false;
            if(delta.x!=0&&support(ground,next.x,feet.z,feet.y,h)){feet.x=next.x;feet.y=h;moved=true;}
            if(delta.y!=0&&support(ground,feet.x,next.y,feet.y,h)){feet.z=next.y;feet.y=h;moved=true;}
            if(!moved)break;
        }
    }
};
