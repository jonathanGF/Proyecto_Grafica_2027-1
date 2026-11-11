#pragma once
// C++17, Assimp, GLM, GLEW y SOIL2. Mapa estatico; no modifica el glTF.
// Todo OpenGL se ejecuta en el hilo principal. El trabajador solo usa disco/CPU.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include "SOIL2/SOIL2.h"
#include "TerrainSurface.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <future>
#include <memory>
#include <cctype>
#include <tuple>
#include <chrono>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace uranden {
static_assert(sizeof(void*) >= 8, "Este mapa requiere compilar el proyecto en x64");
namespace fs = std::filesystem;
constexpr std::size_t MiB = 1024u * 1024u;
struct StreamSettings {
    // Coincidencia EXACTA de nombre de imagen; no elimina otras personas "Mesh0".
    std::vector<std::string> excludedBaseTextures = {
        "Meshy_AI_Straw_Hat_Villager_1019172923_texture.png"
    };
    std::vector<std::string> excludedNodeNames = {"agua"};
    // Vegetacion identificada en este mapa: se dibuja, pero no es piso.
    std::vector<std::string> terrainExcludedPrefixes = {
        "arbol dificil", "Bark.", "Leaves.", "foliage.", "Euro_Beech_",
        "tree", "TreeGen_", "crown ", "br_low_trunk_", "trunk_low_", "VS_Linea_Flores_"
    };
    std::vector<std::string> foundationNodeNames = {"Terreno", "Terreno.001"};
    float terrainScale = 0.2f;
    glm::vec3 terrainOffset = glm::vec3(0,-0.2f,0);
    float drawDistance = 24.0f;       // Unidades de OpenGL, no de Blender.
    float preloadDistance = 28.0f;
    float unloadDistance = 34.0f;     // Histeresis: evita descargar al girar.
    float fogStart = 17.0f;
    float minPixels = 1.0f;           // 0 desactiva rechazo subpixel.
    float cellSize = 20.0f;           // Unidades glTF: 20 * escala 0.2 = 4.
    unsigned maxTrianglesPerPage = 16384;
    unsigned maxTextureSize = 2048;
    std::size_t geometryBudget = 384 * MiB;
    std::size_t textureBudget = 512 * MiB;
};
struct StreamStats {
    std::size_t total = 0, resident = 0, visible = 0, drawn = 0;
    std::size_t triangles = 0, geometryBytes = 0, textureBytes = 0;
    bool loading = false, budgetLimited = false;
};
struct StreamVertex { float p[3], n[3], uv[2], color[4]; };
static_assert(sizeof(StreamVertex) == 48, "Formato de cache inesperado");
struct Box {
    glm::vec3 lo = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 hi = glm::vec3(-std::numeric_limits<float>::max());
    void add(const glm::vec3& p) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
};
inline float distanceToBox(const Box& b, const glm::vec3& p) {
    glm::vec3 d = glm::max(glm::max(b.lo - p, p - b.hi), glm::vec3(0));
    return glm::length(d);
}
inline std::array<glm::vec4,6> planes(const glm::mat4& m) {
    glm::vec4 r[4];
    for (int i=0;i<4;++i) r[i]=glm::vec4(m[0][i],m[1][i],m[2][i],m[3][i]);
    return {{r[3]+r[0],r[3]-r[0],r[3]+r[1],r[3]-r[1],r[3]+r[2],r[3]-r[2]}};
}
inline bool inFrustum(const Box& b,const std::array<glm::vec4,6>& ps) {
    for (const auto& p:ps) {
        glm::vec3 v(p.x>=0?b.hi.x:b.lo.x,p.y>=0?b.hi.y:b.lo.y,p.z>=0?b.hi.z:b.lo.z);
        if (glm::dot(glm::vec3(p),v)+p.w < 0) return false;
    }
    return true;
}
template<class T> inline void put(std::ostream& f,const T& v) {
    f.write(reinterpret_cast<const char*>(&v),sizeof(T));
    if(!f) throw std::runtime_error("No se pudo escribir cache (revisa espacio en disco)");
}
template<class T> inline void get(std::istream& f,T& v) {
    f.read(reinterpret_cast<char*>(&v),sizeof(T));
    if(!f) throw std::runtime_error("Cache incompleta: elimina solamente la carpeta .uranden_cache_v3");
}
inline std::string uriDecode(const std::string& s) {
    std::string o;
    auto hex=[](char c)->int { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; };
    for(std::size_t i=0;i<s.size();++i) {
        if(s[i]=='%'&&i+2<s.size()&&hex(s[i+1])>=0&&hex(s[i+2])>=0) {
            o+=char(hex(s[i+1])*16+hex(s[i+2])); i+=2;
        } else o+=s[i];
    }
    std::replace(o.begin(),o.end(),'\\','/'); return o;
}
inline std::uint64_t hashText(std::uint64_t h,const std::string& s) {
    for(unsigned char c:s) {h^=c; h*=1099511628211ull;} return h;
}
inline std::string stamp(const fs::path& p) {
    if(!fs::is_regular_file(p)) return p.generic_string()+":missing";
    return p.generic_string()+":"+std::to_string(fs::file_size(p))+":"+
        std::to_string(fs::last_write_time(p).time_since_epoch().count());
}
inline std::string hexName(std::uint64_t h) { std::ostringstream s;s<<std::hex<<h;return s.str(); }
inline glm::mat4 toGLM(const aiMatrix4x4& m) {
    return glm::mat4(m.a1,m.b1,m.c1,m.d1,m.a2,m.b2,m.c2,m.d2,
                     m.a3,m.b3,m.c3,m.d3,m.a4,m.b4,m.c4,m.d4);
}

