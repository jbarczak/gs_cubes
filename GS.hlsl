


struct GSIn
{
    float4 R0 : XFORM0;
    float4 R1 : XFORM1;
    float4 R2 : XFORM2;
};

struct GSOut
{
    float4 v : SV_Position;
};

void emit( inout TriangleStream<GSOut> triStream, float4 v )
{
    GSOut s;
    s.v = v;
    triStream.Append(s);
}

// Single tri-strip cube, from:
//  Interesting CUBE triangle strip
//   http://www.asmcommunity.net/board/index.php?action=printpage;topic=6284.0
// ; 1-------3-------4-------2   Cube = 8 vertices
// ; |  E __/|\__ A  |  H __/|   =================
// ; | __/   |   \__ | __/   |   Single Strip: 4 3 7 8 5 3 1 4 2 7 6 5 2 1
// ; |/   D  |  B   \|/   I  |   12 triangles:     A B C D E F G H I J K L
// ; 5-------8-------7-------6
// ;         |  C __/|
// ;         | __/   |
// ;         |/   J  |
// ;         5-------6
// ;         |\__ K  |
// ;         |   \__ |
// ;         |  L   \|         Left  D+E
// ;         1-------2        Right  H+I
// ;         |\__ G  |         Back  K+L
// ;         |   \__ |        Front  A+B
// ;         |  F   \|          Top  F+G
// ;         3-------4       Bottom  C+J
// ;
static const int INDICES[14] =
{
   4, 3, 7, 8, 5, 3, 1, 4, 2, 7, 6, 5, 2, 1,
};

uniform row_major float4x4 g_ViewProj;


//     3   4
//   1   2
//     8   7
//   5   6
void GenerateTransformedBox( out float4 v[8], float4 R0, float4 R1, float4 R2 )
{
    // All of the canonical box verts are +- 1, transformed by the 3x4 matrix { R0,R1,R2}
    // 
    // Instead of transforming each of the 8 verts, we can exploit the distributive law
    //   for matrix multiplication and do the extrusion in clip space
    //
   //float4 center = mul( float4( R0.w,R1.w,R2.w,1), g_ViewProj );
   //float4 X      = mul( float4( R0.xyz,0), g_ViewProj );
   //float4 Y      = mul( float4( R1.xyz,0), g_ViewProj );
   //float4 Z      = mul( float4( R2.xyz,0), g_ViewProj );
   //
    float4 center =float4( R0.w,R1.w,R2.w,1);
    float4 X = float4( R0.x,R1.x,R2.x,0);
    float4 Y = float4( R0.y,R1.y,R2.y,0);
    float4 Z = float4( R0.z,R1.z,R2.z,0);
    center = mul( center, g_ViewProj );
    X = mul( X, g_ViewProj );
    Y = mul( Y, g_ViewProj );
    Z = mul( Z, g_ViewProj );

    float4 t1 = center - X - Z ;
    float4 t2 = center + X - Z ;
    float4 t3 = center - X + Z ;
    float4 t4 = center + X + Z ;

    v[0] = t1 + Y;
    v[1] = t2 + Y;
    v[2] = t3 + Y;
    v[3] = t4 + Y;
    v[4] = t1 - Y;
    v[5] = t2 - Y;
    v[6] = t4 - Y;
    v[7] = t3 - Y;

  // [unroll]
  // for( int i=0; i<8; i++ )
  //     v[i] = mul(v[i],g_ViewProj);
}


[maxvertexcount(14)]
void main( point GSIn box[1], inout TriangleStream<GSOut> triStream )
{
    float4 v[8];
    GenerateTransformedBox( v, box[0].R0, box[0].R1, box[0].R2 );

    //  Indices are off by one, so we just let the optimizer fix it
    [unroll]
    for( int i=0; i<14; i++ )
        emit(triStream, v[INDICES[i]-1] );
}
 