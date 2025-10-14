#include "object3d.hlsli"


ConstantBuffer<Material> gMaterial : register(b0);


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV座標を同次座標系に拡張して（x, y, 0.0, 1.0）、アフィン変換を適用する
    // float32_t4(input.texcoord, 0.0f, 1.0f) を mul することで transformedUV.xy に変換後のUVが入る
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // 変換後のUV座標を使ってテクスチャから色をサンプリングする
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    
    if (gMaterial.enableLighting != 0)//Lightingする場合
    {
        // Half Lambert の計算
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // ★ スライドの指示に基づき、RGBとAlphaを分離して計算 ★
        // RGB (色) にはライティングを適用
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        // Alpha (透過度) にはライティングを適用しない（TextureとMaterialによってのみ制御）
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    { //Lightingしない場合（以前と同じ計算）
        // アルファ値を含めてテクスチャとマテリアルカラーを乗算
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}