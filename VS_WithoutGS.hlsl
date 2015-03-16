
uniform row_major float4x4 g_ViewProj;

float4 main( 
    float4 v : POSITION, 
    float4 R0 : XFORM0,  // R0,R1,R2 are a 3x4 transform matrix
    float4 R1 : XFORM1, 
    float4 R2 : XFORM2 ) : SV_Position
{
    // deform unit box into desired oriented box
    float3 vPosWS = float3( dot(v,R0), dot(v,R1), dot(v,R2) );
   
    // clip-space transform
    return mul( float4(vPosWS,1), g_ViewProj );
}