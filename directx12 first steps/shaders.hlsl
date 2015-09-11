struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput VSMain(float2 position : POSITION, float4 color : COLOR, uint instance : SV_InstanceID)
{
	PSInput result;

	float2 displace = float2(-0.95f, -0.95f);
	displace.x += (instance % 50) * 0.1f;
	displace.y += (instance / 50) * 0.1f;

	result.position.xy = position * 0.1 + displace;
	result.position.zw = float2(0.0f, 1.0f);
	result.color = color;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return input.color;
}
