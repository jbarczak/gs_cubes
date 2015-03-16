
uniform row_major float4x4 g_ViewProj;

Buffer<float4> Verts;
Buffer<float4> XForms;

float4 main( 
    uint vid : SV_VertexID ) : SV_Position
{
    uint xform = vid/8;
    float4 v  = Verts[vid%8];
    float4 R0 = XForms[3*xform];
    float4 R1 = XForms[3*xform+1];
    float4 R2 = XForms[3*xform+2];

    // deform unit box into desired oriented box
    float3 vPosWS = float3( dot(v,R0), dot(v,R1), dot(v,R2) );
   
    // clip-space transform
    return mul( float4(vPosWS,1), g_ViewProj );
}