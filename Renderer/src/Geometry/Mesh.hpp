#pragma once

#include "Texture/Texture2D.hpp"
#include "glm/fwd.hpp"

#include <array>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <stdint.h>
#include <tiny_obj_loader.h>
#include <vector>

namespace soft {
    struct Vertex
    {
        glm::vec3 position  = {0, 0, 0};
        glm::vec3 normal    = {0, 0, 0};
        glm::vec3 tangent   = {0, 0, 0};
        glm::vec3 bitangent = {0, 0, 0};
        glm::vec2 texCoord  = {0, 0};
        glm::vec3 color     = {1, 1, 1};

        bool operator==(const Vertex& other) const
        {
            return position == other.position && color == other.color && texCoord == other.texCoord;
        }

        Vertex() = default;
        Vertex(const vec3& p) : position(p) {}
    };

    enum class PrimitiveType { Triangle, Line, Line_Strip, Point };

    using Face      = std::array<Vertex, 3>;
    using Primitive = std::array<Vertex, 3>;

    using color  = glm::vec3;
    using point3 = glm::vec3;
    using point2 = glm::vec2;

    struct Mesh
    {
        Mesh() = default;

        Mesh(std::vector<Vertex>& vs, std::vector<uint32_t>& ids, std::vector<Texture2D>& texs)
        {
            vertices = vs;
            indices  = ids;
            textures = texs;
        }

        Mesh(std::vector<Vertex>&& vs, std::vector<uint32_t>&& ids, std::vector<Texture2D>&& texs)
        {
            vertices = vs;
            indices  = ids;
            textures = texs;
        }

        // void LoadOBJMesh(std::string_view path);
        // void LoadTexture(std::string_view path);
        // void LoadModel(std::string_view path);

        std::vector<Vertex>    vertices;
        std::vector<uint32_t>  indices;
        std::vector<Texture2D> textures;
    };

}  // namespace soft

namespace std {
    template <>
    struct hash<soft::Vertex>
    {
        size_t operator()(soft::Vertex const& vertex) const
        {
            return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}  // namespace std
