# include <Siv3D.hpp>

enum mode {
	NONE,
	MOVIE,
	PICTURE,
	SV
};

//モニターの表示の拡大率を計算する
inline double CalculateScale(const Vec2& baseSize, const Vec2& currentSize)
{
	return Min((currentSize.x / baseSize.x), (currentSize.y / baseSize.y));
}

void Main()
{
	//Window::Resize(1280, 720);
	System::SetTerminationTriggers(UserAction::NoAction);
	Window::SetToggleFullscreenEnabled(false);
	Window::SetTitle(U"4次元ストリートビュー");
	Scene::SetResizeMode(ResizeMode::Actual);
	Window::Resize(600, 600);

	//背景設定
	const MSRenderTexture renderTexture{ Scene::Size(), TextureFormat::R8G8B8A8_Unorm_SRGB };
	const ColorF backgroundColor = ColorF{ 0.0, 0.0, 0.0 }.removeSRGBCurve();
	const ColorF ambientColor = ColorF{ 1.0,1.0,1.0 };
	const ColorF sunColor = ColorF{ 0,0,0 };
	Graphics3D::SetSunColor(sunColor);
	Graphics3D::SetGlobalAmbientColor(ambientColor);

	//カメラ設定
	Vec3 eyePosition{ 0, 0, 0 };
	Vec3 focusPosition{ 1, 0, 0 };
	BasicCamera3D camera{ renderTexture.size(), 90_deg, eyePosition, focusPosition };

	//画像や動画から読み込んだフレームを格納する
	Image frameImage{ 100, 100, Color{ 0, 0, 0 } };
	DynamicTexture frameTexture = DynamicTexture{ frameImage ,TextureFormat::R8G8B8A8_Unorm_SRGB };

	//4dsv用
	Array<VideoReader> readers;  //読み込んだ動画群
	Vec3 cameraArrangement;
	Vec3 indexMovement;          //x,y,zそれぞれの方向に移動するときどれくらいインデックスを動かせばいいか
	Vec3 currentPosition;
	int32 svIndex = 0;           //readersのどの位置の動画を読み見込むかのインデックス

	//動画再生用の変数
	double frameDeltaSec = 0.0;
	double frameTimeSec = 0.0;
	double frameCount = 0.0;
	bool isPlay = false;
	bool isDragSlider = false;
	bool isDragSliderPrevious = false;
	bool loop = true;

	//回転量
	double roll = 0.0;
	double pitch = 0.0;
	double yaw = 0.0;

	//現在のモード
	mode active = NONE;

	//回転操作用の軸
	Vec3 x_axis = Vec3{ 0,0,-1 };
	Vec3 y_axis = Vec3{ 0,1,0 };
	Vec3 z_axis = Vec3{ 1,0,0 };
	Quaternion pre_spin = Quaternion::Zero();
	Quaternion spin;

	//マウスホイールでの拡大縮小
	double fovy = 90_deg;					//カメラの視野角
	constexpr double zoom_quantity = 1_deg; //１回回転させたときの拡大量
	int32 zoom_count = 0;					//何回回転させたか

	while (System::Update())
	{
		ClearPrint();
		const double scale = CalculateScale(Window::GetState().virtualSize, Scene::Size());
		const Vec2 frameSize = Window::GetState().frameBufferSize;
		Graphics3D::SetCameraTransform(camera);

		//ファイルオープン
		if ((KeyControl + KeyO).down() || (KeyCommand + KeyO).down())
		{
			isPlay = false;
			readers.clear();
			svIndex = 0;
			if (const auto path = Dialog::OpenFile({ FileFilter::AllFiles() }))
			{
				//1つの360度動画を見るとき
				for (const auto format : FileFilter::AllVideoFiles().patterns)
				{
					if (format == FileSystem::Extension(*path))
					{
						//video = VideoReader{ *path };
						readers.push_back(VideoReader{ *path });
						{
							readers[svIndex].readFrame(frameImage);
							frameImage.mirror();
							frameTexture = DynamicTexture{ frameImage ,TextureFormat::R8G8B8A8_Unorm_SRGB };
							frameDeltaSec = readers[svIndex].getFrameDeltaSec();
							frameTimeSec = 0.0;
							active = MOVIE;
						}
					}
				}

				//360度画像を見るとき
				for (const auto format : FileFilter::AllImageFiles().patterns)
				{
					if (format == FileSystem::Extension(*path))
					{
						frameImage = Image{ *path };
						frameImage.mirror();
						frameTexture = DynamicTexture{ frameImage ,TextureFormat::R8G8B8A8_Unorm_SRGB };
						active = PICTURE;
					}
				}
				//4次元ストリートビュー
				if (FileSystem::Extension(*path) == U"4dsv")
				{
					TextReader reader{ *path };
					if (reader)
					{
						Array<String> line;
						String movieDirectory;
						String extension;
						String arrangement;
						String initialPosition;
						String frameRate;

						reader.readLine(movieDirectory);
						reader.readLine(extension);
						reader.readLine(arrangement);
						reader.readLine(initialPosition);
						reader.readLine(frameRate);

						//相対パスの場合絶対パスに変換する
						if (not movieDirectory.starts_with(U"C:") &&
							not movieDirectory.starts_with(U"~/"))
						{
							movieDirectory = *path + U"/" + movieDirectory;
						}
						
						for (const auto& moviePath : FileSystem::DirectoryContents(movieDirectory, Recursive::No))
						{
							//.4dsvファイルの情報から動画のパスを格納していく
							if (FileSystem::Extension(moviePath) == extension)
							{
								//pathes.push_back(moviePath);
								//VideoReader* a = new VideoReader{ moviePath };
								readers.push_back(VideoReader{ moviePath });
							}
						}
						//カメラ配置を読み込みキーボード操作時のインデックス操作を計算
						line = arrangement.split(U' ');
						cameraArrangement = Vec3{ Parse<int32>(line[0]) ,Parse<int32>(line[1]), Parse<int32>(line[2]) };
						indexMovement = Vec3{ 1,cameraArrangement.x,cameraArrangement.x * cameraArrangement.y };

						//初期位置の読み込みとインデックスの計算
						line = initialPosition.split(U' ');
						currentPosition = Vec3{ Parse<int32>(line[0]) ,Parse<int32>(line[1]), Parse<int32>(line[2]) };
						svIndex = currentPosition.dot(indexMovement);
						if (not InRange(svIndex, 0, int32(cameraArrangement.x*cameraArrangement.y*cameraArrangement.z - 1)))
						{
							Console << U"invalid position";
							continue;
						}
						//video = readers[svIndex];
						readers[svIndex].readFrame(frameImage);
						frameImage.mirror();
						frameTexture = DynamicTexture{ frameImage ,TextureFormat::R8G8B8A8_Unorm_SRGB };
						frameDeltaSec = 1.0 / Parse<double>(frameRate);
						frameTimeSec = 0.0;
						active = SV;
					}
				}

			}

		}
		// 3D 描画
		{
			//const ScopedRenderStates3D rastetrizer{ (RasterizerState::SolidCullNone) };
			const ScopedRenderTarget3D target{ renderTexture.clear(backgroundColor) };
			const ScopedRenderStates3D rastetrizer{ (RasterizerState::SolidCullNone) };
			const Transformer3D transform2{ Mat4x4{ pre_spin } };
			Sphere{ 0,0,0,5 }.draw(frameTexture);
		}
		// 3D シーンを 2D シーンに描画
		{
			Graphics3D::Flush();
			renderTexture.resolve();
			Shader::LinearToScreen(renderTexture);
		}

		//4dsvの動画切り替え
		if (active == SV)
		{
			//+x方向へ移動
			if (KeyRight.down())
			{
				if ((currentPosition.x + 1) < cameraArrangement.x)
				{
					currentPosition.x += 1;
					svIndex += 1;
					readers[svIndex].setCurrentFrameIndex(int32(frameCount));
					readers[svIndex].readFrame(frameImage);
					frameImage.mirror();
					frameTexture.fill(frameImage);

				}
			}
			//-x方向へ移動
			if (KeyLeft.down())
			{
				if ((currentPosition.x - 1) >= 0)
				{
					currentPosition.x -= 1;
					svIndex -= 1;
					readers[svIndex].setCurrentFrameIndex(int32(frameCount));
					readers[svIndex].readFrame(frameImage);
					frameImage.mirror();
					frameTexture.fill(frameImage);
				}
			}
			//+y方向へ移動
			if (KeyUp.down())
			{
				if ((currentPosition.y + 1) < cameraArrangement.y)
				{
					currentPosition.y += 1;
					svIndex += indexMovement.y;
					readers[svIndex].setCurrentFrameIndex(int32(frameCount));
					readers[svIndex].readFrame(frameImage);
					frameImage.mirror();
					frameTexture.fill(frameImage);
				}
			}
			//-y方向へ移動
			if (KeyDown.down())
			{
				if ((currentPosition.y - 1) >= 0)
				{
					currentPosition.y -= 1;
					svIndex -= indexMovement.y;
					readers[svIndex].setCurrentFrameIndex(int32(frameCount));
					readers[svIndex].readFrame(frameImage);
					frameImage.mirror();
					frameTexture.fill(frameImage);
				}
			}
			//+z方向へ移動
			if (KeyComma.down())
			{
				if ((currentPosition.z + 1) < cameraArrangement.z)
				{
					currentPosition.z += 1;
					svIndex += indexMovement.z;
					readers[svIndex].setCurrentFrameIndex(int32(frameCount));
					readers[svIndex].readFrame(frameImage);
					frameImage.mirror();
					frameTexture.fill(frameImage);
				}
			}
			//-z方向へ移動
			if (KeyPeriod.down())
			{
				if ((currentPosition.z - 1) >= 0)
				{
					currentPosition.z -= 1;
					svIndex -= indexMovement.z;
					readers[svIndex].setCurrentFrameIndex(int32(frameCount));
					readers[svIndex].readFrame(frameImage);
					frameImage.mirror();
					frameTexture.fill(frameImage);
				}
			}
		}
		//エンターを押すと１フレーム進む
		if (KeyEnter.down() && not isPlay && not readers.empty() && not readers[svIndex].reachedEnd())
		{
			//最後のフレームだけ反転してしまうため応急処置
			if (readers[svIndex].getCurrentFrameIndex() != readers[svIndex].getFrameCount()-1)
			{
				readers[svIndex].readFrame(frameImage);
				frameImage.mirror();
			}
			else
			{
				readers[svIndex].setCurrentFrameIndex(readers[svIndex].getFrameCount());
				readers[svIndex].readFrame(frameImage);
			}
			frameTexture.fill(frameImage);
			frameCount += 1.0;
		}
		if (KeyL.down())
		{
			loop = !loop;
		}
		//拡大操作
		{
			zoom_count -= Mouse::Wheel();
			zoom_count = Clamp(zoom_count, 0, 45);
			camera.setProjection(camera.getSceneSize(), fovy - zoom_count * zoom_quantity);
		}
		//GUI配置
		if (not readers.empty())
		{
			Transformer2D trans{ Mat3x2::Scale(scale / 1.8,Vec2{0,0}),TransformCursor::Yes };
			{
				Transformer2D trans2{ Mat3x2::Translate(Vec2{900,25}),TransformCursor::Yes };
				SimpleGUI::CheckBox(loop, U"Loop play", Vec2{ 0,0 }, unspecified, true);
			}

			isDragSliderPrevious = isDragSlider;
			{
				Transformer2D trans3{ Mat3x2::Translate(Vec2{820,1000}),TransformCursor::Yes };
				isDragSlider = SimpleGUI::Slider(U"frame:{}"_fmt(int32(frameCount)), frameCount, 0, readers[svIndex].getFrameCount() - 1, Vec2{ 0,0 }, 110);
			}
			//スライダーを動かすときは再生を中断
			if (isPlay && isDragSlider)
			{
				readers[svIndex].setCurrentFrameIndex(int32(frameCount));
				isPlay = false;
			}
			//スライダーを離したときに読み込み
			if (not isPlay && not isDragSlider && isDragSliderPrevious)
			{
				readers[svIndex].setCurrentFrameIndex(int32(frameCount));
				readers[svIndex].readFrame(frameImage);
				frameImage.mirror();
				frameTexture.fill(frameImage);
			}
		}
		//時間を進める
		if (isPlay)
		{
			frameTimeSec += Scene::DeltaTime();

			if (readers[svIndex].reachedEnd() && not isDragSlider)
			{
				if (loop)
				{
					readers[svIndex].setCurrentFrameIndex(0);
				}
				else
				{
					isPlay = false;
				}
			}
		}

		//動画再生時のフレーム読み込み
		if (not readers.empty() && (frameDeltaSec <= frameTimeSec))
		{
			readers[svIndex].readFrame(frameImage);
			frameCount = readers[svIndex].getCurrentFrameIndex() - 1;
			//frameCount += 1.0;
			frameImage.mirror();
			frameTexture.fill(frameImage);
			frameTimeSec -= frameDeltaSec;
		}
		//動画の再生と停止
		if (KeySpace.down())
		{
			isPlay = (not isPlay);

			if (isPlay && readers[svIndex].reachedEnd() && (not isDragSlider)) //動画の終わりまで行ったら0フレーム目に戻す
			{
				readers[svIndex].setCurrentFrameIndex(0);
				frameCount = readers[svIndex].getCurrentFrameIndex();
			}
		}

		//回転操作と計算（調整する）
		if (MouseL.pressed() && not MouseL.down() && not isDragSlider)
		{
			//画面の中心から離れるほど移動が少なくなる(横と縦方向)
			pitch = Cursor::DeltaF().y / (1.0 + ((Math::Abs(Cursor::PosF().x - Scene::CenterF().x)) / Scene::Size().maxComponent()));
			roll = Cursor::DeltaF().x / (1.0 + ((Math::Abs(Cursor::PosF().y - Scene::CenterF().y)) / Scene::Size().maxComponent()));

			//外積から回転の向きを求める
			//画面の中心から離れるほど回転が多くなる(z軸からの)
			Vec2 prePos = Vec2{ Cursor::PreviousPosF() - Scene::CenterF() }.normalize();
			Vec2 currentPos = Vec2{ Cursor::PosF() - Scene::CenterF() }.normalize();
			yaw = currentPos.cross(prePos) * Vec2 { Cursor::PosF() - Scene::CenterF() }.lengthSq() / Scene::Size().maxComponent();

			spin = Quaternion::RotationAxis(x_axis, pitch * 0.15_deg * (90 - zoom_count) / 90) *
				Quaternion::RotationAxis(y_axis, roll * 0.15_deg * (90 - zoom_count) / 90) *
				Quaternion::RotationAxis(z_axis, yaw * 0.3_deg * (90 - zoom_count) / 90);

		}

		//回転させる
		if (pre_spin.isZero())
		{
			pre_spin = spin;
		}
		else
		{
			pre_spin *= spin;
		}

		//左上に情報表示
		if (active == NONE)
		{
			Print << U"Press Control/Command + O to open file";
		}
		if (active == SV)
		{
			Print << U"File: " << FileSystem::FileName(readers[svIndex].path());
			Print << U"Dimension " << cameraArrangement.x << U"×"
				<< cameraArrangement.y << U"×"
				<< cameraArrangement.z;
			Print << U"Position " << currentPosition;
		}
		if (active == MOVIE)
		{
			Print << U"File: " << FileSystem::FileName(readers[svIndex].path());
		}

		//OpenSiv3Dのライセンス表示
		if ((KeyControl+KeyL).down() || (KeyCommand+KeyL).down())
		{
			LicenseManager::ShowInBrowser();
		}

		//終了処理
		if (System::GetUserActions() == UserAction::CloseButtonClicked
			|| KeyEscape.down() || KeyQ.down())
		{
			ClearPrint();
			readers.clear();
			System::Exit();
		}
	}

}
