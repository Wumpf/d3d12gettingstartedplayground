struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput VSMain(float2 position : POSITION, float4 color : COLOR, uint instance : SV_InstanceID)
{
	PSInput result;

	float2 displace = float2(0, 0);
	displace.x += (instance % 10) * 0.2f;
	displace.y += (instance / 10) * 0.2f;

	result.position = float4(position.x + displace.x, position.y + displace.y, 0.0f, 1.0f);
	result.color = color;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return input.color;
}
