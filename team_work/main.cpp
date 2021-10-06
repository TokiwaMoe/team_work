#include <Windows.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <dinput.h>
#include <wrl.h>

#define DIRECTINPUT_VERSION             0x0800 //Direct Inputのバージョン指定
#define PAI  3.14159265359

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

//いか

//ウィンドウプロシージャの定義
LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// メッセージで分岐
	switch (msg)
	{
	case WM_DESTROY: // ウィンドウが破棄された
		PostQuitMessage(0); // OSに対して、アプリの終了を伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 標準の処理を行う
}

/*-------------------------------------------------------------------------------------------------データ*/
//パイプラインセット
struct PipeLineSet
{
	//パイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> pipelinestate;
	//ルートシグネチャ
	ComPtr<ID3D12RootSignature> rootsignature;
};

//スプライト１枚分のデータ
struct  Sprite
{
	//頂点バッファ
	ComPtr<ID3D12Resource> vertBuff;
	//頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vbView;
	//定数バッファ
	ComPtr<ID3D12Resource> constBuff;

	//Z軸回りの回転角
	float rotation = 0.0f;
	//座標
	XMFLOAT3 position = { 0, 0, 0 };
	//ワールド行列
	XMMATRIX matWorld;
	//色
	XMFLOAT4 color = { 1, 1, 1, 1 };
	//テクスチャ番号
	UINT texNumber = 0;
	//大きさ
	XMFLOAT2 size = { 100, 100 };

	//アンカーポイント
	XMFLOAT2 anchorpoint = { 0.0f, 0.0f };

	//左右反転
	bool isFilpX = false;
	//上下反転
	bool isFilpY = false;

	//テクスチャの左上座標
	XMFLOAT2 texLeftTop = { 0, 0 };
	//テクスチャの切り出しサイズ
	XMFLOAT2 texSize = { 100, 100 };

	//非表示
	bool isInvisible = false;
};
//定数バッファ用データ構造体
struct ConstBufferData
{
	XMFLOAT4 color; //色
	XMMATRIX mat; //行列
};
//スプライトの頂点データ型
struct VertexPosUv
{
	XMFLOAT3 pos; //xyz座標
	XMFLOAT2 uv; //uv座標
};
//テクスチャの最大枚数
const int spriteSRVCount = 512;
//スプライトの共通データ
struct SpriteCommon
{
	//パイプラインセット
	PipeLineSet pipelineSet;

	//射影行列
	XMMATRIX matProjection;

	//テクスチャ用デスクリプタヒープの生成
	ComPtr<ID3D12DescriptorHeap> descHeap;

	//テクスチャリソース（テクスチャバッファ）の配列
	ComPtr<ID3D12Resource> texBuff[spriteSRVCount];
};

//３Dオブジェクト１個分のデータ
struct Object3d
{
	//頂点バッファの生成
	ComPtr<ID3D12Resource> vertBuff;
	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	//インデックスバッファ
	ComPtr<ID3D12Resource> indexBuff;
	//インデックスバッファビュー
	D3D12_INDEX_BUFFER_VIEW ibView{};
	//定数バッファの生成
	ComPtr<ID3D12Resource> constBuff;

	//スケール
	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f }; //座標を変数として持つ
	//回転
	XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f }; //座標を変数として持つ
	//平行移動
	XMFLOAT3 position = { 0.0f, 0.0f, 0.0f }; //座標を変数として持つ
	//ワールド行列
	XMMATRIX matWorld;
	//始点座標
	XMFLOAT3 eye = { 0, 0, -125 };
	//注意点座標
	XMFLOAT3 target = { 0, 0, 0 };
	//上方向ベクトル
	XMFLOAT3 up = { 0, 1, 0 };
	//ビューの変換行列
	XMMATRIX matView;
	//色(RGBA)
	XMFLOAT4 color = { 1, 1, 1, 1 };
	//テクスチャ番号
	UINT texNumber = 0;
	//非表示
	bool isInvisible = false;
};
//３Dオブジェクトの頂点データ型
struct Vertex
{
	XMFLOAT3 pos; //xyz座標
	XMFLOAT3 normal; //法線ベクトル
	XMFLOAT2 uv; //uv座標
};
//テクスチャの最大枚数
const int object3dSRVCount = 512;
//３Dオブジェクト共通データ
struct Object3dCommon
{
	//パイプラインセット
	PipeLineSet pipelineset;

	//射影行列
	XMMATRIX matProjection;

	//テクスチャ用デスクリプタヒープも生成
	ComPtr<ID3D12DescriptorHeap> descHeap;
	//テクスチャバッファの生成
	ComPtr<ID3D12Resource> texBuff[object3dSRVCount];
};

/*-------------------------------------------------------------------------------------------------関数*/
//パイプラインの生成（関数）
PipeLineSet Object3dCreateGraphicsPipeline(ID3D12Device* dev);
//パイプラインの生成（スプライト用関数）
PipeLineSet SpriteCreateGraphicsPipeline(ID3D12Device* dev);

//スプライト作成
Sprite SpriteCreate(ID3D12Device* dev, int window_width, int window_height, UINT texNumber, const SpriteCommon& spriteCommon, XMFLOAT2 anchorpoint, bool isFilpX, bool isFilpY);
//スプライト共通グラフィックコマンドのセット
void SpriteCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon);
//スプライト単体表示
void SpriteDraw(const Sprite& sprite, ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon, ID3D12Device* dev);
//スプライト共通データ生成
SpriteCommon SpriteCommonCreate(ID3D12Device* dev, int window_width, int window_height);
//スプライト単体更新
void SpriteUpdate(Sprite& sprite, const SpriteCommon& spriteCommon);
//スプライト共通テクスチャ読み込み
void SpriteCommonLoadTexture(SpriteCommon& spriteCommon, UINT texNumber, const wchar_t* filename, ID3D12Device* dev);
//スプライト単体頂点バッファの転送
void SpriteTransferVertexBuffer(const Sprite& sprite, const SpriteCommon& spriteCommon);

//３Dオブジェクト作成
Object3d Object3dCreate_P(ID3D12Device* dev, int window_width, int window_height, UINT texNumber);
Object3d Object3dCreate_E(ID3D12Device* dev, int window_width, int window_height, UINT texNumber);
Object3d Object3dCreate_W(ID3D12Device* dev, int window_width, int window_height, UINT texNumber);
Object3d Object3dCreate_B(ID3D12Device* dev, int window_width, int window_height, UINT texNumber);
//３Dオブジェクト共通グラフィックコマンドのセット
void Object3dCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon);
//３Dオブジェクト単体表示
void Object3dDraw_P(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev);
void Object3dDraw_E(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev);
void Object3dDraw_W(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev);
void Object3dDraw_B(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev);
//３Dオブジェクト共通データ生成
Object3dCommon Object3dCommonCreate(ID3D12Device* dev, int window_width, int window_height);
//３Dオブジェクト単体更新
void Object3dUpdate(Object3d& object3d, const Object3dCommon& object3dCommon);
//３Dオブジェクト共通テクスチャ読み込み
void Object3dCommonLoadTexture(Object3dCommon& object3dCommon, UINT texnumber, const wchar_t* filename, ID3D12Device* dev);

//# Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ウィンドウサイズ
	const int window_width = 1280;  // 横幅
	const int window_height = 720;  // 縦幅

	WNDCLASSEX w{}; // ウィンドウクラスの設定
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc; // ウィンドウプロシージャを設定
	w.lpszClassName = "DirectXGame"; // ウィンドウクラス名
	w.hInstance = GetModuleHandle(nullptr); // ウィンドウハンドル
	w.hCursor = LoadCursor(NULL, IDC_ARROW); // カーソル指定

	// ウィンドウクラスをOSに登録
	RegisterClassEx(&w);
	// ウィンドウサイズ{ X座標 Y座標 横幅 縦幅 }
	RECT wrc = { 0, 0, window_width, window_height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // 自動でサイズ補正

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName, // クラス名
		"DirectXGame",         // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,        // 標準的なウィンドウスタイル
		CW_USEDEFAULT,              // 表示X座標（OSに任せる）
		CW_USEDEFAULT,              // 表示Y座標（OSに任せる）
		wrc.right - wrc.left,       // ウィンドウ横幅
		wrc.bottom - wrc.top,   // ウィンドウ縦幅
		nullptr,                // 親ウィンドウハンドル
		nullptr,                // メニューハンドル

		w.hInstance,            // 呼び出しアプリケーションハンドル
		nullptr);               // オプション

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};  // メッセージ

	// DirectX初期化処理　ここから
	HRESULT result;
	ComPtr<IDXGIFactory6> dxgiFactory;
	ComPtr<ID3D12Device> dev;
	ComPtr<IDXGISwapChain4> swapchain;
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
	ComPtr<ID3D12CommandQueue> cmdQueue;
	ComPtr<ID3D12DescriptorHeap> rtvHeaps;

#ifdef _DEBUG
	//デバッグレイヤーをオンに
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif

	//DXGIファクトリーの生成
	result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

	//アダプターの列挙用
	std::vector<ComPtr<IDXGIAdapter1>> adapters;

	//ここに特定の名前を持つアダプターオブジェクトが入る
	ComPtr<IDXGIAdapter1> tmpAdapter;
	for (int i = 0; dxgiFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++)
	{
		adapters.push_back(tmpAdapter);
	}
	for (int i = 0; i < adapters.size(); i++)
	{
		DXGI_ADAPTER_DESC1 adesc;
		adapters[i]->GetDesc1(&adesc); //アダプターの情報を取得

		//ソフトウェアデバイスを回避
		if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		std::wstring strDesc = adesc.Description; //アダプター名
		//inetl UHD Graphics（オンボードグラフィック）を回避
		if (strDesc.find(L"Intel") == std::wstring::npos)
		{
			tmpAdapter = adapters[i];   // 採用
			break;
		}
	}
	// 対応レベルの配列
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	D3D_FEATURE_LEVEL featureLevel;
	for (int i = 0; i < _countof(levels); i++)
	{
		// 採用したアダプターでデバイスを生成
		result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&dev));
		if (result == S_OK)
		{
			// デバイスを生成できた時点でループを抜ける
			featureLevel = levels[i];
			break;
		}
	}

	// コマンドアロケータを生成
	result = dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&cmdAllocator));

	// コマンドリストを生成
	result = dev->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		cmdAllocator.Get(), nullptr,
		IID_PPV_ARGS(&cmdList));

	// 標準設定でコマンドキューを生成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
	dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));

	// 各種設定をしてスワップチェーンを生成
	ComPtr<IDXGISwapChain1> swapchain1;

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = 1280;
	swapchainDesc.Height = 720;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // 色情報の書式
	swapchainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER; // バックバッファ用
	swapchainDesc.BufferCount = 2;  // バッファ数を２つに設定
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は破棄
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	dxgiFactory->CreateSwapChainForHwnd(
		cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		&swapchain1);

	swapchain1.As(&swapchain);

	// 各種設定をしてデスクリプタヒープを生成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	heapDesc.NumDescriptors = 2;    // 裏表の２つ
	dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(&rtvHeaps));

	// 裏表の２つ分について
	std::vector<ComPtr<ID3D12Resource>> backBuffers(2);
	for (int i = 0; i < 2; i++)
	{
		// スワップチェーンからバッファを取得
		result = swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
		// デスクリプタヒープのハンドルを取得
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle =
			CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeaps->GetCPUDescriptorHandleForHeapStart(), //ヒープの先頭アドレス
				i, //ディスクリプタの番号
				dev->GetDescriptorHandleIncrementSize(heapDesc.Type)); //ディスクリプタ一つ分のサイズ

			// レンダーターゲットビューの生成
		dev->CreateRenderTargetView(
			backBuffers[i].Get(),
			nullptr,
			handle);
		dev->CreateRenderTargetView(
			backBuffers[i].Get(),
			nullptr,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(
				rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
				i,
				dev->GetDescriptorHandleIncrementSize(heapDesc.Type)
			)
		);
	}

	// フェンスの生成
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceVal = 0;
	result = dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	ComPtr<IDirectInput8> dinput;
	result = DirectInput8Create(
		w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&dinput, nullptr);
	IDirectInputDevice8* devkeyboard = nullptr;
	result = dinput->CreateDevice(GUID_SysKeyboard, &devkeyboard, NULL);
	result = devkeyboard->SetDataFormat(&c_dfDIKeyboard);
	// DirectX初期化処理　ここまで

	//描画初期化処理
	//深度バッファ
	ComPtr<ID3D12Resource> depthBuffer;
	//深度バッファリソース設定
	CD3DX12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		window_width,
		window_height,
		1, 0,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	//深度バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //深度値の書き込みに使用
		&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
		IID_PPV_ARGS(&depthBuffer));

	//深度ビュー用デスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1; //深度ビューは1つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; //デプスステンシルビュー
	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	result = dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; //深度地フォーマット
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dev->CreateDepthStencilView(
		depthBuffer.Get(),
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart());

	//パイプラインセット
	PipeLineSet object3dPipelineSet = Object3dCreateGraphicsPipeline((dev.Get()));
	PipeLineSet spritePipelineSet = SpriteCreateGraphicsPipeline(dev.Get());

	/*--------------------------------------------------------------------スプライト＆オブジェクト作成*/
	//３Dオブジェクト
	//３Dオブジェクト共通データ
	Object3dCommon object3dCommon; //TODO テクスチャ
	//３Dオブジェクト共通データ作成
	object3dCommon = Object3dCommonCreate(dev.Get(), window_width, window_height);
	//３Dオブジェクト共通テクスチャ読み込み
	//Object3dCommonLoadTexture(object3dCommon, 0, L"Resources/StrikeEagle.jpg", dev.Get());

	//スプライト
	//スプライト共通データ
	SpriteCommon spriteCommon;
	//スプライト共通データ生成
	spriteCommon = SpriteCommonCreate(dev.Get(), window_width, window_height);
	//スプライト共通テクスチャ読み込み
	//SpriteCommonLoadTexture(spriteCommon, 0, L"Resources/TitleBack.png", dev.Get());

	/*--------------------------------------------------------------------------------------------------------*/

	while (true)  // ゲームループ
	{
		// メッセージがある？
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg); // キー入力メッセージの処理
			DispatchMessage(&msg); // プロシージャにメッセージを送る
		}

		// ✖ボタンで終了メッセージが来たらゲームループを抜ける
		if (msg.message == WM_QUIT) {
			break;
		}
		// DirectX毎フレーム処理　ここから
		devkeyboard->Acquire();

		/*-------------------------------------------------------------------------------------------------ゲーム処理ここから*/
		//入力処理
		

		//更新処理


		//描画処理



		/*-------------------------------------------------------------------------------------------------ゲーム処理ここまで*/

		//３Dオブジェクトの更新
		/*for (int i = 0; i < o_count; i++)
		{
			Object3dUpdate(object[i], object3dCommon);
		}*/
		//スプライトの更新
		/*for (int i = 0; i < s_count; i++)
		{
			SpriteTransferVertexBuffer(sprite[i], spriteCommon);
		}
		for (int i = 0; i < s_count; i++)
		{
			SpriteUpdate(sprite[i], spriteCommon);
		}*/

		//バックバッファの番号を取得（2つなので0番か1番）
		UINT bbIndex = swapchain->GetCurrentBackBufferIndex();

		// １．表示状態から描画状態に変更
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		// ２.　描画先指定
		//レンダ―ターゲッビュー用ディスクリプタヒープのハンドルを取得
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvH = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
			bbIndex,
			dev->GetDescriptorHandleIncrementSize(heapDesc.Type)
		);
		//深度ステンシルビュー用デスクリプタヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		// ３.　画面クリアD3D12
		float clearColor[] = { 0.0f, 0.0f,1.0f , 0.0f };//青っぽい色
		cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// ４．描画コマンドここから
		//ビューポート領域の設定
		cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, window_width, window_height));
		//シザー矩形の設定
		cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, window_width, window_height));

		//３Dオブジェクト共通コマンド
		/*Object3dCommonBeginDraw(cmdList.Get(), object3dCommon); //TODO コマンド
		for (int i = 0; i < o_count; i++)
		{
			//３Dオブジェクト描画
			if (i == Player)
			{
				Object3dDraw_P(object[i], cmdList.Get(), object3dCommon, dev.Get());
			} 			else if (i >= Wall && i < Bullet)
			{
				Object3dDraw_W(object[i], cmdList.Get(), object3dCommon, dev.Get());
			} 			else if (i >= Bullet && i < Enemy && bullet_live[i - Bullet] == true)
			{
				Object3dDraw_B(object[i], cmdList.Get(), object3dCommon, dev.Get());
			}
			else if (i >= Enemy && enemy_live[i - Enemy] == true)
			{
				Object3dDraw_E(object[i], cmdList.Get(), object3dCommon, dev.Get());
			}
		}

		//スプライト共通コマンド
		SpriteCommonBeginDraw(cmdList.Get(), spriteCommon);
		for (int i = 0; i < s_count; i++)
		{
			//スプライト描画
			if (stage == Title && i == Title)
			{
				SpriteDraw(sprite[i], cmdList.Get(), spriteCommon, dev.Get());
			} 			else if (stage == Explan && i == Explan)
			{
				SpriteDraw(sprite[i], cmdList.Get(), spriteCommon, dev.Get());
			} 			else if (stage == End && i >= End)
			{
				SpriteDraw(sprite[i], cmdList.Get(), spriteCommon, dev.Get());
			} 			else if (stage == Game && i >= Score && i != End)
			{
				SpriteDraw(sprite[i], cmdList.Get(), spriteCommon, dev.Get());
			}
		}*/
		// ４．描画コマンドここまで

		// ５．リソースバリアを戻す
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		// 命令のクローズ
		cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdLists[] = { cmdList.Get() }; // コマンドリストの配列
		cmdQueue->ExecuteCommandLists(1, cmdLists);

		// コマンドリストの実行完了を待つ
		cmdQueue->Signal(fence.Get(), ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		cmdAllocator->Reset(); // キューをクリア

		cmdList->Reset(cmdAllocator.Get(), nullptr);  // 再びコマンドリストを貯める準備

		// バッファをフリップ（裏表の入替え）
		swapchain->Present(1, 0);
		// DirectX毎フレーム処理　ここまで
	}

	// ウィンドウクラスを登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}

