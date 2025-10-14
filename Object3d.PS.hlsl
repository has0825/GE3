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
    
    // UV���W�𓯎����W�n�Ɋg�����āix, y, 0.0, 1.0�j�A�A�t�B���ϊ���K�p����
    // float32_t4(input.texcoord, 0.0f, 1.0f) �� mul ���邱�Ƃ� transformedUV.xy �ɕϊ����UV������
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // �ϊ����UV���W���g���ăe�N�X�`������F���T���v�����O����
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    
    // === ������ �������炪�ύX�ӏ� ������ ===
    // �e�N�X�`����RGB�l�����ׂ�0.1f�����i�قڍ��j�̏ꍇ�A���̃s�N�Z����`�悹���ɔj������
    // ����ɂ��A�������������߂��Ă���悤�Ɍ�����
    // ����臒l(0.1f)�́A�K�v�ɉ����Ē������Ă�������
    if (textureColor.r < 0.1f && textureColor.g < 0.1f && textureColor.b < 0.1f)
    {
        discard;
    }
    // === ������ �����܂ł��ύX�ӏ� ������ ===


    if (gMaterial.enableLighting != 0)//Lighting����ꍇ
    {
        // Half Lambert �̌v�Z
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // �� �X���C�h�̎w���Ɋ�Â��ARGB��Alpha�𕪗����Čv�Z ��
        // RGB (�F) �ɂ̓��C�e�B���O��K�p
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        // Alpha (���ߓx) �ɂ̓��C�e�B���O��K�p���Ȃ��iTexture��Material�ɂ���Ă̂ݐ���j
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    { //Lighting���Ȃ��ꍇ�i�ȑO�Ɠ����v�Z�j
        // �A���t�@�l���܂߂ăe�N�X�`���ƃ}�e���A���J���[����Z
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}