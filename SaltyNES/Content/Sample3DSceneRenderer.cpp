#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "NES/Ppu.h"
#include "NES/NESRom.h"

#include "..\Common\DirectXHelper.h"

using namespace SaltyNES;

using namespace DirectX;
using namespace Windows::Foundation;


class UniversalReadOnlyFile : public IReadableFile
{
public:
	virtual ~UniversalReadOnlyFile() override {}
	virtual void Read(uint32_t cbRead, _Out_writes_bytes_(cbRead) byte* pBuffer) override
	{
		if (m_offset + cbRead > m_bytes->Length)
			throw std::runtime_error("Reading beyond the length of the .ROM");

		memcpy(pBuffer, m_bytes->Data + m_offset, cbRead);
		m_offset += cbRead;
	}

	UniversalReadOnlyFile(Windows::Storage::Streams::DataReader^ dataReader)
	{
		m_reader = dataReader;

		m_bytes = ref new Platform::Array<unsigned char>(dataReader->UnconsumedBufferLength);
		dataReader->ReadBytes(m_bytes);
		m_offset = 0;
	}

private:
	Windows::Storage::Streams::DataReader^ m_reader;
	Platform::Array<unsigned char>^ m_bytes;

	uint32_t m_offset;
};




// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(10),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.7f, 1.5f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, -0.1f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		Rotate(radians);
	}
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
}

void Sample3DSceneRenderer::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void Sample3DSceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		Rotate(radians);
	}
}

void Sample3DSceneRenderer::StopTracking()
{
	m_tracking = false;
}

// Renders one frame using the vertex and pixel shaders.
void Sample3DSceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	for (;;)
	{
		m_nes.RunCycle();
		if (m_nes.GetPpu().ShouldRender())
			break;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};

	DX::ThrowIfFailed(context->Map(m_cubeTextureTexture.Get(), 0 /*SubResource*/, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));

	const int c_width = 256;
	const int c_height = 256;
	uint32_t* pPixels = reinterpret_cast<uint32_t*>(mappedResource.pData);

	PPU::ppuDisplayBuffer_t screenPixels;
	m_nes.GetPpu().RenderToBuffer(screenPixels, PPU::RenderOptions());

	for (int iRow = 0; iRow != PPU::c_displayHeight; ++iRow)
	{
		for (int iColumn = 0; iColumn != PPU::c_displayWidth; ++iColumn)
		{
			const DWORD pixelData = screenPixels[iRow][iColumn];
			pPixels[iRow*c_width + iColumn] = RGB(GetBValue(pixelData), GetGValue(pixelData), GetRValue(pixelData));
		}
	}

	context->Unmap(m_cubeTextureTexture.Get(), 0 /*SubResource*/);


	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource(
		m_constantBuffer.Get(),
		0,
		NULL,
		&m_constantBufferData,
		0,
		0
		);

	// Each vertex is one instance of the VertexPositionColor struct.
	UINT stride = sizeof(VertexPositionTexture);
	UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		m_vertexBuffer.GetAddressOf(),
		&stride,
		&offset
		);

	context->IASetIndexBuffer(
		m_indexBuffer.Get(),
		DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
		0
		);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->IASetInputLayout(m_inputLayout.Get());

	// Attach our vertex shader.
	context->VSSetShader(
		m_vertexShader.Get(),
		nullptr,
		0
		);

	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers(
		0,
		1,
		m_constantBuffer.GetAddressOf()
		);

	// Attach our pixel shader.
	context->PSSetShader(
		m_pixelShader.Get(),
		nullptr,
		0
		);

	context->PSSetShaderResources(
		0,
		1,
		m_cubeTextureResource.GetAddressOf()
		);

	context->PSSetSamplers(
		0,
		1,
		m_cubeSamplerState.GetAddressOf())
		;

	// Draw the objects.
	context->DrawIndexed(
		m_indexCount,
		0,
		0
		);
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	// Load shaders asynchronously.
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_vertexShader
				)
			);

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc,
				ARRAYSIZE(vertexDesc),
				&fileData[0],
				fileData.size(),
				&m_inputLayout
				)
			);
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShader
				)
			);

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer) , D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&m_constantBuffer
				)
			);
	});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this] () {

		// Load mesh vertices. Each vertex has a position and a texture coordinate.
		static const VertexPositionTexture cubeVertices[] = 
		{
			// Front Face
			{XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f)},

			// Back Face
			{XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-0.5f,  0.5f, 0.5f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3( 0.5f,  0.5f, 0.5f), XMFLOAT2(1.0f, 0.0f)},
			{XMFLOAT3( 0.5f, -0.5f, 0.5f), XMFLOAT2(1.0f, 1.0f)},

			// Top Face
			{XMFLOAT3(-0.5f, 0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3( 0.5f, 0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3( 0.5f, 0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(-0.5f, 0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f)},

			// Bottom Face
			{XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f)},
			{XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f)},

			// Left Face
			{XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f)},

			// Right Face
			{XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3( 0.5f,  0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f)},
		};

		D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
		vertexBufferData.pSysMem = cubeVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&vertexBufferDesc,
				&vertexBufferData,
				&m_vertexBuffer
				)
			);

		// Load mesh indices. Each trio of indices represents
		// a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes
		// 0, 2 and 1 from the vertex buffer compose the 
		// first triangle of this mesh.
		static const unsigned short cubeIndices [] =
		{
			// Front Face
			0, 1, 2,
			0, 2, 3,

			// Back Face
			4, 5, 6,
			4, 6, 7,

			// Top Face
			8, 9, 10,
			8, 10, 11,

			// Bottom Face
			12, 13, 14,
			12, 14, 15,

			// Left Face
			16, 17, 18,
			16, 18, 19,

			// Right Face
			20, 21, 22,
			20, 22, 23
		};

		m_indexCount = ARRAYSIZE(cubeIndices);

		D3D11_SUBRESOURCE_DATA indexBufferData = {0};
		indexBufferData.pSysMem = cubeIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&indexBufferDesc,
				&indexBufferData,
				&m_indexBuffer
				)
			);
	});

	auto loadTextureTask = createCubeTask.then([this]() {
		// Move to global'ish

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

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&desc, &initData, m_cubeTextureTexture.ReleaseAndGetAddressOf()));

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		memset(&SRVDesc, 0, sizeof(SRVDesc));
		SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_cubeTextureTexture.Get(), &SRVDesc, m_cubeTextureResource.GetAddressOf()));

		//Create the Sample State
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateSamplerState(&sampDesc, m_cubeSamplerState.GetAddressOf()));
	});

	auto loadRomTask = loadTextureTask.then([] () {
		auto installedFolder = Windows::ApplicationModel::Package::Current->InstalledLocation;
		return concurrency::create_task(installedFolder->GetFileAsync("Assets\\DonkeyKongJr.nes"));
	}).then([](Windows::Storage::StorageFile^ storageFile)
	{
		return concurrency::create_task(Windows::Storage::FileIO::ReadBufferAsync(storageFile));
	}).then([this](Windows::Storage::Streams::IBuffer^ buffer)
	{
		Windows::Storage::Streams::DataReader^ dataReader = Windows::Storage::Streams::DataReader::FromBuffer(buffer);

		UniversalReadOnlyFile romFile(dataReader);
		m_nes.LoadRomFile(&romFile);
		m_nes.Reset();
	});

	// Once the cube is loaded, the object is ready to be rendered.
	loadRomTask.then([this] () {
		m_loadingComplete = true;
	});
}

void Sample3DSceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}