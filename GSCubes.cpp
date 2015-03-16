
// flop counts:  VS version: 224 flops/box
//               GS version: 108 flops/box

// curious.  the no-gs version is 5x slower on intel.  Why?
//  It's not the use of strips...


#define _CRT_SECURE_NO_WARNINGS

#include "Simpleton.h"
#include "Mesh.h"
#include "DX11/SimpletonDX11.h"
#include <d3d11.h>

#include "ShaderAutoGen\GS.h"
#include "ShaderAutoGen\VS_WithGS.h"
#include "ShaderAutoGen\VS_Uninstanced.h"
#include "ShaderAutoGen\VS_WithoutGS.h"
#include "ShaderAutoGen\PS.h"

class VCacheWindow : public Simpleton::DX11WindowController
{
public:
    enum 
    {
        BOX_COUNT=25000,
        INDEX_COUNT=14,
        INDEX_COUNT_LIST=36
    };

    Simpleton::DX11PipelineState m_PSO_NoInstance;
    Simpleton::DX11PipelineState m_PSO_NoGS;
    Simpleton::DX11PipelineState m_PSO_GS;
    Simpleton::DX11PipelineResourceSet m_Resources_NoGS;
    Simpleton::DX11PipelineResourceSet m_Resources_GS;
    Simpleton::DX11PipelineResourceSet m_Resources_NoInstance;
    Simpleton::DX11PipelineStateBuilder m_PSOBuilder;
    
    Simpleton::ComPtr<ID3D11Buffer> m_pUnitBoxVerts;
    Simpleton::ComPtr<ID3D11Buffer> m_pUnitBoxIndices;
    Simpleton::ComPtr<ID3D11Buffer> m_pBoxInstances;
    
    Simpleton::ComPtr<ID3D11ShaderResourceView> m_pUnitBoxVertsSRV;
    Simpleton::ComPtr<ID3D11ShaderResourceView> m_pBoxInstancesSRV;
    Simpleton::ComPtr<ID3D11Buffer> m_pUninstancedBoxIndices;

    bool m_bUseGS;

