

struct GSIn
{
    float4 R0 : XFORM0;
    float4 R1 : XFORM1;
    float4 R2 : XFORM2;
};

GSIn main( float4 R0 : XFORM0, float4 R1 : XFORM1, float4 R2 : XFORM2 )
{
    GSIn g;
    g.R0 = R0;
    g.R1 = R1;
    g.R2 = R2;
    return g;
}