cbuffer Constants : register(b0)
{
	uint DrawID;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

PSInput VSMain(float2 position : POSITION, float2 texcoord : TEXCOORD)
{
	PSInput result;

	float2 displace = float2(-0.95f, -0.95f);
	displace.x += (DrawID % 20) * 0.1f;
	displace.y += (DrawID / 20) * 0.1f;

	result.position.xy = position * 0.1 + displace;
	result.position.zw = float2(0.0f, 1.0f);
	result.texcoord = texcoord;

	return result;
}


SamplerState defaultSampler : register(s0);
Texture2D colorTexture : register(t0);

float4 PSMain(PSInput input) : SV_TARGET
{
	return colorTexture.Sample(defaultSampler, input.texcoord);
}
