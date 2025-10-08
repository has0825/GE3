#pragma once
#include "MathTypes.h"
#include <string>
#include <vector>

// (他の構造体は変更なし)
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};
struct CameraForGpu {
	Vector3 worldPosition;
};

// ★修正点: color と intensity メンバを元に戻す
struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

// (以降の構造体は変更なし)
struct MaterialData {
	std::string textureFilePath;
};
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};