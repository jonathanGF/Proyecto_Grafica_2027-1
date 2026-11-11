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

    unsigned int TextureFromFile(const char* path, const std::string& directory);

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
            if (mesh->mMaterialIndex >= 0) {
                aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

                aiColor4D materialColor(1.0f, 1.0f, 1.0f, 1.0f);
                if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &materialColor) != AI_SUCCESS) {
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
                    aiTextureType_DIFFUSE,
                    "texture_diffuse"
                );

                if (diffuseMaps.empty()) {
                    diffuseMaps = loadMaterialTextures(
                        material,
                        aiTextureType_DIFFUSE,
                        "texture_diffuse"
                    );
                }
                textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

                // Mapas especulares
                std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
                textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
            }

            return Mesh(vertices, indices, textures, baseColorFactor);
        }

        std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
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
                    texture.id = TextureFromFile(str.C_Str(), this->directory);
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

    inline unsigned int TextureFromFile(const char* path, const std::string& directory) {
        std::string filename = DecodeTextureURI(std::string(path));
        filename = directory + '/' + filename;

        GLuint textureID = SOIL_load_OGL_texture(
            filename.c_str(),
            SOIL_LOAD_AUTO,
            SOIL_CREATE_NEW_ID,
            SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y
        );

        if (textureID == 0) {
            std::cout << "ERROR::SOIL2::Fallo al cargar textura en ruta: " << filename << std::endl;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return textureID;
    }
