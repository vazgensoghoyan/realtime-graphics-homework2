#pragma once

#include <math/aliases.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ObjVertex {
    math::vector3f position;
    math::vector3f normal;
    math::vector2f texcoord;
};

struct ObjMaterial {
    std::string name;
    std::filesystem::path albedoTexture;
    std::filesystem::path transparencyTexture;
    math::vector3f specular{0.f, 0.f, 0.f};
    float specularPower = 1.f;
};

struct ObjSubmesh {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t materialIndex = 0;
};

struct ObjMesh {
    std::vector<ObjVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<ObjMaterial> materials;
    std::vector<ObjSubmesh> submeshes;
    math::box3f bounds = math::box3f::null();
};

ObjMesh loadObj(std::filesystem::path const & path);