//パイプラインの生成（関数）
PipeLineSet Object3dCreateGraphicsPipeline(ID3D12Device* dev)
{
	HRESULT result;

	ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
	ComPtr<ID3DBlob>psBlob; // ピクセルシェーダオブジェクト
	ComPtr<ID3DBlob>errorBlob; // エラーオブジェクト

	//頂点シェーダーの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shaders/BasicVS.hlsl",  // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shaders/BasicPS.hlsl",   // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	//エラー表示
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};	// 1行で書いたほうが見やすい

	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
	//頂点シェーダー、ピクセルシェーダ
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	//標準的な設定(背面カリング、塗りつぶし、深度クリッピング有効)
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//ラスタライザステート
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	//レンダ―ターゲットのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //標準設定
	blenddesc.BlendEnable = true; //ブレンドを有効にする
	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE; //ソースの値を100%使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO; //デストの値を0%使う

	//半透明合成
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; //ソースのアルファ値
	blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; //1.0f-ソースのアルファ値
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1; // 描画対象は1つ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
	gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	//デプスステンシルステートの設定
	//標準的な設定(深度テストを行う、書き込み許可、深度が小さければ合格)
	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//デスクリプタテーブルの設定
	CD3DX12_DESCRIPTOR_RANGE descRangeSRV;
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);//t0レジスタ

	//ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootparams[2];
	rootparams[0].InitAsConstantBufferView(0); //定数バッファビューとして初期化(b0レジスタ)
	rootparams[1].InitAsDescriptorTable(1, &descRangeSRV); //テクスチャ用

	//テクスチャサンプラーの設定
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	PipeLineSet pipelineSet;

	//ルートシグネチャの設定
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> rootSigBlob;
	//バージョン自動判定でのシリアライズ
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	//ルートシグネチャの生成
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));
	// パイプラインにルートシグネチャをセット
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();

	//パイプラインステートの生成
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

	return pipelineSet;
}
//パイプラインの生成（スプライト用関数）
PipeLineSet SpriteCreateGraphicsPipeline(ID3D12Device* dev)
{
	HRESULT result;

	ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
	ComPtr<ID3DBlob>psBlob; // ピクセルシェーダオブジェクト
	ComPtr<ID3DBlob>errorBlob; // エラーオブジェクト

	//頂点シェーダーの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shaders/SpriteVS.hlsl",  // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shaders/SpritePS.hlsl",   // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	//エラー表示
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};	// 1行で書いたほうが見やすい

	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
	//頂点シェーダー、ピクセルシェーダ
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	//標準的な設定(背面カリング、塗りつぶし、深度クリッピング有効)
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); //一旦標準値をリセット
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; //背面カリングなし
	//ラスタライザステート
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	//レンダ―ターゲットのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //標準設定
	blenddesc.BlendEnable = true; //ブレンドを有効にする
	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE; //ソースの値を100%使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO; //デストの値を0%使う

	//半透明合成
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; //ソースのアルファ値
	blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; //1.0f-ソースのアルファ値
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1; // 描画対象は1つ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
	gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	//デプスステンシルステートの設定
	//標準的な設定(深度テストを行う、書き込み許可、深度が小さければ合格)
	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); //一旦標準値をリセット
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; //常に上書きルール
	gpipeline.DepthStencilState.DepthEnable = false; //深度テストをしない
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//デスクリプタテーブルの設定
	CD3DX12_DESCRIPTOR_RANGE descRangeSRV;
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);//t0レジスタ

	//ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootparams[2];
	rootparams[0].InitAsConstantBufferView(0); //定数バッファビューとして初期化(b0レジスタ)
	rootparams[1].InitAsDescriptorTable(1, &descRangeSRV); //テクスチャ用

	//テクスチャサンプラーの設定
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	PipeLineSet pipelineSet;

	//ルートシグネチャの設定
	ComPtr<ID3D12RootSignature> rootsignature;
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> rootSigBlob;
	//バージョン自動判定でのシリアライズ
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	//ルートシグネチャの生成
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));
	// パイプラインにルートシグネチャをセット
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();

	//パイプラインステートの生成
	ComPtr<ID3D12PipelineState> pipelinestate;
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

	return pipelineSet;
}