class StreamingMap {
    struct Material { glm::vec4 color=glm::vec4(1); int texture=-1, alpha=0; float cutoff=0.5f; int wrapS=GL_REPEAT,wrapT=GL_REPEAT; };
    struct Page {
        Box box; unsigned material=0,nv=0,ni=0;
        GLuint vao=0,vbo=0,ebo=0; bool failed=false;
        bool visible=false,preloadWanted=false; float distance=0; double lastUse=0;
        std::size_t bytes() const {return std::size_t(nv)*sizeof(StreamVertex)+std::size_t(ni)*4;}
    };
    struct Tex { std::string path; GLuint id=0; std::size_t bytes=0; double lastUse=0; bool failed=false,pinned=false; };
    struct Result {
        bool texture=false; std::size_t index=0; std::vector<StreamVertex> vertices;
        std::vector<unsigned> indices; std::vector<unsigned char> pixels;
        unsigned width=0,height=0; std::string error;
    };
    StreamSettings cfg; fs::path cache,source;
    TerrainSurface terrain;
    std::vector<Page> pages; std::vector<Material> materials; std::vector<Tex> textures;
    std::vector<unsigned> canonicalMaterial;
    std::vector<bool> excludedMaterial;
    std::size_t excludedMeshes=0, excludedTriangles=0;
    // Capacidad reutilizada entre cuadros: evita reservar listas cada frame.
    std::vector<std::size_t> drawOrder,opaqueOrder,transparentOrder;
    std::future<Result> task; bool pending=false,pendingTexture=false;
    std::size_t pendingIndex=0,geoBytes=0,texBytes=0;
    GLuint program=0;
    GLint uTexture=-1,uUse=-1,uColor=-1,uAlpha=-1,uMode=-1,uCutoff=-1,uMap=-1,uFogStart=-1,uFogEnd=-1;
    bool initialized=false;
    std::atomic<int> phase{0};
    std::atomic<bool> cancelled{false};
    StreamStats stats;

