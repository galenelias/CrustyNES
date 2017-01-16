#pragma once

#include "NES/Ppu.h"

#include <wrl/client.h>
#include <memory>

struct ID3D11Device;
struct IDXGISwapChain;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;

struct ID3D11Texture2D;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;

namespace DirectX
{
	class SpriteBatch;
}

class D3D11Renderer
{
public:
	D3D11Renderer();
	~D3D11Renderer();

	bool Initialize(HWND hwnd);

	void Render(const PPU::ppuDisplayBuffer_t& screenPixels);


private:
	bool CreateSwapChain(HWND hwnd);
	bool CreateNesTexture();

	Microsoft::WRL::ComPtr<ID3D11Device> m_spDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_spSwapChain;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_spImmediateContext;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_spRenderTargetView;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_nesTexture;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>	m_nesSamplerState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_nesTextureResource;
	
	std::unique_ptr<DirectX::SpriteBatch> m_spSpriteBatch;

};