//スプライト作成
Sprite SpriteCreate(ID3D12Device* dev, int window_width, int window_height, UINT texNumber, const SpriteCommon& spriteCommon, XMFLOAT2 anchorpoint, bool isFilpX, bool isFilpY)
{
	HRESULT result = S_FALSE;

	///新しいスプライトを作る
	Sprite sprite{};

	//頂点データ
	VertexPosUv vertices[] = {
		{{0.0f, 100.0f, 0.0f}, {0.0f, 1.0f}},//左下
		{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},//左上
		{{100.0f, 100.0f, 0.0f}, {1.0f, 1.0f}},//右下
		{{100.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},//右上
	};

	//テクスチャ番号をコピー
	sprite.texNumber = texNumber;

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sprite.vertBuff));

	//指定番号が読み込み済みなら
	if (spriteCommon.texBuff[sprite.texNumber])
	{
		//テクスチャ番号取得
		D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texNumber]->GetDesc();

		//スプライトの大きさを画像の解像度に合わせる
		sprite.size = { (float)resDesc.Width, (float)resDesc.Height };
		sprite.texSize = { (float)resDesc.Width, (float)resDesc.Height };
	}

	//アンカーポイントをコピー
	sprite.anchorpoint = anchorpoint;

	//反転フラグをコピー
	sprite.isFilpX = isFilpX;
	sprite.isFilpY = isFilpY;

	//頂点バッファの転送
	SpriteTransferVertexBuffer(sprite, spriteCommon);

	// 頂点バッファへのデータ転送
	VertexPosUv* vertMap = nullptr;
	result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	sprite.vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	sprite.vbView.BufferLocation = sprite.vertBuff->GetGPUVirtualAddress();
	sprite.vbView.SizeInBytes = sizeof(vertices);
	sprite.vbView.StrideInBytes = sizeof(vertices[0]);

	//定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sprite.constBuff)
	);

	//定数バッファ転送
	ConstBufferData* constMap = nullptr;
	result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	//平行投影行列
	constMap->mat = XMMatrixOrthographicOffCenterLH(
		0.0f, window_width, window_height, 0.0f, 0.0f, 1.0f);
	sprite.constBuff->Unmap(0, nullptr);

	return sprite;
}
//スプライト共通グラフィックコマンドのセット
void SpriteCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon)
{
	//パイプラインとルートシグネチャの設定
	cmdList->SetPipelineState(spriteCommon.pipelineSet.pipelinestate.Get());
	cmdList->SetGraphicsRootSignature(spriteCommon.pipelineSet.rootsignature.Get());

	//プリミティブ形状を設定
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//デスクリプタヒープをセット
	ID3D12DescriptorHeap* ppHeaps[] = { spriteCommon.descHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}
//スプライト単体表示
void SpriteDraw(const Sprite& sprite, ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon, ID3D12Device* dev)
{
	//非表示フラグがtrueなら
	if (sprite.isInvisible)
	{
		//描画せず抜ける
		return;
	}

	//頂点バッファをセット
	cmdList->IASetVertexBuffers(0, 1, &sprite.vbView);

	//定数バッファをセット
	cmdList->SetGraphicsRootConstantBufferView(0, sprite.constBuff->GetGPUVirtualAddress());

	//シェーダリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			spriteCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			sprite.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

	//ポリゴンの描画（４頂点で四角形）
	cmdList->DrawInstanced(4, 1, 0, 0);
}
//スプライト共通データ生成
SpriteCommon SpriteCommonCreate(ID3D12Device* dev, int window_width, int window_height)
{
	HRESULT result = S_FALSE;

	//新たなスプライト共通データを作成
	SpriteCommon spriteCommon{};

	//スプライト用パイプライン生成
	spriteCommon.pipelineSet = SpriteCreateGraphicsPipeline(dev);

	//並行行列の射影行列生成
	spriteCommon.matProjection = XMMatrixOrthographicOffCenterLH(
		0.0f, (float)window_width, (float)window_height, 0.0f, 0.0f, 1.0f);

	//デスクリプタヒープを生成
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NumDescriptors = spriteSRVCount;
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&spriteCommon.descHeap));

	return spriteCommon;
}
//スプライト単体更新
void SpriteUpdate(Sprite& sprite, const SpriteCommon& spriteCommon)
{
	//ワールド行列の更新
	sprite.matWorld = XMMatrixIdentity();
	//Z軸回転
	sprite.matWorld *= XMMatrixRotationZ(XMConvertToRadians(sprite.rotation));
	//平行移動
	sprite.matWorld *= XMMatrixTranslation(sprite.position.x, sprite.position.y, sprite.position.z);

	//定数バッファの転送
	ConstBufferData* constMap = nullptr;
	HRESULT result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->mat = sprite.matWorld * spriteCommon.matProjection;
	constMap->color = sprite.color;
	sprite.constBuff->Unmap(0, nullptr);
}
//スプライト共通テクスチャ読み込み
void SpriteCommonLoadTexture(SpriteCommon& spriteCommon, UINT texNumber, const wchar_t* filename, ID3D12Device* dev)
{
	//異常な番号の指定を検出
	assert(texNumber <= spriteSRVCount - 1);

	HRESULT result;

	//WICテクスチャのロード
	TexMetadata metadate{};
	ScratchImage scratchImg{};
	result = LoadFromWICFile(
		filename, //画像
		WIC_FLAGS_NONE,
		&metadate, scratchImg);
	const Image* img = scratchImg.GetImage(0, 0, 0); //生データ抽出

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadate.format,
		metadate.width,
		(UINT)metadate.height,
		(UINT16)metadate.arraySize,
		(UINT16)metadate.mipLevels);

	//テクスチャバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&spriteCommon.texBuff[texNumber])
	);

	//テクスチャバッファにデータ転送
	result = spriteCommon.texBuff[texNumber]->WriteToSubresource(
		0,
		nullptr, //全領域にコピー
		img->pixels, //元データアドレス
		(UINT)img->rowPitch, //1ラインサイズ
		(UINT)img->slicePitch //全ラインサイズ
	);

	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{}; //設定構造体
	srvDesc.Format = metadate.format; //RGBA
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; //２Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;

	//ヒープのtexnumber番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(
		spriteCommon.texBuff[texNumber].Get(), //ビューと関連付けるバッファ
		&srvDesc, //テクスチャ設定情報
		CD3DX12_CPU_DESCRIPTOR_HANDLE(spriteCommon.descHeap->GetCPUDescriptorHandleForHeapStart(), texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
		)
	);
}
//スプライト単体頂点バッファの転送
void SpriteTransferVertexBuffer(const Sprite& sprite, const SpriteCommon& spriteCommon)
{
	HRESULT result = S_FALSE;

	//頂点データ
	VertexPosUv vertices[] = {
		//			u		  v
		{{}, {0.0f, 1.0f}}, //左下
		{{}, {0.0f, 0.0f}}, //左上
		{{}, {1.0f, 1.0f}}, //右下
		{{}, {1.0f, 0.0f}}, //右上
	};

	//左下、左上、右下、右上
	enum { LB, LT, RB, RT };

	float left = (0.0f - sprite.anchorpoint.x) * sprite.size.x;
	float right = (1.0f - sprite.anchorpoint.x) * sprite.size.x;
	float top = (0.0f - sprite.anchorpoint.y) * sprite.size.y;
	float bottom = (1.0f - sprite.anchorpoint.y) * sprite.size.y;

	if (sprite.isFilpX)
	{
		left = -left;
		right = -right;
	}
	if (sprite.isFilpY)
	{
		top = -top;
		bottom = -bottom;
	}

	if (spriteCommon.texBuff[sprite.texNumber])
	{
		//テクスチャ情報取得
		D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texNumber]->GetDesc();

		float tex_left = sprite.texLeftTop.x / resDesc.Width;
		float tex_right = (sprite.texLeftTop.x + sprite.texSize.x) / resDesc.Width;
		float tex_top = sprite.texLeftTop.y / resDesc.Height;
		float tex_bottom = (sprite.texLeftTop.y + sprite.texSize.y) / resDesc.Height;

		vertices[LB].uv = { tex_left, tex_bottom };
		vertices[LT].uv = { tex_left, tex_top };
		vertices[RB].uv = { tex_right, tex_bottom };
		vertices[RT].uv = { tex_right, tex_top };
	}

	vertices[LB].pos = { left, bottom, 0.0f }; //左下
	vertices[LT].pos = { left, top, 0.0f }; //左上
	vertices[RB].pos = { right, bottom, 0.0f }; //右下
	vertices[RT].pos = { right, top, 0.0f }; //右上

	//頂点バッファへのデータ転送
	VertexPosUv* vertMap = nullptr;
	result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	sprite.vertBuff->Unmap(0, nullptr);
}