    fs::path pagePath(std::size_t i) const {return cache/("page_"+std::to_string(i)+".bin");}
    fs::path texturePath(std::size_t i) const {
        return cache/("tex_"+hexName(hashText(1469598103934665603ull,
            stamp(fs::u8path(textures[i].path))+std::to_string(cfg.maxTextureSize)))+".rgba");
    }
    void checkCancel() const { if(cancelled.load()) throw std::runtime_error("Preparacion cancelada"); }
    void saveIndex(const std::string& signature) {
        std::ofstream f(cache/"index.tmp");
        if(!f) throw std::runtime_error("No se puede crear cache junto al glTF");
        f<<"URANDEN_CACHE_3 "<<signature<<'\n'<<std::setprecision(9);
        f<<textures.size()<<'\n';for(const auto& t:textures) f<<std::quoted(t.path)<<'\n';
        f<<materials.size()<<'\n';
        for(const auto& m:materials) f<<m.color.r<<' '<<m.color.g<<' '<<m.color.b<<' '<<m.color.a<<' '
            <<m.texture<<' '<<m.alpha<<' '<<m.cutoff<<' '<<m.wrapS<<' '<<m.wrapT<<'\n';
        f<<pages.size()<<'\n';
        for(const auto& p:pages) f<<p.box.lo.x<<' '<<p.box.lo.y<<' '<<p.box.lo.z<<' '
            <<p.box.hi.x<<' '<<p.box.hi.y<<' '<<p.box.hi.z<<' '<<p.material<<' '<<p.nv<<' '<<p.ni<<'\n';
        f.close(); if(!f) throw std::runtime_error("Disco lleno al guardar indice");
        fs::rename(cache/"index.tmp",cache/"index.txt");
    }
    bool readIndex(const std::string& signature) {
        std::ifstream f(cache/"index.txt"); std::string magic,sig; f>>magic>>sig;
        if(!f||magic!="URANDEN_CACHE_3"||sig!=signature) return false;
        std::size_t n=0; f>>n;if(!f||n>100000)return false;
        textures.resize(n);for(auto& t:textures)f>>std::quoted(t.path);
        f>>n;if(!f||n>100000)return false;
        materials.resize(n);for(auto& m:materials) {
            f>>m.color.r>>m.color.g>>m.color.b>>m.color.a>>m.texture>>m.alpha>>m.cutoff>>m.wrapS>>m.wrapT;
            if(m.texture < -1 || m.texture>=int(textures.size()) || m.alpha<0 || m.alpha>2)return false;
        }
        f>>n;if(!f||n>1000000)return false;
        pages.resize(n);for(std::size_t i=0;i<n;++i) {
            auto& p=pages[i]; f>>p.box.lo.x>>p.box.lo.y>>p.box.lo.z>>p.box.hi.x>>p.box.hi.y>>p.box.hi.z>>p.material>>p.nv>>p.ni;
            if(!f||p.material>=materials.size()||p.nv>cfg.maxTrianglesPerPage*3||p.ni>cfg.maxTrianglesPerPage*3
                ||!fs::is_regular_file(pagePath(i))||fs::file_size(pagePath(i))!=8+p.bytes())return false;
        }
        return bool(f)&&!pages.empty();
    }
    int textureIndex(const aiString& name,const aiScene* scene) {
        if(name.length==0)return -1;
        std::string path;
        if(const aiTexture* embedded=scene->GetEmbeddedTexture(name.C_Str())) {
            // Materializar solo bytes comprimidos; SOIL los decodifica bajo demanda.
            if(embedded->mHeight!=0) throw std::runtime_error("Textura embebida sin comprimir no soportada; usa glTF separado");
            fs::path dest=cache/("embedded_"+hexName(hashText(1469598103934665603ull,name.C_Str()))+".img");
            std::ofstream f(dest,std::ios::binary); f.write(reinterpret_cast<const char*>(embedded->pcData),embedded->mWidth);
            f.close();if(!f)throw std::runtime_error("Fallo al guardar imagen embebida");
            path=dest.u8string();
        } else {
            fs::path p=fs::u8path(uriDecode(name.C_Str()));
            path=(p.is_absolute()?p:source.parent_path()/p).lexically_normal().u8string();
        }
        for(std::size_t i=0;i<textures.size();++i)if(textures[i].path==path)return int(i);
        Tex t;t.path=path;textures.push_back(t);return int(textures.size()-1);
    }
    bool isExcludedTexture(const aiString& name) const {
        auto lower=[](std::string text) {
            for(char& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        };
        const std::string base=lower(fs::u8path(uriDecode(name.C_Str())).filename().u8string());
        for(const auto& item:cfg.excludedBaseTextures)
            if(base==lower(item))return true;
        return false;
    }
    void makeMaterials(const aiScene* scene) {
        materials.resize(scene->mNumMaterials);
        canonicalMaterial.resize(scene->mNumMaterials);
        excludedMaterial.assign(scene->mNumMaterials,false);
        for(unsigned i=0;i<scene->mNumMaterials;++i) {
            const aiMaterial* a=scene->mMaterials[i]; auto& m=materials[i];
            aiColor4D c(1,1,1,1);
            if(aiGetMaterialColor(a,"$clr.base",0,0,&c)!=AI_SUCCESS) aiGetMaterialColor(a,AI_MATKEY_COLOR_DIFFUSE,&c);
            m.color=glm::vec4(c.r,c.g,c.b,c.a);
            aiString mode;if(a->Get("$mat.gltf.alphaMode",0,0,mode)==AI_SUCCESS) {
                if(std::string(mode.C_Str())=="MASK")m.alpha=1;
                if(std::string(mode.C_Str())=="BLEND")m.alpha=2;
            }
            a->Get("$mat.gltf.alphaCutoff",0,0,m.cutoff);
            aiString name; unsigned uv=0; aiTextureMapMode wrap[3]={aiTextureMapMode_Wrap,aiTextureMapMode_Wrap,aiTextureMapMode_Wrap};
            // Assimp 5: BASE_COLOR=12, con fallback DIFFUSE para instalaciones anteriores.
            if(a->GetTexture(static_cast<aiTextureType>(12),0,&name,nullptr,&uv,nullptr,nullptr,wrap)!=AI_SUCCESS)
                a->GetTexture(aiTextureType_DIFFUSE,0,&name,nullptr,&uv,nullptr,nullptr,wrap);
            excludedMaterial[i]=isExcludedTexture(name);
            if(!excludedMaterial[i])m.texture=textureIndex(name,scene);
            auto modeGL=[](aiTextureMapMode x){return x==aiTextureMapMode_Clamp?GL_CLAMP_TO_EDGE:x==aiTextureMapMode_Mirror?GL_MIRRORED_REPEAT:GL_REPEAT;};
            m.wrapS=modeGL(wrap[0]);m.wrapT=modeGL(wrap[1]);
        }
        // UV/transformacion de textura ya resueltas por Assimp en cada malla.
        // Solo unificamos el estado que usa este shader; la geometria y sus UV
        // permanecen propias. No comparamos nombres como "Material.001".
        using Key=std::tuple<float,float,float,float,int,int,float,int,int>;
        std::map<Key,unsigned> unique;
        for(unsigned i=0;i<materials.size();++i) {
            const auto& m=materials[i];canonicalMaterial[i]=i;
            if(excludedMaterial[i])continue;
            Key key(m.color.r,m.color.g,m.color.b,m.color.a,m.texture,m.alpha,m.cutoff,m.wrapS,m.wrapT);
            auto result=unique.emplace(key,i);canonicalMaterial[i]=result.first->second;
        }
        std::cout<<"Materiales: "<<materials.size()<<" originales -> "<<unique.size()
                 <<" grupos compatibles con el shader.\n";
    }
    void writePage(unsigned material,const std::vector<StreamVertex>& v,const std::vector<unsigned>& ix) {
        if(ix.empty())return;
        Page p;p.material=material;p.nv=unsigned(v.size());p.ni=unsigned(ix.size());
        for(const auto& x:v)p.box.add(glm::vec3(x.p[0],x.p[1],x.p[2]));
        std::ofstream f(pagePath(pages.size()),std::ios::binary);
        put(f,p.nv);put(f,p.ni);
        f.write(reinterpret_cast<const char*>(v.data()),std::streamsize(v.size()*sizeof(StreamVertex)));
        f.write(reinterpret_cast<const char*>(ix.data()),std::streamsize(ix.size()*sizeof(unsigned)));
        f.close();if(!f)throw std::runtime_error("No se pudo guardar una zona: revisa espacio libre");
        pages.push_back(p);
    }
    void makeMesh(const aiMesh* mesh,const glm::mat4& transform,const aiScene* scene,bool supportGeometry,bool foundation) {
        if(!mesh->HasPositions()||mesh->mNumFaces==0)return;
        if(excludedMaterial.at(mesh->mMaterialIndex)) {
            ++excludedMeshes;
            for(unsigned i=0;i<mesh->mNumFaces;++i)
                if(mesh->mFaces[i].mNumIndices==3)++excludedTriangles;
            return; // Antes de transformar vertices, crear paginas o cargar imagenes.
        }
        if(mesh->HasBones())throw std::runtime_error("El mapa contiene un skin. Carga personajes animados por separado del mapa estatico");
        glm::mat3 normalMatrix=glm::transpose(glm::inverse(glm::mat3(transform)));
        bool mirrored=glm::determinant(glm::mat3(transform))<0;
        std::vector<glm::vec3> positions(mesh->mNumVertices);
        for(unsigned i=0;i<mesh->mNumVertices;++i) {
            auto p=mesh->mVertices[i];positions[i]=glm::vec3(transform*glm::vec4(p.x,p.y,p.z,1));
            if(!std::isfinite(positions[i].x)||!std::isfinite(positions[i].y)||!std::isfinite(positions[i].z))throw std::runtime_error("Vertices no finitos en el mapa");
        }
        // Separacion por centro de triangulo, SIN eliminar caras ni cambiar geometria.
        // Las cajas incluyen vertices reales, tambien si atraviesan una celda.
        std::map<std::pair<int,int>,std::vector<unsigned>> cells;
        for(unsigned i=0;i<mesh->mNumFaces;++i) {
            const auto& f=mesh->mFaces[i];if(f.mNumIndices!=3)continue;
            if(supportGeometry)terrain.triangle(positions[f.mIndices[0]]*cfg.terrainScale+cfg.terrainOffset,
                positions[f.mIndices[1]]*cfg.terrainScale+cfg.terrainOffset,
                positions[f.mIndices[2]]*cfg.terrainScale+cfg.terrainOffset,foundation);
            glm::vec3 c=(positions[f.mIndices[0]]+positions[f.mIndices[1]]+positions[f.mIndices[2]])/3.0f;
            cells[{int(std::floor(c.x/cfg.cellSize)),int(std::floor(c.z/cfg.cellSize))}].push_back(i);
        }
        unsigned uvChannel=0;aiString unused;const aiMaterial* mat=scene->mMaterials[mesh->mMaterialIndex];
        if(mat->GetTexture(static_cast<aiTextureType>(12),0,&unused,nullptr,&uvChannel)!=AI_SUCCESS)
            mat->GetTexture(aiTextureType_DIFFUSE,0,&unused,nullptr,&uvChannel);
        if(uvChannel>=AI_MAX_NUMBER_OF_TEXTURECOORDS)uvChannel=0;
        if(materials[mesh->mMaterialIndex].texture>=0&&!mesh->HasTextureCoords(uvChannel))
            throw std::runtime_error("Falta el canal UV requerido por una textura");
        for(const auto& entry:cells) {
            const auto& faces=entry.second;
            for(std::size_t start=0;start<faces.size();start+=cfg.maxTrianglesPerPage) {
                checkCancel();std::vector<StreamVertex> v;std::vector<unsigned> ix;std::unordered_map<unsigned,unsigned> remap;
                auto end=(std::min)(faces.size(),start+cfg.maxTrianglesPerPage);ix.reserve((end-start)*3);
                for(std::size_t k=start;k<end;++k) {
                    const auto& f=mesh->mFaces[faces[k]];
                    for(unsigned j=0;j<3;++j) {
                        unsigned old=f.mIndices[mirrored&&j>0?3-j:j];auto it=remap.find(old);
                        if(it!=remap.end()){ix.push_back(it->second);continue;}
                        StreamVertex x{};auto p=positions[old];x.p[0]=p.x;x.p[1]=p.y;x.p[2]=p.z;
                        glm::vec3 n(0,1,0);if(mesh->HasNormals()) {
                            auto a=mesh->mNormals[old];n=normalMatrix*glm::vec3(a.x,a.y,a.z);
                            n=glm::dot(n,n)>1e-16f?glm::normalize(n):glm::vec3(0,1,0);
                        }
                        x.n[0]=n.x;x.n[1]=n.y;x.n[2]=n.z;
                        if(mesh->HasTextureCoords(uvChannel)){x.uv[0]=mesh->mTextureCoords[uvChannel][old].x;x.uv[1]=mesh->mTextureCoords[uvChannel][old].y;}
                        for(int a=0;a<4;++a)x.color[a]=1;
                        if(mesh->HasVertexColors(0)){auto c=mesh->mColors[0][old];x.color[0]=c.r;x.color[1]=c.g;x.color[2]=c.b;x.color[3]=c.a;}
                        unsigned id=unsigned(v.size());remap.emplace(old,id);v.push_back(x);ix.push_back(id);
                    }
                }
                writePage(canonicalMaterial[mesh->mMaterialIndex],v,ix);
            }
        }
    }
    void walk(const aiNode* node,const glm::mat4& parent,const aiScene* scene) {
        checkCancel();glm::mat4 tr=parent*toGLM(node->mTransformation);
        bool omit=std::find(cfg.excludedNodeNames.begin(),cfg.excludedNodeNames.end(),
            std::string(node->mName.C_Str()))!=cfg.excludedNodeNames.end();
        if(omit)std::cout<<"Nodo excluido: "<<node->mName.C_Str()<<'\n';
        bool supportGeometry=true;
        const std::string name=node->mName.C_Str();
        for(const auto& prefix:cfg.terrainExcludedPrefixes)if(name.compare(0,prefix.size(),prefix)==0){supportGeometry=false;break;}
        bool foundation=std::find(cfg.foundationNodeNames.begin(),cfg.foundationNodeNames.end(),name)!=cfg.foundationNodeNames.end();
        if(!omit)for(unsigned i=0;i<node->mNumMeshes;++i)makeMesh(scene->mMeshes[node->mMeshes[i]],tr,scene,supportGeometry,foundation);
        for(unsigned i=0;i<node->mNumChildren;++i)walk(node->mChildren[i],tr,scene);
    }
    // Fusiona piezas pequenas de la misma zona/material para reducir draw calls.
    // Trabaja desde disco con unas pocas paginas en RAM, sin cargar toda la cache.
    void packPages() {
        const std::size_t originalCount=pages.size();
        std::map<std::array<int,3>,std::vector<std::size_t>> groups;
        for(std::size_t i=0;i<originalCount;++i) {
            const auto& p=pages[i];glm::vec3 c=(p.box.lo+p.box.hi)*0.5f;
            groups[{{int(p.material),int(std::floor(c.x/cfg.cellSize)),int(std::floor(c.z/cfg.cellSize))}}].push_back(i);
        }
        for(const auto& entry:groups) {
            std::vector<StreamVertex> vertices;std::vector<unsigned> indices;
            for(std::size_t i:entry.second) {
                checkCancel();Result src=readPage(i);
                if(indices.size()+src.indices.size()>std::size_t(cfg.maxTrianglesPerPage)*3) {
                    writePage(unsigned(entry.first[0]),vertices,indices);vertices.clear();indices.clear();
                }
                unsigned offset=unsigned(vertices.size());
                vertices.insert(vertices.end(),src.vertices.begin(),src.vertices.end());
                for(unsigned id:src.indices)indices.push_back(id+offset);
            }
            writePage(unsigned(entry.first[0]),vertices,indices);
        }
        std::vector<Page> packed(pages.begin()+originalCount,pages.end());
        for(std::size_t i=0;i<packed.size();++i) {
            fs::remove(pagePath(i));fs::rename(pagePath(originalCount+i),pagePath(i));
        }
        for(std::size_t i=packed.size();i<originalCount;++i)fs::remove(pagePath(i));
        pages=std::move(packed);
    }
    Result readPage(std::size_t i) const {
        Result r;r.index=i;std::ifstream f(pagePath(i),std::ios::binary);unsigned nv=0,ni=0;get(f,nv);get(f,ni);
        if(nv!=pages[i].nv||ni!=pages[i].ni)throw std::runtime_error("Zona de cache invalida");
        r.vertices.resize(nv);r.indices.resize(ni);
        f.read(reinterpret_cast<char*>(r.vertices.data()),std::streamsize(nv*sizeof(StreamVertex)));
        f.read(reinterpret_cast<char*>(r.indices.data()),std::streamsize(ni*sizeof(unsigned)));
        if(!f)throw std::runtime_error("Lectura incompleta de zona");
        for(unsigned id:r.indices)if(id>=nv)throw std::runtime_error("Indice fuera de rango en cache");
        return r;
    }
    Result readTexture(std::size_t i) const {
        Result r;r.texture=true;r.index=i;fs::path target=texturePath(i);
        {
            std::ifstream f(target,std::ios::binary);
            if(f) {
                std::uint32_t magic=0;get(f,magic);get(f,r.width);get(f,r.height);
                if(magic!=0x55494D33||!r.width||!r.height||r.width>cfg.maxTextureSize||r.height>cfg.maxTextureSize)
                    throw std::runtime_error("Cache de textura invalida: elimina .uranden_cache_v3");
                r.pixels.resize(std::size_t(r.width)*r.height*4);
                f.read(reinterpret_cast<char*>(r.pixels.data()),std::streamsize(r.pixels.size()));
                if(!f)throw std::runtime_error("Cache de textura truncada");
                return r;
            }
        }
        int w=0,h=0,ch=0;
        unsigned char* raw=SOIL_load_image(textures[i].path.c_str(),&w,&h,&ch,SOIL_LOAD_RGBA);
        if(!raw||w<=0||h<=0){if(raw)SOIL_free_image_data(raw);throw std::runtime_error("Textura no disponible: "+textures[i].path);}
        std::unique_ptr<unsigned char,void(*)(unsigned char*)> pixels(raw,SOIL_free_image_data);
        double factor=(std::min)(1.0,double(cfg.maxTextureSize)/(std::max)(w,h));
        r.width=(std::max)(1u,unsigned(w*factor));r.height=(std::max)(1u,unsigned(h*factor));
        r.pixels.resize(std::size_t(r.width)*r.height*4);
        // Promedio de area: reduce aliasing de texturas muy grandes. Invierte Y
        // igual que el cargador original (SOIL_FLAG_INVERT_Y + UV de Assimp).
        for(unsigned y=0;y<r.height;++y) {
            unsigned sy0=unsigned(std::uint64_t(y)*h/r.height),sy1=unsigned(std::uint64_t(y+1)*h/r.height);
            for(unsigned x=0;x<r.width;++x) {
                unsigned sx0=unsigned(std::uint64_t(x)*w/r.width),sx1=unsigned(std::uint64_t(x+1)*w/r.width);
                std::uint64_t sum[4]={0,0,0,0};std::uint64_t count=std::uint64_t(sy1-sy0)*(sx1-sx0);
                for(unsigned yy=sy0;yy<sy1;++yy)for(unsigned xx=sx0;xx<sx1;++xx) {
                    const auto* p=raw+(std::size_t(yy)*w+xx)*4;for(int c=0;c<4;++c)sum[c]+=p[c];
                }
                auto* p=r.pixels.data()+(std::size_t(r.height-1-y)*r.width+x)*4;
                for(int c=0;c<4;++c)p[c]=static_cast<unsigned char>(sum[c]/count);
            }
        }
        // Cache derivada opcional: si falla su escritura, la textura aun se usa.
        fs::path tmp=target;tmp+=".tmp";
        std::ofstream f(tmp,std::ios::binary);
        if(f) {
            std::uint32_t magic=0x55494D33;f.write(reinterpret_cast<const char*>(&magic),4);
            f.write(reinterpret_cast<const char*>(&r.width),4);
            f.write(reinterpret_cast<const char*>(&r.height),4);
            f.write(reinterpret_cast<const char*>(r.pixels.data()),std::streamsize(r.pixels.size()));f.close();
            if(f){std::error_code ec;fs::rename(tmp,target,ec);}
        }
        return r;
    }
    void unload(Page& p) {
        if(!p.vao)return;
        glDeleteVertexArrays(1,&p.vao);glDeleteBuffers(1,&p.vbo);glDeleteBuffers(1,&p.ebo);
        p.vao=p.vbo=p.ebo=0;geoBytes-=p.bytes();
    }
    void unload(Tex& t) {if(t.id){glDeleteTextures(1,&t.id);t.id=0;texBytes-=t.bytes;t.bytes=0;}}
    bool roomGeometry(std::size_t bytes) {
        if(bytes>cfg.geometryBudget)return false;
        while(geoBytes+bytes>cfg.geometryBudget) {
            Page* oldest=nullptr;for(auto& p:pages)if(p.vao&&!p.visible&&(!oldest||p.lastUse<oldest->lastUse))oldest=&p;
            if(!oldest)return false;
            unload(*oldest);
        }return true;
    }
    bool roomTexture(std::size_t bytes) {
        if(bytes>cfg.textureBudget)return false;
        while(texBytes+bytes>cfg.textureBudget) {
            Tex* oldest=nullptr;for(auto& t:textures)if(t.id&&!t.pinned&&(!oldest||t.lastUse<oldest->lastUse))oldest=&t;
            if(!oldest)return false;
            unload(*oldest);
        }return true;
    }
    void upload(Result& r,double now) {
        for(GLenum e=glGetError();e!=GL_NO_ERROR;e=glGetError())
            std::cerr<<"STREAM: error OpenGL previo a subida: "<<e<<'\n';
        if(r.texture) {
            auto& t=textures[r.index];if(!t.pinned&&now-t.lastUse>4)return;
            std::size_t bytes=r.pixels.size()*4/3+64;
            if(!roomTexture(bytes)){stats.budgetLimited=true;return;}
            glGenTextures(1,&t.id);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,t.id);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,r.width,r.height,0,GL_RGBA,GL_UNSIGNED_BYTE,r.pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            if(glGetError()!=GL_NO_ERROR) {
                glDeleteTextures(1,&t.id);t.id=0;t.failed=true;
                std::cerr<<"STREAM: fallo OpenGL al subir textura; reduce maxTextureSize/presupuesto.\n";
                return;
            }
            t.bytes=bytes;texBytes+=bytes;t.lastUse=now;
        } else {
            auto& p=pages[r.index];if(!p.preloadWanted)return;
            if(!roomGeometry(p.bytes())){stats.budgetLimited=true;return;}
            glGenVertexArrays(1,&p.vao);glGenBuffers(1,&p.vbo);glGenBuffers(1,&p.ebo);glBindVertexArray(p.vao);
            glBindBuffer(GL_ARRAY_BUFFER,p.vbo);glBufferData(GL_ARRAY_BUFFER,r.vertices.size()*sizeof(StreamVertex),r.vertices.data(),GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,p.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,r.indices.size()*4,r.indices.data(),GL_STATIC_DRAW);
            const int count[]={3,3,2,4};const std::size_t offsets[]={0,12,24,32};
            for(int a=0;a<4;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,count[a],GL_FLOAT,GL_FALSE,sizeof(StreamVertex),reinterpret_cast<void*>(offsets[a]));}
            glBindVertexArray(0);
            if(glGetError()!=GL_NO_ERROR) {
                glDeleteVertexArrays(1,&p.vao);glDeleteBuffers(1,&p.vbo);glDeleteBuffers(1,&p.ebo);
                p.vao=p.vbo=p.ebo=0;p.failed=true;
                std::cerr<<"STREAM: fallo OpenGL al subir geometria.\n";return;
            }
            geoBytes+=p.bytes();p.lastUse=now;
        }
    }
    void launch(bool texture,std::size_t i) {
        pending=true;pendingTexture=texture;pendingIndex=i;
        task=std::async(std::launch::async,[this,texture,i] {
            try {return texture?readTexture(i):readPage(i);}catch(const std::exception& e){Result r;r.texture=texture;r.index=i;r.error=e.what();return r;}
        });
    }
