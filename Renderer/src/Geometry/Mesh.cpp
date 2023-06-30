
// #define TINYOBJLOADER_IMPLEMENTATION
// #include "Geometry/Mesh.hpp"
// #include "Mesh.hpp"
// #include "Texture/Texture.hpp"

// #include <assimp/Importer.hpp>   // C++ importer interface
// #include <assimp/postprocess.h>  // Post processing flags
// #include <assimp/scene.h>        // Output data structure
// #include <memory>
// #include <spdlog/spdlog.h>

// namespace soft {
//     void Mesh::LoadOBJMesh(std::string_view path)
//     {
//         {
//             // std::string              inputfile = "cornell_box.obj";
//             tinyobj::ObjReaderConfig reader_config;
//             reader_config.mtl_search_path = "./";  // Path to material files

//             tinyobj::ObjReader reader;

//             if (!reader.ParseFromFile(path.data(), reader_config)) {
//                 if (!reader.Error().empty())
//                     spdlog::error("TinyObjReader: {}", reader.Error());

//                 exit(1);
//             }

//             if (!reader.Warning().empty())
//                 spdlog::warn("TinyObjReader: {}", reader.Warning());

//             auto& attrib    = reader.GetAttrib();
//             auto& shapes    = reader.GetShapes();
//             auto& materials = reader.GetMaterials();

//             // Loop over shapes
//             for (const auto& shape : shapes) {
//                 // Loop over faces(polygon)
//                 size_t index_offset = 0;
//                 for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
//                     size_t fv = size_t(shape.mesh.num_face_vertices[f]);

//                     // Loop over vertices in the face.
//                     Face face;
//                     for (size_t v = 0; v < fv; v++) {
//                         Vertex vert;
//                         // access to vertex
//                         tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
//                         tinyobj::real_t  vx  = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
//                         tinyobj::real_t  vy  = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
//                         tinyobj::real_t  vz  = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

//                         // Check if `normal_index` is zero or positive.
//                         // negative
//                         // = no normal data
//                         if (idx.normal_index >= 0) {
//                             tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
//                             tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
//                             tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
//                             vert.normal        = glm::vec3{nx, ny, nz};
//                         }

//                         // Check if `texcoord_index` is zero or positive.
//                         // negative = no texcoord data
//                         if (idx.texcoord_index >= 0) {
//                             tinyobj::real_t tx =
//                                 attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
//                             tinyobj::real_t ty =
//                                 attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
//                             vert.texCoord = {tx, ty};
//                         }

//                         // Optional: vertex colors
//                         tinyobj::real_t red   = attrib.colors[3 * size_t(idx.vertex_index) + 0];
//                         tinyobj::real_t green = attrib.colors[3 * size_t(idx.vertex_index) + 1];
//                         tinyobj::real_t blue  = attrib.colors[3 * size_t(idx.vertex_index) + 2];

//                         vert.color = {red, green, blue};

//                         vert.position = glm::vec3{vx, vy, vz};
//                         face[v]       = vert;
//                     }
//                     faces.push_back(face);
//                     index_offset += fv;

//                     // per-face material
//                     auto mat = shape.mesh.material_ids[f];
//                 }
//             }
//         }
//     }

//     // void Mesh::LoadTexture(std::string_view path)
//     // {
//     //     if (texture != nullptr)
//     //         texture->LoadTextureData(path);
//     //     else
//     //         texture = std::make_shared<Texture>(path);
//     // }

// }  // namespace soft
