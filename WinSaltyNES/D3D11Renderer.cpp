#include "stdafx.h"

#include "D3D11Renderer.h"
#include "SpriteBatch.h"
#include "hqx.h"

#include <d3d11_1.h>
#include <memory>

//#define IfFailedHrReturn(_hr) \
//	do { \
//		HRESULT _hrTemp = _hr; \
//		if (FAILED(_hrTemp)) \
//			return false; \
//	}
#define IfFailedHrReturn(_hr) \
	if (FAILED(_hr)) \
		return false

#define IfFailedReturn(_bool) \
	if (!(_bool)) \
		return false

D3D11Renderer::D3D11Renderer()
{
}


D3D11Renderer::~D3D11Renderer()
{
	if (m_spImmediateContext)
		m_spImmediateContext->ClearState();
}

bool D3D11Renderer::Initialize(HWND hwnd)
{
	m_hwnd = hwnd;

	m_textureWidth = PPU::c_displayWidth * 2;
	m_textureHeight = PPU::c_displayHeight * 2;

	IfFailedReturn(CreateSwapChain(hwnd));
	IfFailedReturn(CreateNesTexture());

	m_spSpriteBatch = std::make_unique<DirectX::SpriteBatch>(m_spImmediateContext.Get());

	hqxInit();

	return true;
}

bool D3D11Renderer::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = m_textureWidth;
	sd.BufferDesc.Height = m_textureHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	IfFailedHrReturn(D3D11CreateDeviceAndSwapChain(nullptr /*pAdapter*/, D3D_DRIVER_TYPE_HARDWARE, nullptr /*software module*/, 0 /*flags*/,
											 nullptr /*featureLevels*/, 0 /*numFeatureLevels*/, D3D11_SDK_VERSION, &sd, &m_spSwapChain, &m_spDevice,
											 nullptr /*featureLevel*/, &m_spImmediateContext));

	// Create a render target view
	Microsoft::WRL::ComPtr<ID3D11Texture2D> spBackBuffer;
	IfFailedHrReturn(m_spSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (LPVOID*)&spBackBuffer));
	HRESULT hr = m_spDevice->CreateRenderTargetView( spBackBuffer.Get(), NULL, &m_spRenderTargetView);
	spBackBuffer.Reset();
	IfFailedHrReturn(hr);
	m_spImmediateContext->OMSetRenderTargets(1, m_spRenderTargetView.GetAddressOf(), NULL /*pDepthStencilView*/ );

	(bool)Resize();

	return true;
}


