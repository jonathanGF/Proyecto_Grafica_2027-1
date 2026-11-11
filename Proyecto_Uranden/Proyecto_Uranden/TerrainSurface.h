#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <stdexcept>

// Triangulos de apoyo en coordenadas finales, independientes del streaming visual.
// BVH espacial: consulta solamente las ramas que cubren la posicion del jugador.
// Sin banda de alturas ni redondeo a celdas; conserva pisos superpuestos.
class TerrainSurface {
    struct Triangle { float p[9]; std::uint32_t foundation=0; };
    struct Node {
        glm::vec3 lo,hi;
        std::uint32_t first=0,count=0,left=0,right=0;
    };
    std::vector<Triangle> triangles;
    std::vector<Node> nodes;
    static glm::vec3 vertex(const Triangle& t,int i) {return {t.p[i*3],t.p[i*3+1],t.p[i*3+2]};}
    std::uint32_t buildNode(std::uint32_t first,std::uint32_t count) {
        Node n;n.first=first;n.count=count;
        n.lo=glm::vec3(std::numeric_limits<float>::max());n.hi=-n.lo;
        for(std::uint32_t i=first;i<first+count;++i)for(int j=0;j<3;++j) {
            auto p=vertex(triangles[i],j);n.lo=glm::min(n.lo,p);n.hi=glm::max(n.hi,p);
        }
        auto id=std::uint32_t(nodes.size());nodes.push_back(n);
        if(count>8) {
            int axis=(n.hi.x-n.lo.x>n.hi.z-n.lo.z)?0:2;
            auto mid=first+count/2;
            std::nth_element(triangles.begin()+first,triangles.begin()+mid,triangles.begin()+first+count,
                [axis](const Triangle& a,const Triangle& b){return a.p[axis]+a.p[axis+3]+a.p[axis+6]<b.p[axis]+b.p[axis+3]+b.p[axis+6];});
            auto left=buildNode(first,mid-first),right=buildNode(mid,first+count-mid);
            nodes[id].count=0;nodes[id].left=left;nodes[id].right=right;
        }
        return id;
    }
    static bool heightAt(const Triangle& t,float x,float z,float& y) {
        const double ax=t.p[0],az=t.p[2],bx=t.p[3],bz=t.p[5],cx=t.p[6],cz=t.p[8];
        const double det=(bz-cz)*(ax-cx)+(cx-bx)*(az-cz);
        if(std::abs(det)<1e-16)return false;
        const double u=((bz-cz)*(double(x)-cx)+(cx-bx)*(double(z)-cz))/det;
        const double v=((cz-az)*(double(x)-cx)+(ax-cx)*(double(z)-cz))/det;
        // Tolerancia baricentrica pequena, solo para aristas compartidas.
        if(u< -1e-6||v< -1e-6||u+v>1.000001)return false;
        y=float(u*t.p[1]+v*t.p[4]+(1-u-v)*t.p[7]);return std::isfinite(y);
    }
    template<class Visitor> void visit(std::uint32_t id,float x,float z,Visitor& visitor) const {
        const auto& n=nodes[id];
        if(x<n.lo.x-1e-6f||x>n.hi.x+1e-6f||z<n.lo.z-1e-6f||z>n.hi.z+1e-6f)return;
        if(n.count){for(std::uint32_t i=n.first;i<n.first+n.count;++i){float y;if(heightAt(triangles[i],x,z,y))visitor(y,triangles[i].foundation!=0);}}
        else {visit(n.left,x,z,visitor);visit(n.right,x,z,visitor);}
    }
    template<class Visitor> void heightsAt(float x,float z,Visitor& visitor) const {
        if(!std::isfinite(x)||!std::isfinite(z))return;
        if(!nodes.empty())visit(0,x,z,visitor);
        else for(const auto& t:triangles){float y;if(heightAt(t,x,z,y))visitor(y,t.foundation!=0);}
    }
public:
    void clear(){triangles.clear();nodes.clear();}
    void triangle(const glm::vec3& a,const glm::vec3& b,const glm::vec3& c,bool foundation=false) {
        for(const auto& p:{a,b,c})for(int i=0;i<3;++i)if(!std::isfinite(p[i]))return;
        auto normal=glm::cross(b-a,c-a);float area2=glm::dot(normal,normal);
        // Solo superficies con proyeccion horizontal. Sin exigir sentido de las
        // normales, porque algunas piezas importadas tienen winding invertido.
        if(area2<1e-20f||normal.y*normal.y<area2*0.0001f)return;
        if(!nodes.empty())nodes.clear();
        triangles.push_back({{a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z},foundation?1u:0u});
    }
    void build(){nodes.clear();if(!triangles.empty()){nodes.reserve(triangles.size()/3+1);buildNode(0,std::uint32_t(triangles.size()));}}
    std::size_t triangleCount() const{return triangles.size();}
    // Piso mas alto que se puede alcanzar desde los pies; un techo queda arriba
    // del limite y no sustituye al piso inferior.
    bool sampleBelow(float x,float z,float ceiling,float& y) const {
        bool found=false;float best=-std::numeric_limits<float>::infinity();
        auto v=[&](float h,bool){if(h<=ceiling+1e-5f&&h>best){best=h;found=true;}};
        heightsAt(x,z,v);if(found)y=best;return found;
    }
    // Solo para colocacion inicial/teletransporte, nunca para atravesar pendientes.
    bool sampleNearest(float x,float z,float reference,float& y) const {
        bool found=false;float best=std::numeric_limits<float>::infinity(),value=0;
        auto v=[&](float h,bool){float d=std::abs(h-reference);if(d<best||(d==best&&h>value)){best=d;value=h;found=true;}};
        heightsAt(x,z,v);if(found)y=value;return found;
    }
    // El terreno base nunca se trata como un techo o como la cara inferior de un puente.
    bool sampleFoundation(float x,float z,float& y) const {
        bool found=false;float best=-std::numeric_limits<float>::infinity();
        auto v=[&](float h,bool primary){if(primary&&h>best){best=h;found=true;}};
        heightsAt(x,z,v);if(found)y=best;return found;
    }
    bool sample(float x,float z,float& y) const {return sampleBelow(x,z,std::numeric_limits<float>::infinity(),y);}
    void save(const std::filesystem::path& p) const {
        auto tmp=p;tmp+=".tmp";std::ofstream f(tmp,std::ios::binary);
        std::uint32_t magic=0x55475333;std::uint64_t n=triangles.size();
        f.write(reinterpret_cast<const char*>(&magic),4);f.write(reinterpret_cast<const char*>(&n),8);
        f.write(reinterpret_cast<const char*>(triangles.data()),std::streamsize(n*sizeof(Triangle)));
        f.close();if(!f)throw std::runtime_error("No se pudo guardar la superficie de colision");
        std::error_code ec;std::filesystem::remove(p,ec);std::filesystem::rename(tmp,p);
    }
    bool load(const std::filesystem::path& p) {
        clear();std::ifstream f(p,std::ios::binary);std::uint32_t magic=0;std::uint64_t n=0;
        f.read(reinterpret_cast<char*>(&magic),4);f.read(reinterpret_cast<char*>(&n),8);
        if(!f||magic!=0x55475333||n>20000000)return false;
        if(std::filesystem::file_size(p)!=12+n*sizeof(Triangle))return false;
        triangles.resize(std::size_t(n));
        f.read(reinterpret_cast<char*>(triangles.data()),std::streamsize(n*sizeof(Triangle)));
        if(!f){clear();return false;}
        for(const auto& t:triangles){if(t.foundation>1){clear();return false;}for(float v:t.p)if(!std::isfinite(v)){clear();return false;}}
        build();return true;
    }
};
