// GraphicsData.h
#pragma once
#include "MathUtility.h"
#include <cstdint>
#include <string>
#include <vector>

// 頂点データ構造体
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

// マテリアル構造体
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};

// ワールド変換行列構造体
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

// 平行光源構造体
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

// パーティクル用のフラグメント構造体
struct Fragment {
    Vector3 position;
    Vector3 velocity;
    Vector3 rotation;
    Vector3 rotationSpeed;
    float alpha;
    bool active;
};

// モデルのマテリアルデータ
struct MaterialData {
    std::string textureFilePath;
};

// モデルデータ
struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

// グローバル定数
const int kSubdivision = 16; // 球の分割数
const int kNumSphereVertices = kSubdivision * kSubdivision * 6; // 球の頂点数

// 列挙体
enum class WaveType {
    SINE,
    CHAINSAW,
    SQUARE,
};

enum class AnimationType {
    RESET,
    NONE,
    COLOR,
    SCALE,
    ROTATE,
    TRANSLATE,
    TORNADO,
    PULSE,
    AURORA,
    BOUNCE,
    TWIST,
    ALL
};