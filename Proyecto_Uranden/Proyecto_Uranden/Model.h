    #pragma once

    #include <GL/glew.h>
    #include <glm/glm.hpp>
    #include <glm/gtc/matrix_transform.hpp>
    #include "SOIL2/SOIL2.h"

    #include <assimp/Importer.hpp>
    #include <assimp/scene.h>
    #include <assimp/postprocess.h>

    #include "Mesh.h"
    #include "Shader.h"

    #include <string>
    #include <fstream>
    #include <sstream>
    #include <iostream>
    #include <vector>
    #include <cstring>
    #include <algorithm>
    #include <memory>
    #include <cstdint>

    inline unsigned int TextureFromFile(const char* path, const std::string& directory);
    inline unsigned int TextureFromEmbedded(const aiTexture* texture);

    class Model {
    public:
        std::vector<Texture> textures_loaded;
        std::vector<Mesh>    meshes;
        std::string directory;

        Model(std::string const& path) {
            loadModel(path);
        }

        void Draw(Shader& shader) {
            for (unsigned int i = 0; i < meshes.size(); i++)
                meshes[i].Draw(shader);
        }

    private:
        void loadModel(std::string const& path) {
            Assimp::Importer importer;
            // aiProcess_Triangulate: transforma cuadriláteros o polígonos en triángulos
            // PreTransformVertices aplica las posiciones, rotaciones y escalas de
            // los nodos glTF directamente sobre sus vertices. Esto evita que las
            // piezas del mapa aparezcan amontonadas en el origen.
            const aiScene* scene = importer.ReadFile(
                path,
                aiProcess_Triangulate |
                aiProcess_GenSmoothNormals |
                aiProcess_TransformUVCoords |
                aiProcess_PreTransformVertices
            );

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
                std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
                return;
            }
            directory = path.substr(0, path.find_last_of('/'));

            processNode(scene->mRootNode, scene);
        }

        void processNode(aiNode* node, const aiScene* scene) {
            // Procesar todas las mallas del nodo actual
            for (unsigned int i = 0; i < node->mNumMeshes; i++) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(processMesh(mesh, scene));
            }
            // Procesar los nodos hijos de manera recursiva
            for (unsigned int i = 0; i < node->mNumChildren; i++) {
                processNode(node->mChildren[i], scene);
            }
        }

        Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;
            std::vector<Texture> textures;

            // 1. Procesar vértices
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                Vertex vertex;
                glm::vec3 vector;

                // Posiciones
                vector.x = mesh->mVertices[i].x;
                vector.y = mesh->mVertices[i].y;
                vector.z = mesh->mVertices[i].z;
                vertex.Position = vector;

                // Normales
                if (mesh->HasNormals()) {
                    vector.x = mesh->mNormals[i].x;
                    vector.y = mesh->mNormals[i].y;
                    vector.z = mesh->mNormals[i].z;
                    vertex.Normal = vector;
                }
                else {
                    vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                // Coordenadas de textura
                if (mesh->mTextureCoords[0]) {
                    glm::vec2 vec;
                    vec.x = mesh->mTextureCoords[0][i].x;
                    vec.y = mesh->mTextureCoords[0][i].y;
                    vertex.TexCoords = vec;
                }
                else {
                    vertex.TexCoords = glm::vec2(0.0f, 0.0f);
                }

                // Colores pintados sobre los vértices en Blender.
                if (mesh->HasVertexColors(0)) {
                    const aiColor4D& color = mesh->mColors[0][i];
                    vertex.Color = glm::vec4(color.r, color.g, color.b, color.a);
                }
                else {
                    vertex.Color = glm::vec4(1.0f);
                }

                vertices.push_back(vertex);
            }

            // 2. Procesar índices
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                    indices.push_back(face.mIndices[j]);
            }

            // 3. Procesar materiales y texturas asociadas
            glm::vec4 baseColorFactor(1.0f);
            if (mesh->mMaterialIndex < scene->mNumMaterials) {
                aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

                aiColor4D materialColor(1.0f, 1.0f, 1.0f, 1.0f);
                if (aiGetMaterialColor(material, "$clr.base", 0, 0, &materialColor) != AI_SUCCESS) {
                    aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &materialColor);
                }

                float opacity = materialColor.a;
                material->Get(AI_MATKEY_OPACITY, opacity);
                baseColorFactor = glm::vec4(
                    materialColor.r,
                    materialColor.g,
                    materialColor.b,
                    opacity
                );

                // glTF almacena el color principal como BASE_COLOR. Si el modelo
                // es OBJ u otro formato tradicional, se conserva el soporte para
                // las texturas DIFFUSE que ya utilizaba el proyecto.
                std::vector<Texture> diffuseMaps = loadMaterialTextures(
                    material,
                    static_cast<aiTextureType>(12),
                    "texture_diffuse", scene
                );

                if (diffuseMaps.empty()) {
                    diffuseMaps = loadMaterialTextures(
                        material,
                        aiTextureType_DIFFUSE,
                        "texture_diffuse", scene
                    );
                }
                textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

                // Mapas especulares
                std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
                textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
            }

            return Mesh(vertices, indices, textures, baseColorFactor);
        }

        std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene) {
            std::vector<Texture> textures;
            for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
                aiString str;
                mat->GetTexture(type, i, &str);

                // Ignorar ranuras de textura vacías del GLTF
                if (str.length == 0) {
                    continue;
                }
                bool skip = false;
                for (unsigned int j = 0; j < textures_loaded.size(); j++) {
                    if (std::strcmp(textures_loaded[j].path.c_str(), str.C_Str()) == 0) {
                        Texture reusedTexture = textures_loaded[j];
                        reusedTexture.type = typeName;
                        textures.push_back(reusedTexture);
                        skip = true;
                        break;
                    }
                }

                if (!skip) {
                    Texture texture;
                    const aiTexture* embedded=scene->GetEmbeddedTexture(str.C_Str());
                    texture.id=embedded?TextureFromEmbedded(embedded):TextureFromFile(str.C_Str(),this->directory);
                    texture.type = typeName;
                    texture.path = str.C_Str();
                    textures.push_back(texture);
                    textures_loaded.push_back(texture);
                }
            }
            return textures;
        }
    };

    inline int HexDigitValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    inline std::string DecodeTextureURI(const std::string& uri) {
        std::string decoded;
        decoded.reserve(uri.size());

        for (std::size_t i = 0; i < uri.size(); ++i) {
            if (uri[i] == '%' && i + 2 < uri.size()) {
                int high = HexDigitValue(uri[i + 1]);
                int low = HexDigitValue(uri[i + 2]);
                if (high >= 0 && low >= 0) {
                    decoded.push_back(static_cast<char>((high << 4) | low));
                    i += 2;
                    continue;
                }
            }
            decoded.push_back(uri[i]);
        }

        return decoded;
    }

    // Texturas de los actores: soporta imagenes externas y embebidas en GLB.
    // Limita las copias en GPU a 2048 px; no modifica sus archivos originales.
    inline GLuint UploadModelPixels(const unsigned char* raw,int w,int h) {
        if(!raw||w<=0||h<=0)return 0;
        const double factor=(std::min)(1.0,2048.0/(std::max)(w,h));
        const int nw=(std::max)(1,int(w*factor)),nh=(std::max)(1,int(h*factor));
        std::vector<unsigned char> out(std::size_t(nw)*nh*4);
        for(int y=0;y<nh;++y)for(int x=0;x<nw;++x) {
            int y0=int(std::int64_t(y)*h/nh),y1=int(std::int64_t(y+1)*h/nh);
            int x0=int(std::int64_t(x)*w/nw),x1=int(std::int64_t(x+1)*w/nw);
            std::uint64_t sum[4]={0,0,0,0};
            for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)
                for(int c=0;c<4;++c)sum[c]+=raw[(std::size_t(yy)*w+xx)*4+c];
            for(int c=0;c<4;++c)out[(std::size_t(nh-1-y)*nw+x)*4+c]=static_cast<unsigned char>(sum[c]/((y1-y0)*(x1-x0)));
        }
        GLuint id=0;glGenTextures(1,&id);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,id);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,nw,nh,0,GL_RGBA,GL_UNSIGNED_BYTE,out.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        return id;
    }
    inline unsigned int TextureFromFile(const char* path,const std::string& directory) {
        std::string filename=directory+"/"+DecodeTextureURI(path);
        int w=0,h=0,c=0;unsigned char* data=SOIL_load_image(filename.c_str(),&w,&h,&c,SOIL_LOAD_RGBA);
        if(!data){std::cerr<<"TEXTURA NO ENCONTRADA: "<<filename<<'\n';return 0;}
        GLuint id=UploadModelPixels(data,w,h);SOIL_free_image_data(data);return id;
    }
    inline unsigned int TextureFromEmbedded(const aiTexture* texture) {
        if(texture->mHeight==0) {
            int w=0,h=0,c=0;
            unsigned char* data=SOIL_load_image_from_memory(reinterpret_cast<const unsigned char*>(texture->pcData),
                int(texture->mWidth),&w,&h,&c,SOIL_LOAD_RGBA);
            if(!data){std::cerr<<"No se pudo decodificar la imagen embebida del GLB\n";return 0;}
            GLuint id=UploadModelPixels(data,w,h);SOIL_free_image_data(data);return id;
        }
        std::vector<unsigned char> rgba(std::size_t(texture->mWidth)*texture->mHeight*4);
        for(std::size_t i=0;i<rgba.size()/4;++i){auto p=texture->pcData[i];rgba[i*4]=p.r;rgba[i*4+1]=p.g;rgba[i*4+2]=p.b;rgba[i*4+3]=p.a;}
        return UploadModelPixels(rgba.data(),int(texture->mWidth),int(texture->mHeight));
    }