public:
    explicit StreamingMap(StreamSettings settings={}):cfg(settings) {}
    StreamingMap(const StreamingMap&)=delete;StreamingMap& operator=(const StreamingMap&)=delete;
    ~StreamingMap(){if(pending)task.wait();} // release() obligatorio antes de destruir el contexto GL.
    const StreamSettings& settings()const{return cfg;}
    const StreamStats& statistics()const{return stats;}
    int preparationPhase()const{return phase.load();}
    void cancelPreparation(){cancelled=true;}
    // Llamar una vez en un trabajador y esperar su future antes de Draw/release.
    void prepare(const std::string& filename) {
        if(cfg.cellSize<=0||!cfg.maxTrianglesPerPage||!cfg.maxTextureSize)throw std::runtime_error("Ajustes de cache invalidos");
        source=fs::absolute(fs::u8path(filename)).lexically_normal();
        if(!fs::exists(source))throw std::runtime_error("No existe "+filename);
        phase=1;
        // Firma de glTF y buffers externos en su carpeta. Texturas tienen firma propia.
        std::vector<std::string> inputs{stamp(source)};
        for(const auto& f:fs::directory_iterator(source.parent_path()))if(f.is_regular_file()&&f.path().extension()==".bin")inputs.push_back(stamp(f.path()));
        std::sort(inputs.begin(),inputs.end());std::uint64_t h=1469598103934665603ull;
        for(const auto& s:inputs)h=hashText(h,s);
        h=hashText(h,std::to_string(cfg.cellSize)+":"+std::to_string(cfg.maxTrianglesPerPage));
        h=hashText(h,"packed-layout-4-exact-ground-bvh-foundation");
        for(const auto& item:cfg.excludedBaseTextures)h=hashText(h,item);
        for(const auto& item:cfg.excludedNodeNames)h=hashText(h,item);
        for(const auto& item:cfg.terrainExcludedPrefixes)h=hashText(h,item);
        for(const auto& item:cfg.foundationNodeNames)h=hashText(h,item);
        h=hashText(h,std::to_string(cfg.terrainScale)+":"+std::to_string(cfg.terrainOffset.x)+":"+
            std::to_string(cfg.terrainOffset.y)+":"+std::to_string(cfg.terrainOffset.z));
        std::string sig=hexName(h);cache=source.parent_path()/".uranden_cache_v3"/(source.stem().string()+"_"+sig);
        fs::create_directories(cache);
        if(readIndex(sig)&&terrain.load(cache/"ground.bin")){phase=4;initialized=true;return;}
        terrain.clear();pages.clear();materials.clear();textures.clear();
        canonicalMaterial.clear();excludedMaterial.clear();excludedMeshes=excludedTriangles=0;
        std::error_code ignored;fs::remove(cache/"index.txt",ignored);
        std::cout<<"Preparando cache de mapa: importacion inicial, sin subir todo a GPU.\n";
        {
            Assimp::Importer importer;
            const aiScene* scene=importer.ReadFile(source.u8string(),aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_TransformUVCoords);
            if(!scene||!scene->mRootNode||(scene->mFlags&AI_SCENE_FLAGS_INCOMPLETE))throw std::runtime_error(importer.GetErrorString());
            checkCancel();phase=2;
            if(scene->mNumAnimations)std::cerr<<"AVISO: se usa la pose estatica del mapa; animaciones deben cargarse aparte.\n";
            makeMaterials(scene);walk(scene->mRootNode,glm::mat4(1),scene);phase=3;
        } // Libera scene/Assimp ANTES del recorrido.
        if(pages.empty())throw std::runtime_error("El mapa no contiene triangulos");
        std::cout<<"Excluidos: "<<excludedMeshes<<" mallas/instancias, "<<excludedTriangles<<" triangulos.\n";
        packPages();terrain.build();terrain.save(cache/"ground.bin");saveIndex(sig);initialized=true;phase=4;
        std::cout<<"Cache lista: "<<pages.size()<<" paginas, "<<textures.size()<<" texturas disponibles bajo demanda.\n";
    }
    void draw(GLuint shader,const glm::mat4& model,const glm::mat4& view,const glm::mat4& projection,
              const glm::vec3& eye,int height,double now) {
        if(!initialized)return;
        stats={};stats.total=pages.size();
        auto frustum=planes(projection*view*model);
        glm::vec3 localEye=glm::vec3(glm::inverse(model)*glm::vec4(eye,1));
        float scale=glm::length(glm::vec3(model[0])); // Main usa escala uniforme.
        auto& order=drawOrder;order.clear();order.reserve(pages.size());
        for(auto& t:textures)t.pinned=false;
        for(std::size_t i=0;i<pages.size();++i) {
            auto& p=pages[i];p.distance=distanceToBox(p.box,localEye)*scale;
            float radius=glm::length(p.box.hi-p.box.lo)*0.5f*scale;
            float pixels=radius*projection[1][1]*height/(std::max)(p.distance,0.01f);
            p.visible=p.distance<cfg.drawDistance&&inFrustum(p.box,frustum)&&pixels>=cfg.minPixels;
            p.preloadWanted=p.distance<cfg.preloadDistance;
            if(p.visible){++stats.visible;p.lastUse=now;}
            if(p.vao&&(p.distance>cfg.unloadDistance||(now-p.lastUse>8&&!p.preloadWanted)))unload(p);
            if(p.preloadWanted&&!p.failed)order.push_back(i);
            const auto& m=materials[p.material];
            if(m.texture>=0&&p.vao&&p.visible){auto& t=textures[m.texture];t.pinned=true;t.lastUse=now;}
        }
        std::sort(order.begin(),order.end(),[this](std::size_t a,std::size_t b){
            if(pages[a].visible!=pages[b].visible)return pages[a].visible>pages[b].visible;
            return pages[a].distance<pages[b].distance;
        });
        for(auto& t:textures)if(t.id&&!t.pinned&&now-t.lastUse>8)unload(t);
        if(pending&&task.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            Result r=task.get();pending=false;
            if(!r.error.empty()) {
                std::cerr<<"STREAM: "<<r.error<<'\n';
                if(r.texture)textures[r.index].failed=true;else pages[r.index].failed=true;
            }else upload(r,now);
        }
        // Una tarea / subida por cuadro. Prioriza texturas de geometria visible.
        if(!pending) {
            for(std::size_t i:order) {
                auto& p=pages[i];const auto& m=materials[p.material];
                if(p.vao&&p.visible&&m.texture>=0) {
                    auto& t=textures[m.texture];
                    std::size_t estimate=std::size_t(cfg.maxTextureSize)*cfg.maxTextureSize*4*4/3+64;
                    if(!t.id&&!t.failed) {
                        if(roomTexture(estimate)){t.pinned=true;t.lastUse=now;launch(true,m.texture);break;}
                        stats.budgetLimited=true;
                    }
                }
                if(!p.vao) {
                    // No expulsar paginas cercanas para precargar otras: evita thrashing.
                    bool room=p.visible?roomGeometry(p.bytes()):geoBytes+p.bytes()<=cfg.geometryBudget;
                    if(room){launch(false,i);break;}stats.budgetLimited=true;
                }
            }
        }
        if(program!=shader) {
            program=shader;
            uTexture=glGetUniformLocation(shader,"texture_diffuse1");uUse=glGetUniformLocation(shader,"useTexture");
            uColor=glGetUniformLocation(shader,"objectColor");uAlpha=glGetUniformLocation(shader,"materialAlpha");
            uMode=glGetUniformLocation(shader,"mapAlphaMode");uCutoff=glGetUniformLocation(shader,"mapAlphaCutoff");
            uMap=glGetUniformLocation(shader,"streamMap");uFogStart=glGetUniformLocation(shader,"streamFogStart");uFogEnd=glGetUniformLocation(shader,"streamFogEnd");
        }
        glUniform1i(uMap,1);glUniform1f(uFogStart,cfg.fogStart);glUniform1f(uFogEnd,cfg.drawDistance);
        glUniform1i(uTexture,0);glActiveTexture(GL_TEXTURE0);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
        auto& opaque=opaqueOrder;auto& transparent=transparentOrder;
        opaque.clear();transparent.clear();
        opaque.reserve(order.size());transparent.reserve(order.size());
        for(std::size_t i:order)if(pages[i].visible&&pages[i].vao) {
            (materials[pages[i].material].alpha==2?transparent:opaque).push_back(i);
        }
        std::stable_sort(opaque.begin(),opaque.end(),[this](std::size_t a,std::size_t b){return pages[a].material<pages[b].material;});
        std::sort(transparent.begin(),transparent.end(),[this](std::size_t a,std::size_t b){return pages[a].distance>pages[b].distance;});
        auto drawList=[&](const std::vector<std::size_t>& list) {
            int previous=-1;
            for(std::size_t i:list) {
                auto& p=pages[i];auto& m=materials[p.material];
                if(previous!=int(p.material)) {
                    previous=int(p.material);GLuint tex=m.texture>=0?textures[m.texture].id:0;
                    glBindTexture(GL_TEXTURE_2D,tex);glUniform1i(uUse,tex?1:0);
                    if(tex){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,m.wrapS);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,m.wrapT);}
                    glUniform3fv(uColor,1,glm::value_ptr(m.color));glUniform1f(uAlpha,m.color.a);
                    glUniform1i(uMode,m.alpha);glUniform1f(uCutoff,m.cutoff);
                }
                glBindVertexArray(p.vao);glDrawElements(GL_TRIANGLES,p.ni,GL_UNSIGNED_INT,nullptr);
                ++stats.drawn;stats.triangles+=p.ni/3;
            }
        };
        drawList(opaque);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawList(transparent);
        glDepthMask(GL_TRUE);glBindVertexArray(0);glUniform1i(uMap,0);glActiveTexture(GL_TEXTURE0);
        for(const auto& p:pages)if(p.vao)++stats.resident;
        stats.geometryBytes=geoBytes;stats.textureBytes=texBytes;stats.loading=pending;
    }
    const TerrainSurface& collisionSurface() const {return terrain;}
    bool terrainHeight(float x,float z,float& y) const {return terrain.sample(x,z,y);}
    void release() {
        if(pending){task.wait();task.get();pending=false;}
        for(auto& p:pages)unload(p);
        for(auto& t:textures)unload(t);
        initialized=false;
    }
};
} // namespace uranden
