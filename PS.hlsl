

 row_major float4x4 g_RasterToWorld;

float4 main( float4 v : SV_Position ) : SV_Target
{
    float4 p = mul( float4(v.xyz,1), g_RasterToWorld);
    p.xyz/= p.w;
    float3 n = normalize( cross( ddx(p.xyz), ddy(p.xyz) ) );
    float3 L = normalize(float3(1,1,-1));
    float  d = saturate(dot(n,L));
    return float4(d.xxx,1);
}