#include "stdafx.h"

#include "D3D11Renderer.h"

#include "SpriteBatch.h"


#include <memory>
#include <d3d11_1.h>

D3D11Renderer::D3D11Renderer()
{
}


D3D11Renderer::~D3D11Renderer()
{
}

bool D3D11Renderer::Initialize(HWND hwnd)
{
	bool result = CreateSwapChain(hwnd);
	result = CreateNesTexture();

	m_spSpriteBatch = std::make_unique<DirectX::SpriteBatch>(m_spImmediateContext.Get());

	return result;
}

bool D3D11Renderer::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = 640;
	sd.BufferDesc.Height = 480;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr /*pAdapter*/, D3D_DRIVER_TYPE_HARDWARE, nullptr /*software module*/, 0 /*flags*/,
											 nullptr /*featureLevels*/, 0 /*numFeatureLevels*/, D3D11_SDK_VERSION, &sd, &m_spSwapChain, &m_spDevice,
											 nullptr /*featureLevel*/, &m_spImmediateContext)))
	{
		return false;
	}

	// Create a render target view
	Microsoft::WRL::ComPtr<ID3D11Texture2D> spBackBuffer;
	if( FAILED( m_spSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (LPVOID*)&spBackBuffer ) ) )
		return FALSE;
	HRESULT hr = m_spDevice->CreateRenderTargetView( spBackBuffer.Get(), NULL, &m_spRenderTargetView );
	spBackBuffer.Reset();
	if (FAILED(hr))
		return FALSE;
	m_spImmediateContext->OMSetRenderTargets(1, m_spRenderTargetView.GetAddressOf(), NULL /*pDepthStencilView*/ );

	CRect clientRect;
	GetClientRect(hwnd, &clientRect);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)clientRect.Width();
	vp.Height = (FLOAT)clientRect.Height();
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_spImmediateContext->RSSetViewports( 1, &vp );

	return true;
}


void D3D11Renderer::Render(const PPU::ppuDisplayBuffer_t& screenPixels)
{
	float clearColor[4] = { 0.0f, 0.125f, 0.6f, 1.0f }; // RGBA
	m_spImmediateContext->ClearRenderTargetView(m_spRenderTargetView.Get(), clearColor);

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	/*DX::ThrowIfFailed*/(m_spImmediateContext->Map(m_nesTexture.Get(), 0 /*SubResource*/, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));

	const int c_width = 256;
	const int c_height = 256;
	uint32_t* pPixels = reinterpret_cast<uint32_t*>(mappedResource.pData);

	for (int iRow = 0; iRow != PPU::c_displayHeight; ++iRow)
	{
		for (int iColumn = 0; iColumn != PPU::c_displayWidth; ++iColumn)
		{
			const DWORD pixelData = screenPixels[iRow][iColumn];
			pPixels[iRow*c_width + iColumn] = RGB(GetBValue(pixelData), GetGValue(pixelData), GetRValue(pixelData));
		}
	}

	m_spImmediateContext->Unmap(m_nesTexture.Get(), 0 /*SubResource*/);


	m_spSpriteBatch->Begin();
	m_spSpriteBatch->Draw(m_nesTextureResource.Get(), DirectX::XMFLOAT2(0, 0), nullptr /*sourceRect*/, DirectX::Colors::White /*tint*/, 0.0f /*rotation*/, DirectX::XMFLOAT2() /*origin*/, 2.0f /*scale*/);
	m_spSpriteBatch->End();

	m_spSwapChain->Present(0, 0);
}

bool D3D11Renderer::CreateNesTexture()
{
	// Describe the Sample State
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;


	const int c_width = 256;
	const int c_height = 256;
	std::unique_ptr<uint32_t[]> temp(new uint32_t[c_width * c_height]);

	for (int iRow = 0; iRow != c_height; ++iRow)
	{
		for (int iColumn = 0; iColumn != c_width; ++iColumn)
		{
			temp[iRow*c_width + iColumn] = 0;

			if (iColumn == iRow)
				temp[iRow*c_width + iColumn] = 0x00FFFF00;
		}
	}

	// Create texture
	D3D11_TEXTURE2D_DESC desc;
	desc.Width = c_width; // PPU::c_displayWidth
	desc.Height = c_height;  //PPU::c_displayHeight
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = temp.get();
	initData.SysMemPitch = static_cast<UINT>(c_width);
	initData.SysMemSlicePitch = static_cast<UINT>(c_width * c_height);

	/*DX::ThrowIfFailed*/(m_spDevice->CreateTexture2D(&desc, &initData, m_nesTexture.ReleaseAndGetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	memset(&SRVDesc, 0, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	/*DX::ThrowIfFailed*/(m_spDevice->CreateShaderResourceView(m_nesTexture.Get(), &SRVDesc, m_nesTextureResource.GetAddressOf()));

	//Create the Sample State
	/*DX::ThrowIfFailed*/(m_spDevice->CreateSamplerState(&sampDesc, m_nesSamplerState.GetAddressOf()));

	return true;
}