    void CreateBoxInstances( uint nBoxes, ID3D11Device* pDev )
    {
        D3D11_BUFFER_DESC bd;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_SHADER_RESOURCE;
        bd.ByteWidth = 48*nBoxes;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        bd.StructureByteStride = 0;
        bd.Usage = D3D11_USAGE_DEFAULT;

        void* pMem = malloc( bd.ByteWidth );
        for( uint i=0; i<nBoxes; i++ )
        {
            float PI = 3.1415926f;
            float t = Simpleton::Rand(0,PI);
            float p = Simpleton::Rand(-PI,PI);

            Simpleton::Vec3f N = Simpleton::SphericalToCartesian(t,p);
            Simpleton::Vec3f T,B;
            Simpleton::BuildTangentFrame(N,T,B);

            N *= Simpleton::Rand(0.2f,0.5f);
            T *= Simpleton::Rand(0.2f,0.5f);
            B *= Simpleton::Rand(0.2f,0.5f);

            float x = Simpleton::Rand(-50,50);
            float y = 0;
            float z = Simpleton::Rand(-50,50);

            float* pMatrix = (float*)(pMem) + 12*i;
            pMatrix[0] = T.x;
            pMatrix[1] = T.y;
            pMatrix[2] = T.z;
            pMatrix[3] = x;
            pMatrix[4] = B.x;
            pMatrix[5] = B.y;
            pMatrix[6] = B.z;
            pMatrix[7] = y;
            pMatrix[8] = N.x;
            pMatrix[9] = N.y;
            pMatrix[10] = N.z;
            pMatrix[11] = z;

        }

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = pMem;
        data.SysMemPitch = bd.ByteWidth;
        data.SysMemSlicePitch = 0;

        ID3D11Buffer* p;
        pDev->CreateBuffer( &bd, &data, &p );
        m_pBoxInstances.Owns(p);


        D3D11_SHADER_RESOURCE_VIEW_DESC vd;
        vd.Buffer.ElementOffset = 0;
        vd.Buffer.ElementWidth = 3*nBoxes;
        vd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        vd.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        ID3D11ShaderResourceView* pSRV = 0;
        pDev->CreateShaderResourceView( p, &vd,&pSRV);
        m_pBoxInstancesSRV.Owns(pSRV);

        free(pMem);
    }


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
    virtual bool OnCreate( Simpleton::DX11Window* pWindow ) 
    {
        //     2   3
        //   0   1
        //     7   6
        //   4   5
        float UnitCube[] = {
             -1, 1,-1, 1,
              1, 1,-1, 1,
             -1, 1, 1, 1,
              1, 1, 1, 1,

             -1, -1,-1,1,
              1, -1,-1,1,
              1, -1, 1,1,
             -1, -1, 1,1,
        };
        uint32 CubeStripIndices[] = {
           3, 2, 6, 7, 4, 2, 0, 3, 1, 6, 5, 4, 1, 0,
        };
        uint32 CubeIndices[36];
        Simpleton::ExpandTriangleStrip(CubeIndices, CubeStripIndices, 12 );

        D3D11_BUFFER_DESC vb;
        vb.BindFlags = D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_SHADER_RESOURCE;
        vb.ByteWidth = sizeof(UnitCube);
        vb.CPUAccessFlags = 0;
        vb.MiscFlags = 0;
        vb.StructureByteStride = 0;
        vb.Usage = D3D11_USAGE_DEFAULT;

        D3D11_BUFFER_DESC ib;
        ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ib.ByteWidth = sizeof(CubeIndices);
        ib.CPUAccessFlags = 0;
        ib.MiscFlags = 0;
        ib.StructureByteStride = 0;
        ib.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA vbdata;
        vbdata.SysMemPitch = sizeof(UnitCube);
        vbdata.SysMemSlicePitch=0;
        vbdata.pSysMem = UnitCube;

        D3D11_SUBRESOURCE_DATA ibdata;
        ibdata.SysMemPitch = sizeof(CubeIndices);
        ibdata.SysMemSlicePitch=0;
        ibdata.pSysMem = CubeIndices;

        ID3D11Buffer* pVB;
        ID3D11Buffer* pIB;
        pWindow->GetDevice()->CreateBuffer( &vb, &vbdata, &pVB );
        pWindow->GetDevice()->CreateBuffer( &ib, &ibdata, &pIB );

        m_pUnitBoxVerts.Owns(pVB);
        m_pUnitBoxIndices.Owns(pIB);

        
        D3D11_SHADER_RESOURCE_VIEW_DESC vd;
        vd.Buffer.ElementOffset = 0;
        vd.Buffer.ElementWidth = 8;
        vd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        vd.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        ID3D11ShaderResourceView* pSRV = 0;
        pWindow->GetDevice()->CreateShaderResourceView( pVB, &vd,&pSRV);
        m_pUnitBoxVertsSRV.Owns(pSRV);

        uint* pUninstancedIndices=new uint[36*BOX_COUNT];
        for( uint i=0; i<BOX_COUNT; i++ )
            for( uint j=0; j<36; j++ )
                pUninstancedIndices[36*i+j] = 8*i+CubeIndices[j];

        ibdata.pSysMem = pUninstancedIndices;
        ibdata.SysMemPitch = 36*BOX_COUNT*sizeof(uint);
        ibdata.SysMemSlicePitch=0;

        ib.ByteWidth = ibdata.SysMemPitch;
        pWindow->GetDevice()->CreateBuffer( &ib,&ibdata,&pIB);
        m_pUninstancedBoxIndices.Owns(pIB);
        delete[] pUninstancedIndices;

        D3D11_INPUT_ELEMENT_DESC il_nogs[]={
             { "POSITION",0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
             { "XFORM",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,  1,0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
             { "XFORM",  1, DXGI_FORMAT_R32G32B32A32_FLOAT,  1,16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
             { "XFORM",  2, DXGI_FORMAT_R32G32B32A32_FLOAT,  1,32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };

         D3D11_INPUT_ELEMENT_DESC il_gs[]={
             { "XFORM",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
             { "XFORM",  1, DXGI_FORMAT_R32G32B32A32_FLOAT,0,16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
             { "XFORM",  2, DXGI_FORMAT_R32G32B32A32_FLOAT,0,32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        m_PSOBuilder.BeginState(pWindow->GetDevice());
        m_PSOBuilder.SetInputLayout( il_nogs, 4 );
        m_PSOBuilder.SetPixelShader( PS, sizeof(PS) );
        m_PSOBuilder.SetVertexShader( VS_WithoutGS, sizeof(VS_WithoutGS));
        m_PSOBuilder.SetCullMode( D3D11_CULL_BACK );
        m_PSOBuilder.EndState( &m_PSO_NoGS );

        m_PSOBuilder.BeginState(pWindow->GetDevice());
        m_PSOBuilder.SetInputLayout( il_gs, 3 );
        m_PSOBuilder.SetPixelShader( PS, sizeof(PS) );
        m_PSOBuilder.SetVertexShader( VS_WithGS, sizeof(VS_WithGS));
        m_PSOBuilder.SetGeometryShader( GS, sizeof(GS));
        m_PSOBuilder.SetCullMode( D3D11_CULL_BACK );
        m_PSOBuilder.EndState( &m_PSO_GS );

        m_PSOBuilder.BeginState(pWindow->GetDevice());
        m_PSOBuilder.SetPixelShader( PS, sizeof(PS) );
        m_PSOBuilder.SetVertexShader( VS_Uninstanced, sizeof(VS_Uninstanced));
        m_PSOBuilder.SetCullMode( D3D11_CULL_BACK );
        m_PSOBuilder.EndState( &m_PSO_NoInstance );


        m_PSO_NoGS.GetResourceSchema()->CreateResourceSet( &m_Resources_NoGS, pWindow->GetDevice() );
        m_PSO_GS.GetResourceSchema()->CreateResourceSet( &m_Resources_GS, pWindow->GetDevice() );
        m_PSO_NoInstance.GetResourceSchema()->CreateResourceSet( &m_Resources_NoInstance, pWindow->GetDevice() );
        CreateBoxInstances(BOX_COUNT,pWindow->GetDevice());

        m_bUseGS = false;

        return true;
    }
        
    virtual void OnKeyDown( Simpleton::Window* pWindow, KeyCode eKey ) 
    {
        switch( eKey )
        {
        case KEY_G:
            m_bUseGS = !m_bUseGS;
            break;
        }
    };


    virtual void OnFrame( Simpleton::DX11Window* pWindow )
    {
        float black[4] = {0.2,0.2,0.2,1};
        float red[4] = {0.2,0,0,1};
        ID3D11Device* pDev = pWindow->GetDevice();
        ID3D11DeviceContext* pCtx = pWindow->GetDeviceContext();
        ID3D11RenderTargetView* pRTV = pWindow->GetBackbufferRTV();

        D3D11_VIEWPORT vp  = pWindow->BuildViewport();
        D3D11_RECT scissor = pWindow->BuildScissorRect();

        float* pClearColor = m_bUseGS ? red : black;

        pCtx->RSSetViewports(1,&vp);
        pCtx->RSSetScissorRects(1,&scissor);
        pCtx->ClearRenderTargetView( pWindow->GetBackbufferRTV(), pClearColor );
        pCtx->ClearDepthStencilView( pWindow->GetBackbufferDSV(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
        
        Simpleton::Vec3f eye(0,10,-10);
        Simpleton::Matrix4f mView = Simpleton::MatrixLookAtLH( eye, Simpleton::Vec3f(0,0,0), Simpleton::Vec3f(0,1,0) );
        Simpleton::Matrix4f mProj = Simpleton::MatrixPerspectiveFovLH( 1,45,1,1000 );
        Simpleton::Matrix4f mViewProj = mProj * mView;
        
        Simpleton::Matrix4f mInv = Simpleton::Inverse( mViewProj );
        Simpleton::Matrix4f mUVToClip = Simpleton::MatrixScale( 1,-1,1) * 
                                      Simpleton::MatrixTranslate( -1,-1,0) * 
                                      Simpleton::MatrixScale(2.0f/vp.Width, 2.0f/vp.Height, 1);

        mInv = mInv*mUVToClip;

                                      
        m_Resources_NoGS.BeginUpdate(pCtx);
        m_Resources_NoGS.BindConstant( "g_ViewProj", &mViewProj, sizeof(mViewProj) );
        m_Resources_NoGS.BindConstant( "g_RasterToWorld", &mInv, sizeof(mInv) );
        m_Resources_NoGS.EndUpdate(pCtx);

        m_Resources_GS.BeginUpdate(pCtx);
        m_Resources_GS.BindConstant( "g_ViewProj", &mViewProj, sizeof(mViewProj) );
        m_Resources_GS.BindConstant( "g_RasterToWorld", &mInv, sizeof(mInv) );
        m_Resources_GS.EndUpdate(pCtx);

        m_Resources_NoInstance.BeginUpdate(pCtx);
        m_Resources_NoInstance.BindConstant( "g_ViewProj", &mViewProj, sizeof(mViewProj) );
        m_Resources_NoInstance.BindConstant( "g_RasterToWorld", &mInv, sizeof(mInv) );
        m_Resources_NoInstance.BindSRV("Verts", m_pUnitBoxVertsSRV);
        m_Resources_NoInstance.BindSRV("XForms", m_pBoxInstancesSRV);


        m_Resources_NoInstance.EndUpdate(pCtx);


        pCtx->OMSetRenderTargets( 1, &pRTV, pWindow->GetBackbufferDSV() ); 
      
        if( m_bUseGS )
        {
            ID3D11Buffer* pBuffers[2] = { m_pBoxInstances };
            uint pStrides[2] = {48};
            uint pOffsets[2] = {0,0};

            m_PSO_GS.Apply(pCtx);
            m_Resources_GS.Apply(pCtx);

            pCtx->IASetVertexBuffers( 0,1,pBuffers,pStrides, pOffsets );
            pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
            pCtx->Draw( BOX_COUNT, 0 );
        }
        else
        {
            m_PSO_NoInstance.Apply(pCtx);
            m_Resources_NoInstance.Apply(pCtx);
            pCtx->IASetIndexBuffer( m_pUninstancedBoxIndices, DXGI_FORMAT_R32_UINT, 0 );
            pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            pCtx->DrawIndexed( BOX_COUNT*36, 0,0);

           //ID3D11Buffer* pBuffers[2] = { m_pUnitBoxVerts, m_pBoxInstances };
           //uint pStrides[2] = {16,48};
           //uint pOffsets[2] = {0,0};
           //
           //m_PSO_NoGS.Apply(pCtx);
           //m_Resources_NoGS.Apply(pCtx);
           //
           //pCtx->IASetVertexBuffers( 0,2,pBuffers,pStrides, pOffsets );
           //pCtx->IASetIndexBuffer(m_pUnitBoxIndices,DXGI_FORMAT_R32_UINT,0);
           //pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
           //
           //pCtx->DrawIndexedInstanced( INDEX_COUNT_LIST, BOX_COUNT, 0, 0, 0 );
        }
      
    }
};

int main()
{
    VCacheWindow wc;
    Simpleton::DX11Window* pWin = Simpleton::DX11Window::Create( 512,512,Simpleton::DX11Window::FPS_TITLE,&wc);
    while( pWin->DoEvents() )
        pWin->DoFrame();

    return 0;
}