// Model.h
#pragma once
#include "GraphicsData.h"
#include <string>
#include <vector>

class Model {
public:
    // .objファイルからのモデル読み込み
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private:
    // .mtlファイルからのマテリアル読み込み（LoadObjFileから内部的に呼ばれる）
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
};


class GeometryGenerator {
public:
    // 球の頂点データ生成
    static void GenerateSphereVertices(std::vector<VertexData>& vertices, int subdivision, float radius);
};