// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

Texture2D ObjTexture;
SamplerState ObjSamplerState;

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
	return ObjTexture.Sample(ObjSamplerState, input.texCoord);
}
