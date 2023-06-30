#pragma once

#include "Geometry/Model.hpp"
#include "Texture/Texture2D.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include <assimp/types.h>
#include <glm/glm.hpp>
#include <intrin0.inl.h>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "Mesh.hpp"

using namespace spdlog;

namespace soft {

    class Model {
    public:
        Model(std::string_view path)
        {
            LoadModel(path);

            debug("Load model {} complete", path);
        }

        auto& GetMeshes()
        {
            return m_Meshes;
        }

    private:
        void LoadModel(std::string_view path)
        {
            // read file via ASSIMP
            Assimp::Importer importer;
            const aiScene*   scene =
                importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                                   aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
            // check for errors
            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
                !scene->mRootNode)  // if is Not Zero
            {
                spdlog::error("ERROR::ASSIMP:: {}", importer.GetErrorString());
                return;
            }

            m_Directory = path.substr(0, path.find_last_of('\\'));
            // process ASSIMP's root node recursively
            ProcessNode(scene->mRootNode, scene);
        }

        // processes a node in a recursive fashion. Processes each individual
        // mesh located at the node and repeats this process on its children
        // nodes (if any).
        void ProcessNode(aiNode* node, const aiScene* scene)
        {
            // process each mesh located at the current node
            for (unsigned int i = 0; i < node->mNumMeshes; i++) {
                // the node object only contains indices to index the actual
                // objects in the scene. the scene contains all the data, node
                // is just to keep stuff organized (like relations between
                // nodes).
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                m_Meshes.push_back(ProcessMesh(mesh, scene));
            }
            // after we've processed all of the meshes (if any) we then
            // recursively process each of the children nodes
            for (unsigned int i = 0; i < node->mNumChildren; i++) {
                ProcessNode(node->mChildren[i], scene);
            }
        }

        Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene)
        {
            // data to fill
            std::vector<Vertex>    vertices;
            std::vector<uint32_t>  indices;
            std::vector<Texture2D> textures;

            // walk through each of the mesh's vertices
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                Vertex    vertex;
                glm::vec3 vector;
                // we declare a placeholder vector since assimp
                // uses its own vector class that doesn't directly
                // convert to glm's vec3 class so we transfer the
                // data to this placeholder glm::vec3 first.
                // positions
                vector.x        = mesh->mVertices[i].x;
                vector.y        = mesh->mVertices[i].y;
                vector.z        = mesh->mVertices[i].z;
                vertex.position = vector;
                // normals
                if (mesh->HasNormals()) {
                    vector.x      = mesh->mNormals[i].x;
                    vector.y      = mesh->mNormals[i].y;
                    vector.z      = mesh->mNormals[i].z;
                    vertex.normal = vector;
                }
                // texture coordinates
                // does the mesh contain texture coordinates?
                if (mesh->mTextureCoords[0]) {
                    glm::vec2 vec;
                    // a vertex can contain up to 8 different texture
                    // coordinates. We thus make the assumption that we
                    // won't use models where a vertex can have multiple
                    // texture coordinates so we always take the first set (0).
                    vec.x           = mesh->mTextureCoords[0][i].x;
                    vec.y           = mesh->mTextureCoords[0][i].y;
                    vertex.texCoord = vec;
                    // tangent
                    vector.x       = mesh->mTangents[i].x;
                    vector.y       = mesh->mTangents[i].y;
                    vector.z       = mesh->mTangents[i].z;
                    vertex.tangent = vector;
                    // bitangent
                    vector.x         = mesh->mBitangents[i].x;
                    vector.y         = mesh->mBitangents[i].y;
                    vector.z         = mesh->mBitangents[i].z;
                    vertex.bitangent = vector;
                }
                else
                    vertex.texCoord = glm::vec2(0.0f, 0.0f);

                vertices.push_back(vertex);
            }
            // now wak through each of the mesh's faces (a face is a mesh its
            // triangle) and retrieve the corresponding vertex indices.
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];
                // retrieve all indices of the face and store them in the
                // indices vector
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                    indices.push_back(face.mIndices[j]);
            }
            // process materials
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            // we assume a convention for sampler names in the shaders. Each
            // diffuse texture should be named as 'texture_diffuseN' where N is
            // a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
            // Same applies to other texture as the following list
            // summarizes: diffuse: texture_diffuseN specular:
            // texture_specularN normal: texture_normalN

            // 1. diffuse maps
            std::vector<Texture2D> diffuseMaps =
                LoadMaterialTextures(material, aiTextureType_DIFFUSE, "Diffuse Map", scene);
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
            // 2. specular maps
            std::vector<Texture2D> specularMaps =
                LoadMaterialTextures(material, aiTextureType_SPECULAR, "Specular Map", scene);
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
            // 3. normal maps
            std::vector<Texture2D> normalMaps =
                LoadMaterialTextures(material, aiTextureType_HEIGHT, "Normal Map", scene);
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
            // 4. height maps
            std::vector<Texture2D> heightMaps =
                LoadMaterialTextures(material, aiTextureType_AMBIENT, "Height Map", scene);
            textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

            // return a mesh object created from the extracted mesh data
            return Mesh(std::move(vertices), std::move(indices), std::move(textures));
        }

        // checks all material textures of a given type and loads the textures
        // if they're not loaded yet. the required info is returned as a Texture struct.
        std::vector<Texture2D> LoadMaterialTextures(aiMaterial*    mat,
                                                    aiTextureType  type,
                                                    std::string    typeName,
                                                    const aiScene* scene)
        {
            std::vector<Texture2D> textures;
            for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
                aiString str;
                mat->GetTexture(type, i, &str);

                auto      tex = scene->GetEmbeddedTexture(str.C_Str());
                Texture2D texture;
                if (tex != nullptr) {
                    texture.LoadTextureFromMemory(tex);
                }
                else {
                    // To prevent temporary object been constructed
                    // use emplace_back here instead of push_back(Texture(str.C_Str())))
                    auto textureFilename = m_Directory + "\\" + std::string(str.C_Str());
                    texture.LoadTextureData(textureFilename);
                }
                textures.push_back(texture);
            }

            return textures;
        }

    private:
        std::vector<Mesh> m_Meshes;
        std::string       m_Directory;
    };
}  // namespace soft