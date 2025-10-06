// Model.cpp
#include "Model.h"
#include <fstream>
#include <sstream>
#include <cassert>

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    ModelData modelData;
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.x *= -1.0f; // 左手座標系に
            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y; // UVのV座標を反転
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f; // 左手座標系に
            normals.push_back(normal);
        } else if (identifier == "f") {
            VertexData triangle[3];
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(v, index, '/');
                    elementIndices[element] = std::stoi(index);
                }
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];
                triangle[faceVertex] = { position, texcoord, normal };
            }
            // 逆順に格納
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);
        } else if (identifier == "mtllib") {
            std::string materialFilename;
            s >> materialFilename;
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }
    return modelData;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    MaterialData materialData;
    std::string line;
    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    return materialData;
}


void GeometryGenerator::GenerateSphereVertices(std::vector<VertexData>& vertices, int subdivision, float radius) {
    vertices.resize(subdivision * subdivision * 6);

    const float lonEvery = static_cast<float>(M_PI * 2.0f) / subdivision;
    const float latEvery = static_cast<float>(M_PI) / subdivision;

    for (int latIndex = 0; latIndex < subdivision; ++latIndex) {
        float lat = -static_cast<float>(M_PI) / 2.0f + latEvery * latIndex;
        for (int lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
            float lon = lonEvery * lonIndex;

            // 4つの頂点を計算
            Vector3 pA = { radius * cosf(lat) * cosf(lon), radius * sinf(lat), radius * cosf(lat) * sinf(lon) };
            Vector3 pB = { radius * cosf(lat + latEvery) * cosf(lon), radius * sinf(lat + latEvery), radius * cosf(lat + latEvery) * sinf(lon) };
            Vector3 pC = { radius * cosf(lat) * cosf(lon + lonEvery), radius * sinf(lat), radius * cosf(lat) * sinf(lon + lonEvery) };
            Vector3 pD = { radius * cosf(lat + latEvery) * cosf(lon + lonEvery), radius * sinf(lat + latEvery), radius * cosf(lat + latEvery) * sinf(lon + lonEvery) };

            Vector2 uvA = { float(lonIndex) / subdivision, 1.0f - float(latIndex) / subdivision };
            Vector2 uvB = { float(lonIndex) / subdivision, 1.0f - float(latIndex + 1) / subdivision };
            Vector2 uvC = { float(lonIndex + 1) / subdivision, 1.0f - float(latIndex) / subdivision };
            Vector2 uvD = { float(lonIndex + 1) / subdivision, 1.0f - float(latIndex + 1) / subdivision };

            uint32_t startIndex = (latIndex * subdivision + lonIndex) * 6;

            // 1つ目の三角形
            vertices[startIndex] = { {pA.x, pA.y, pA.z, 1.0f}, uvA, MathUtility::Normalize(pA) };
            vertices[startIndex + 1] = { {pB.x, pB.y, pB.z, 1.0f}, uvB, MathUtility::Normalize(pB) };
            vertices[startIndex + 2] = { {pC.x, pC.y, pC.z, 1.0f}, uvC, MathUtility::Normalize(pC) };
            // 2つ目の三角形
            vertices[startIndex + 3] = { {pC.x, pC.y, pC.z, 1.0f}, uvC, MathUtility::Normalize(pC) };
            vertices[startIndex + 4] = { {pB.x, pB.y, pB.z, 1.0f}, uvB, MathUtility::Normalize(pB) };
            vertices[startIndex + 5] = { {pD.x, pD.y, pD.z, 1.0f}, uvD, MathUtility::Normalize(pD) };
        }
    }
}