bool D3D11Renderer::Resize()
{
	if (m_hwnd == NULL)
		return false;

	CRect clientRect;
	GetClientRect(m_hwnd, &clientRect);

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

void RunFilter(const PPU::ppuDisplayBuffer_t& screenPixels, uint32_t* pPixels, int targetWidth, int targetHeight)
{

#if 1
	hq2x_32(reinterpret_cast<const uint32_t*>(&screenPixels), pPixels, PPU::c_displayWidth, PPU::c_displayHeight);
#elif 0
	// Nearest neighbor.
	const int ratio = targetWidth / PPU::c_displayWidth;
	for (int iRow = 0; iRow != targetHeight; ++iRow)
	{
		for (int iColumn = 0; iColumn != targetWidth; ++iColumn)
		{
			const DWORD pixelData = screenPixels[iRow / ratio][iColumn / ratio];
			pPixels[iRow*targetWidth + iColumn] = pixelData;//GetRValue(pixelData) << 24 | GetGValue(pixelData) << 16 | GetBValue(pixelData) << 8;
		}
	}
#elif 0
	// Linear interpolation
	const int ratio = targetWidth / PPU::c_displayWidth;
	for (int iRow = 0; iRow != targetHeight - ratio; ++iRow)
	{
		for (int iColumn = 0; iColumn != targetWidth - ratio; ++iColumn)
		{
			const uint32_t* pRootPixel = &screenPixels[iRow / ratio][iColumn / ratio];
			
			const uint32_t p00 = *(pRootPixel + 0 * PPU::c_displayWidth + 0);
			const uint32_t p01 = *(pRootPixel + 0 * PPU::c_displayWidth + 1);
			const uint32_t p10 = *(pRootPixel + 1 * PPU::c_displayWidth + 0);
			const uint32_t p11 = *(pRootPixel + 1 * PPU::c_displayWidth + 1);

			const float fx = (float)(iColumn % ratio) / ratio;
			const float fy = (float)(iRow % ratio) / ratio;
			const float fx1 = 1.0f - fx;
			const float fy1 = 1.0f - fy;

			const int w00 = static_cast<int>(fx1 * fy1 * 256.0f);
			const int w01 = static_cast<int>(fx * fy1 * 256.0f);
			const int w10 = static_cast<int>(fx1 * fy * 256.0f);
			const int w11 = static_cast<int>(fx * fy * 256.0f);

			const int resultR = GetRValue(p00) * w00 + GetRValue(p01) * w01 + GetRValue(p10) * w10 + GetRValue(p11) * w11;
			const int resultG = GetGValue(p00) * w00 + GetGValue(p01) * w01 + GetGValue(p10) * w10 + GetGValue(p11) * w11;
			const int resultB = GetBValue(p00) * w00 + GetBValue(p01) * w01 + GetBValue(p10) * w10 + GetBValue(p11) * w11;

			pPixels[iRow*targetWidth + iColumn] = RGB(resultR >> 8, resultG >> 8, resultB >> 8);
		}
	}
#endif

}

bool D3D11Renderer::Render(const PPU::ppuDisplayBuffer_t& screenPixels)
{
	float clearColor[4] = { 1.0f, 0.125f, 0.6f, 1.0f }; // RGBA
	m_spImmediateContext->ClearRenderTargetView(m_spRenderTargetView.Get(), clearColor);

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	IfFailedHrReturn(m_spImmediateContext->Map(m_nesTexture.Get(), 0 /*SubResource*/, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));

	uint32_t* pPixels = reinterpret_cast<uint32_t*>(mappedResource.pData);
	RunFilter(screenPixels, pPixels, m_textureWidth, m_textureHeight);
	m_spImmediateContext->Unmap(m_nesTexture.Get(), 0 /*SubResource*/);

	m_spSpriteBatch->Begin();
	//m_spSpriteBatch->Draw(m_nesTextureResource.Get(), DirectX::XMFLOAT2(0, 0), nullptr /*sourceRect*/, DirectX::Colors::White /*tint*/, 0.0f /*rotation*/, DirectX::XMFLOAT2(0.0f,0.0f)/*origin*/, 1.0f /*scale*/);
	m_spSpriteBatch->Draw(m_nesTextureResource.Get(), DirectX::XMFLOAT2(0, 0));
	m_spSpriteBatch->End();

	m_spSwapChain->Present(0, 0);

	return true;
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

	std::unique_ptr<uint32_t[]> temp(new uint32_t[m_textureWidth * m_textureHeight]);

	for (int iRow = 0; iRow != m_textureHeight; ++iRow)
	{
		for (int iColumn = 0; iColumn != m_textureWidth; ++iColumn)
		{
			temp[iRow*m_textureWidth + iColumn] = 0xFF000000;
		}
	}

	// Create texture
	D3D11_TEXTURE2D_DESC desc;
	desc.Width = m_textureWidth;
	desc.Height = m_textureHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = temp.get();
	initData.SysMemPitch = static_cast<UINT>(m_textureWidth);
	initData.SysMemSlicePitch = static_cast<UINT>(m_textureWidth * m_textureHeight);

	IfFailedHrReturn(m_spDevice->CreateTexture2D(&desc, &initData, m_nesTexture.ReleaseAndGetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	memset(&SRVDesc, 0, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	IfFailedHrReturn(m_spDevice->CreateShaderResourceView(m_nesTexture.Get(), &SRVDesc, m_nesTextureResource.GetAddressOf()));

	//Create the Sample State
	IfFailedHrReturn(m_spDevice->CreateSamplerState(&sampDesc, m_nesSamplerState.GetAddressOf()));

	return true;
}