//３Dオブジェクト作成
Object3d Object3dCreate_P(ID3D12Device* dev, int window_width, int window_height, UINT texNumber)
{
	HRESULT result = S_FALSE;

	//新しいオブジェクトを作成
	Object3d object3d = {};

	//頂点データ
	Vertex vertices[] = {
		//	{{x, y, z}, {x, y, z}, {u, v}}
		//前
		{{-4, -4, -4}, {}, {0.0f, 1.0f}},//左下
		{{-4, 4, -4}, {}, {0.0f, 0.0f}},//左上 
		{{4, -4, -4}, {}, {1.0f, 1.0f}},//右下
		{{4, 4, -4}, {}, {1.0f, 0.0f}},//右上

		//後
		{{4, -4, 4}, {}, {0.0f, 0.0f}},//右下
		{{4, 4, 4}, {}, {0.0f, 1.0f}},//右上
		{{-4, -4, 4}, {}, {1.0f, 0.0f}},//左下
		{{-4, 4, 4}, {}, {1.0f, 1.0f}},//左上

		//左
		{{-4, -4, -4}, {}, {0.0f, 1.0f}},//左下
		{{-4, -4, 4}, {}, {0.0f, 0.0f}},//左上
		{{-4, 4, -4}, {}, {1.0f, 1.0f}},//右下
		{{-4, 4, 4}, {}, {1.0f, 0.0f}},//右上

		//右
		{{4, 4, -4}, {}, {0.0f, 1.0f}},//右下
		{{4, 4, 4}, {}, {0.0f, 0.0f}},//右上
		{{4, -4, -4}, {}, {1.0f, 1.0f}},//左下
		{{4, -4, 4}, {}, {1.0f, 0.0f}},//左上

		//上
		{{-4, 4, -4}, {}, {0.0f, 1.0f}},//左下
		{{-4, 4, 4}, {}, {0.0f, 0.0f}},//左上
		{{4, 4, -4}, {}, {1.0f, 1.0f}},//右下
		{{4, 4, 4}, {}, {1.0f, 0.0f}},//右上

		//下
		{{4, -4, -4}, {}, {0.0f, 0.0f}},//右下
		{{4, -4, 4}, {}, {0.0f, 1.0f}},//右上
		{{-4, -4, -4}, {}, {1.0f, 0.0f}},//左下
		{{-4, -4, 4}, {}, {1.0f, 1.0f}},//左上
	};

	//インデックスデータ
	unsigned short indices[] = {
		//前
		0, 1, 2, //三角形1つ目
		2, 1, 3, //三角形2つ目

		//後
		4, 5, 6, //三角形1つ目
		6, 5, 7, //三角形2つ目

		//左
		8, 9, 10, //三角形1つ目
		10, 9, 11, //三角形2つ目

		//右
		12, 13, 14, //三角形1つ目
		14, 13, 15, //三角形2つ目

		//上
		16, 17, 18, //三角形1つ目
		18, 17, 19, //三角形2つ目

		//下
		20, 21, 22, //三角形1つ目
		22, 21, 23, //三角形2つ目
	};

	for (int i = 0; i < _countof(indices) / 3; i++)
	{
		//三角形1つごとに計算していく
		//三角形のインデックスを取り出して、一時的な変数に入れる
		unsigned short index_0 = indices[i * 3 + 0];
		unsigned short index_1 = indices[i * 3 + 1];
		unsigned short index_2 = indices[i * 3 + 2];

		//三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[index_0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[index_1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[index_2].pos);

		//p0→p1ベクトル、p0→p2ベクトルを計算 (ベクトルの減算)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);

		//外積は両方から垂直ベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);

		//正規化 (長さを1にする)
		normal = XMVector3Normalize(normal);

		//求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[index_0].normal, normal);
		XMStoreFloat3(&vertices[index_1].normal, normal);
		XMStoreFloat3(&vertices[index_2].normal, normal);
	}

	object3d.texNumber = texNumber;

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object3d.vertBuff));

	//頂点バッファへのデータ転送
	Vertex* vertMap = nullptr;
	result = object3d.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	object3d.vertBuff->Unmap(0, nullptr);

	//頂点バッファビューの作成
	object3d.vbView.BufferLocation = object3d.vertBuff->GetGPUVirtualAddress();
	object3d.vbView.SizeInBytes = sizeof(vertices);
	object3d.vbView.StrideInBytes = sizeof(vertices[0]);

	//インデックスバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.indexBuff));

	//インデックスバッファへのデータ転送
	unsigned short* indexMap = nullptr;
	result = object3d.indexBuff->Map(0, nullptr, (void**)&indexMap);
	memcpy(indexMap, indices, sizeof(indices));
	object3d.indexBuff->Unmap(0, nullptr);

	//インデックスバッファビューの作成
	object3d.ibView.BufferLocation = object3d.indexBuff->GetGPUVirtualAddress();
	object3d.ibView.Format = DXGI_FORMAT_R16_UINT;
	object3d.ibView.SizeInBytes = sizeof(indices);

	//定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.constBuff));

	//定数バッファ転送
	ConstBufferData* constMap = nullptr;
	result = object3d.constBuff->Map(0, nullptr, (void**)&constMap);
	//ワールド行列の更新
	object3d.matWorld = XMMatrixIdentity(); //単位行列
	//拡大行列
	object3d.matWorld *= XMMatrixScaling(object3d.scale.x, object3d.scale.y, object3d.scale.z);
	//回転行列
	object3d.matWorld *= XMMatrixRotationX(XMConvertToRadians(object3d.rotation.x));
	object3d.matWorld *= XMMatrixRotationY(XMConvertToRadians(object3d.rotation.y));
	object3d.matWorld *= XMMatrixRotationZ(XMConvertToRadians(object3d.rotation.z));
	//平行移動行列
	object3d.matWorld *= XMMatrixTranslation(object3d.position.x, object3d.position.y, object3d.position.z);

	//ビューの変換行列
	object3d.matView = XMMatrixLookAtLH(XMLoadFloat3(&object3d.eye), XMLoadFloat3(&object3d.target), XMLoadFloat3(&object3d.up));

	constMap->mat = object3d.matWorld * object3d.matView;
	constMap->color = object3d.color;

	object3d.constBuff->Unmap(0, nullptr);

	return object3d;
}
Object3d Object3dCreate_E(ID3D12Device* dev, int window_width, int window_height, UINT texNumber)
{
	HRESULT result = S_FALSE;

	//新しいオブジェクトを作成
	Object3d object3d = {};

	//頂点データ
	Vertex vertices[] = {
		//	{{x, y, z}, {x, y, z}, {u, v}}
		{{-10, -10, 0}, {}, {0.0f, 1.0f}},//左下0
		{{-10, 10, 0}, {}, {0.0f, 0.0f}},//左上 1
		{{10, -10, 0}, {}, {1.0f, 1.0f}},//右下 2
		{{10, 10, 0}, {}, {1.0f, 0.0f}},//右上 3
	};

	//インデックスデータ
	unsigned short indices[] = {
		//前
		0, 1, 2, //三角形1つ目
		2, 1, 3, //三角形2つ目
	};

	for (int i = 0; i < _countof(indices) / 3; i++)
	{
		//三角形1つごとに計算していく
		//三角形のインデックスを取り出して、一時的な変数に入れる
		unsigned short index_0 = indices[i * 3 + 0];
		unsigned short index_1 = indices[i * 3 + 1];
		unsigned short index_2 = indices[i * 3 + 2];

		//三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[index_0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[index_1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[index_2].pos);

		//p0→p1ベクトル、p0→p2ベクトルを計算 (ベクトルの減算)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);

		//外積は両方から垂直ベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);

		//正規化 (長さを1にする)
		normal = XMVector3Normalize(normal);

		//求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[index_0].normal, normal);
		XMStoreFloat3(&vertices[index_1].normal, normal);
		XMStoreFloat3(&vertices[index_2].normal, normal);
	}

	object3d.texNumber = texNumber;

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object3d.vertBuff));

	//頂点バッファへのデータ転送
	Vertex* vertMap = nullptr;
	result = object3d.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	object3d.vertBuff->Unmap(0, nullptr);

	//頂点バッファビューの作成
	object3d.vbView.BufferLocation = object3d.vertBuff->GetGPUVirtualAddress();
	object3d.vbView.SizeInBytes = sizeof(vertices);
	object3d.vbView.StrideInBytes = sizeof(vertices[0]);

	//インデックスバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.indexBuff));

	//インデックスバッファへのデータ転送
	unsigned short* indexMap = nullptr;
	result = object3d.indexBuff->Map(0, nullptr, (void**)&indexMap);
	memcpy(indexMap, indices, sizeof(indices));
	object3d.indexBuff->Unmap(0, nullptr);

	//インデックスバッファビューの作成
	object3d.ibView.BufferLocation = object3d.indexBuff->GetGPUVirtualAddress();
	object3d.ibView.Format = DXGI_FORMAT_R16_UINT;
	object3d.ibView.SizeInBytes = sizeof(indices);

	//定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.constBuff));

	//定数バッファ転送
	ConstBufferData* constMap = nullptr;
	result = object3d.constBuff->Map(0, nullptr, (void**)&constMap);
	//ワールド行列の更新
	object3d.matWorld = XMMatrixIdentity(); //単位行列
	//拡大行列
	object3d.matWorld *= XMMatrixScaling(object3d.scale.x, object3d.scale.y, object3d.scale.z);
	//回転行列
	object3d.matWorld *= XMMatrixRotationX(XMConvertToRadians(object3d.rotation.x));
	object3d.matWorld *= XMMatrixRotationY(XMConvertToRadians(object3d.rotation.y));
	object3d.matWorld *= XMMatrixRotationZ(XMConvertToRadians(object3d.rotation.z));
	//平行移動行列
	object3d.matWorld *= XMMatrixTranslation(object3d.position.x, object3d.position.y, object3d.position.z);

	//ビューの変換行列
	object3d.matView = XMMatrixLookAtLH(XMLoadFloat3(&object3d.eye), XMLoadFloat3(&object3d.target), XMLoadFloat3(&object3d.up));

	constMap->mat = object3d.matWorld * object3d.matView;
	constMap->color = object3d.color;

	object3d.constBuff->Unmap(0, nullptr);

	return object3d;
}
Object3d Object3dCreate_W(ID3D12Device* dev, int window_width, int window_height, UINT texNumber)
{
	HRESULT result = S_FALSE;

	//新しいオブジェクトを作成
	Object3d object3d = {};

	//頂点データ
	Vertex vertices[] = {
		//	{{x, y, z}, {x, y, z}, {u, v}}
		{{-10, -10, 0}, {}, {0.0f, 0.0f}},//左下0
		{{-10, 10, 0}, {}, {0.0f, 1.0f}},//左上 1
		{{10, -10, 0}, {}, {1.0f, 0.0f}},//右下 2
		{{10, 10, 0}, {}, {1.0f, 1.0f}},//右上 3
	};

	//インデックスデータ
	unsigned short indices[] = {
		//前
		0, 1, 2, //三角形1つ目
		2, 1, 3, //三角形2つ目
	};

	for (int i = 0; i < _countof(indices) / 3; i++)
	{
		//三角形1つごとに計算していく
		//三角形のインデックスを取り出して、一時的な変数に入れる
		unsigned short index_0 = indices[i * 3 + 0];
		unsigned short index_1 = indices[i * 3 + 1];
		unsigned short index_2 = indices[i * 3 + 2];

		//三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[index_0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[index_1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[index_2].pos);

		//p0→p1ベクトル、p0→p2ベクトルを計算 (ベクトルの減算)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);

		//外積は両方から垂直ベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);

		//正規化 (長さを1にする)
		normal = XMVector3Normalize(normal);

		//求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[index_0].normal, normal);
		XMStoreFloat3(&vertices[index_1].normal, normal);
		XMStoreFloat3(&vertices[index_2].normal, normal);
	}

	object3d.texNumber = texNumber;

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object3d.vertBuff));

	//頂点バッファへのデータ転送
	Vertex* vertMap = nullptr;
	result = object3d.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	object3d.vertBuff->Unmap(0, nullptr);

	//頂点バッファビューの作成
	object3d.vbView.BufferLocation = object3d.vertBuff->GetGPUVirtualAddress();
	object3d.vbView.SizeInBytes = sizeof(vertices);
	object3d.vbView.StrideInBytes = sizeof(vertices[0]);

	//インデックスバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.indexBuff));

	//インデックスバッファへのデータ転送
	unsigned short* indexMap = nullptr;
	result = object3d.indexBuff->Map(0, nullptr, (void**)&indexMap);
	memcpy(indexMap, indices, sizeof(indices));
	object3d.indexBuff->Unmap(0, nullptr);

	//インデックスバッファビューの作成
	object3d.ibView.BufferLocation = object3d.indexBuff->GetGPUVirtualAddress();
	object3d.ibView.Format = DXGI_FORMAT_R16_UINT;
	object3d.ibView.SizeInBytes = sizeof(indices);

	//定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.constBuff));

	//定数バッファ転送
	ConstBufferData* constMap = nullptr;
	result = object3d.constBuff->Map(0, nullptr, (void**)&constMap);
	//ワールド行列の更新
	object3d.matWorld = XMMatrixIdentity(); //単位行列
	//拡大行列
	object3d.matWorld *= XMMatrixScaling(object3d.scale.x, object3d.scale.y, object3d.scale.z);
	//回転行列
	object3d.matWorld *= XMMatrixRotationX(XMConvertToRadians(object3d.rotation.x));
	object3d.matWorld *= XMMatrixRotationY(XMConvertToRadians(object3d.rotation.y));
	object3d.matWorld *= XMMatrixRotationZ(XMConvertToRadians(object3d.rotation.z));
	//平行移動行列
	object3d.matWorld *= XMMatrixTranslation(object3d.position.x, object3d.position.y, object3d.position.z);

	//ビューの変換行列
	object3d.matView = XMMatrixLookAtLH(XMLoadFloat3(&object3d.eye), XMLoadFloat3(&object3d.target), XMLoadFloat3(&object3d.up));

	constMap->mat = object3d.matWorld * object3d.matView;
	constMap->color = object3d.color;

	object3d.constBuff->Unmap(0, nullptr);

	return object3d;
}
Object3d Object3dCreate_B(ID3D12Device* dev, int window_width, int window_height, UINT texNumber)
{
	HRESULT result = S_FALSE;

	//新しいオブジェクトを作成
	Object3d object3d = {};

	//頂点データ
	Vertex vertices[] =
	{
		//	{{x, y, z}, {x, y, z}, {u, v}}
		{{0, 12.5, 0}, {}, {0, 0}}, //0
		{{-11.5, 0, -11.5}, {}, {1, 0}},//1
		{{11.5, 0, -11.5}, {}, {0, 1}},//2
		{{11.5, 0, 11.5}, {}, {1, 0}}, //3
		{{-11.5, 0, 11.5}, {}, {1, 1}}, //4
		{{0, -12.5, 0}, {}, {0, 0}}, //5
	};

	//インデックスデータ配列
	unsigned short indices[] =
	{
		1, 0, 2,
		2, 0, 3,
		3, 0, 4,
		4, 0, 1,
		1, 2, 5,
		2, 3, 5,
		3, 4, 5,
		4, 1, 5,
	};

	for (int i = 0; i < _countof(indices) / 3; i++)
	{
		//三角形1つごとに計算していく
		//三角形のインデックスを取り出して、一時的な変数に入れる
		unsigned short index_0 = indices[i * 3 + 0];
		unsigned short index_1 = indices[i * 3 + 1];
		unsigned short index_2 = indices[i * 3 + 2];

		//三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[index_0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[index_1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[index_2].pos);

		//p0→p1ベクトル、p0→p2ベクトルを計算 (ベクトルの減算)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);

		//外積は両方から垂直ベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);

		//正規化 (長さを1にする)
		normal = XMVector3Normalize(normal);

		//求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[index_0].normal, normal);
		XMStoreFloat3(&vertices[index_1].normal, normal);
		XMStoreFloat3(&vertices[index_2].normal, normal);
	}

	object3d.texNumber = texNumber;

	//頂点バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object3d.vertBuff));

	//頂点バッファへのデータ転送
	Vertex* vertMap = nullptr;
	result = object3d.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	object3d.vertBuff->Unmap(0, nullptr);

	//頂点バッファビューの作成
	object3d.vbView.BufferLocation = object3d.vertBuff->GetGPUVirtualAddress();
	object3d.vbView.SizeInBytes = sizeof(vertices);
	object3d.vbView.StrideInBytes = sizeof(vertices[0]);

	//インデックスバッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.indexBuff));

	//インデックスバッファへのデータ転送
	unsigned short* indexMap = nullptr;
	result = object3d.indexBuff->Map(0, nullptr, (void**)&indexMap);
	memcpy(indexMap, indices, sizeof(indices));
	object3d.indexBuff->Unmap(0, nullptr);

	//インデックスバッファビューの作成
	object3d.ibView.BufferLocation = object3d.indexBuff->GetGPUVirtualAddress();
	object3d.ibView.Format = DXGI_FORMAT_R16_UINT;
	object3d.ibView.SizeInBytes = sizeof(indices);

	//定数バッファの生成
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&object3d.constBuff));

	//定数バッファ転送
	ConstBufferData* constMap = nullptr;
	result = object3d.constBuff->Map(0, nullptr, (void**)&constMap);
	//ワールド行列の更新
	object3d.matWorld = XMMatrixIdentity(); //単位行列
	//拡大行列
	object3d.matWorld *= XMMatrixScaling(object3d.scale.x, object3d.scale.y, object3d.scale.z);
	//回転行列
	object3d.matWorld *= XMMatrixRotationX(XMConvertToRadians(object3d.rotation.x));
	object3d.matWorld *= XMMatrixRotationY(XMConvertToRadians(object3d.rotation.y));
	object3d.matWorld *= XMMatrixRotationZ(XMConvertToRadians(object3d.rotation.z));
	//平行移動行列
	object3d.matWorld *= XMMatrixTranslation(object3d.position.x, object3d.position.y, object3d.position.z);
	//ビューの変換行列
	object3d.matView = XMMatrixLookAtLH(XMLoadFloat3(&object3d.eye), XMLoadFloat3(&object3d.target), XMLoadFloat3(&object3d.up));
	constMap->mat = object3d.matWorld * object3d.matView;
	constMap->color = object3d.color;
	object3d.constBuff->Unmap(0, nullptr);

	return object3d;
}
//３Dオブジェクト共通グラフィックコマンドのセット
void Object3dCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon)
{
	//パイプラインとルートシグネチャの設定
	cmdList->SetPipelineState(object3dCommon.pipelineset.pipelinestate.Get());
	cmdList->SetGraphicsRootSignature(object3dCommon.pipelineset.rootsignature.Get());
	//プリミティブ形状を設定
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//デスクリプタヒープの設定
	ID3D12DescriptorHeap* ppHeaps[] = { object3dCommon.descHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}
//３Dオブジェクト単体表示
void Object3dDraw_P(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev)
{
	//非表示フラグがtrueなら
	if (object3d.isInvisible)
	{	//描画せず抜ける
		return;
	}

	//頂点バッファをセット
	cmdList->IASetVertexBuffers(0, 1, &object3d.vbView);

	//インデックスバッファをセット
	cmdList->IASetIndexBuffer(&object3d.ibView);

	//定数バッファをセット
	cmdList->SetGraphicsRootConstantBufferView(0, object3d.constBuff->GetGPUVirtualAddress());

	//シェーダーリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			object3dCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			object3d.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

	//ポリゴン描画
	//_countof(indices)
	cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
void Object3dDraw_E(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev)
{
	//非表示フラグがtrueなら
	if (object3d.isInvisible)
	{	//描画せず抜ける
		return;
	}

	//頂点バッファをセット
	cmdList->IASetVertexBuffers(0, 1, &object3d.vbView);

	//インデックスバッファをセット
	cmdList->IASetIndexBuffer(&object3d.ibView);

	//定数バッファをセット
	cmdList->SetGraphicsRootConstantBufferView(0, object3d.constBuff->GetGPUVirtualAddress());

	//シェーダーリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			object3dCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			object3d.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

	//ポリゴン描画
	//_countof(indices)
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
void Object3dDraw_W(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev)
{
	//非表示フラグがtrueなら
	if (object3d.isInvisible)
	{	//描画せず抜ける
		return;
	}

	//頂点バッファをセット
	cmdList->IASetVertexBuffers(0, 1, &object3d.vbView);

	//インデックスバッファをセット
	cmdList->IASetIndexBuffer(&object3d.ibView);

	//定数バッファをセット
	cmdList->SetGraphicsRootConstantBufferView(0, object3d.constBuff->GetGPUVirtualAddress());

	//シェーダーリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			object3dCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			object3d.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

	//ポリゴン描画
	//_countof(indices)
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
void Object3dDraw_B(const Object3d& object3d, ID3D12GraphicsCommandList* cmdList, const Object3dCommon& object3dCommon, ID3D12Device* dev)
{
	//非表示フラグがtrueなら
	if (object3d.isInvisible)
	{	//描画せず抜ける
		return;
	}

	//頂点バッファをセット
	cmdList->IASetVertexBuffers(0, 1, &object3d.vbView);

	//インデックスバッファをセット
	cmdList->IASetIndexBuffer(&object3d.ibView);

	//定数バッファをセット
	cmdList->SetGraphicsRootConstantBufferView(0, object3d.constBuff->GetGPUVirtualAddress());

	//シェーダーリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			object3dCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			object3d.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

	//ポリゴン描画
	//_countof(indices)
	cmdList->DrawIndexedInstanced(24, 1, 0, 0, 0);
}
//３Dオブジェクト共通データ生成
Object3dCommon Object3dCommonCreate(ID3D12Device* dev, int window_width, int window_height)
{
	HRESULT result = S_FALSE;

	//新たなオブジェクト共通データを生成
	Object3dCommon object3dCommon{};

	//３Dオブジェクト用パイプライン生成
	object3dCommon.pipelineset = Object3dCreateGraphicsPipeline(dev);

	//射影変換行列(透視投影)
	object3dCommon.matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f), // 上下画角60度
		(float)window_width / window_height, // アスペクト比（画面横幅 / 画面縦幅）
		0.1f, 2000.0f // 前端、奥端
	);

	//デスクリプタヒープの生成
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NumDescriptors = object3dSRVCount;
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&object3dCommon.descHeap));

	return object3dCommon;
}
//３Dオブジェクト単体更新
void Object3dUpdate(Object3d& object3d, const Object3dCommon& object3dCommon)
{
	//ワールド行列の更新
	object3d.matWorld = XMMatrixIdentity(); //単位行列
	//拡大行列
	object3d.matWorld *= XMMatrixScaling(object3d.scale.x, object3d.scale.y, object3d.scale.z);
	//回転行列
	object3d.matWorld *= XMMatrixRotationX(XMConvertToRadians(object3d.rotation.x));
	object3d.matWorld *= XMMatrixRotationY(XMConvertToRadians(object3d.rotation.y));
	object3d.matWorld *= XMMatrixRotationZ(XMConvertToRadians(object3d.rotation.z));
	//平行移動行列
	object3d.matWorld *= XMMatrixTranslation(object3d.position.x, object3d.position.y, object3d.position.z);

	//ビューの変換行列
	object3d.matView = XMMatrixLookAtLH(XMLoadFloat3(&object3d.eye), XMLoadFloat3(&object3d.target), XMLoadFloat3(&object3d.up));

	///定数バッファ転送
	ConstBufferData* constMap = nullptr;
	HRESULT result = object3d.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->mat = object3d.matWorld * object3d.matView * object3dCommon.matProjection;
	constMap->color = object3d.color;
	object3d.constBuff->Unmap(0, nullptr);
}
//３Dオブジェクト共通テクスチャ読み込み
void Object3dCommonLoadTexture(Object3dCommon& object3dCommon, UINT texnumber, const wchar_t* filename, ID3D12Device* dev)
{
	//異常な番号の指定
	assert(texnumber <= spriteSRVCount - 1);

	HRESULT result;

	//WICでテクスチャのロード
	TexMetadata metadate{};
	ScratchImage scratchImg{};
	result = LoadFromWICFile(
		filename, //画像
		WIC_FLAGS_NONE,
		&metadate, scratchImg);
	const Image* img = scratchImg.GetImage(0, 0, 0); //生データ抽出

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadate.format,
		metadate.width,
		(UINT)metadate.height,
		(UINT16)metadate.arraySize,
		(UINT16)metadate.mipLevels);

	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&object3dCommon.texBuff[texnumber]));

	//テクスチャバッファにデータ転送
	result = object3dCommon.texBuff[texnumber]->WriteToSubresource(
		0,
		nullptr, //全領域にコピー
		img->pixels, //元データアドレス
		(UINT)img->rowPitch, //1ラインサイズ
		(UINT)img->slicePitch //全ラインサイズ
	);

	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{}; //設定構造体
	srvDesc.Format = metadate.format; //RGBA
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; //２Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;

	//ヒープのtexnumber番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(
		object3dCommon.texBuff[texnumber].Get(), //ビューと関連付けるバッファ
		&srvDesc, //テクスチャ設定情報
		CD3DX12_CPU_DESCRIPTOR_HANDLE(object3dCommon.descHeap->GetCPUDescriptorHandleForHeapStart(), texnumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	);
}