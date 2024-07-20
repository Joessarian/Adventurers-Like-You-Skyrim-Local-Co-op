#pragma once
#include "DebugAPI.h"
#include <GlobalCoopData.h>
#include <Util.h>
#include <windows.h>

using SteadyClock = std::chrono::steady_clock;
namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();
	// Full credits to Shrimperator and ersh1.
	// Code was pieced together from the mods BTPS:
	// https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection
	// and TrueHUD:
	// https://github.com/ersh1/TrueHUD
	std::vector<std::unique_ptr<DebugAPILine>> DebugAPI::linesToDraw;
	std::vector<std::unique_ptr<DebugAPIPoint>> DebugAPI::pointsToDraw;
	std::vector<std::unique_ptr<DebugAPIShape>> DebugAPI::shapesToDraw;
	bool DebugAPI::cachedMenuData;
	float DebugAPI::screenResX;
	float DebugAPI::screenResY;
	std::vector<std::string> DebugOverlayMenu::hiddenSources;

	DebugAPILine::DebugAPILine() :
		from(0.0f),
		to(0.0f),
		rgba(0xFFFFFFFF),
		lineThickness(1.0f),
		durationSecs(0.0f),
		requestTimestamp(SteadyClock::now())
	{ }

	DebugAPILine::DebugAPILine(glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_lineThickness, float a_durationSecs)
	{
		from = a_from;
		to = a_to;
		rgba = a_rgba;
		lineThickness = a_lineThickness;
		durationSecs = a_durationSecs;
		requestTimestamp = SteadyClock::now();
	}

	DebugAPIPoint::DebugAPIPoint() :
		center(0.0f),
		rgba(0xFFFFFFFF),
		size(1.0f),
		durationSecs(0.0f),
		requestTimestamp(SteadyClock::now())
	{ }

	DebugAPIPoint::DebugAPIPoint(glm::vec2 a_center, uint32_t a_rgba, float a_size, float a_durationSecs)
	{
		center = a_center;
		rgba = a_rgba;
		size = a_size;
		durationSecs = a_durationSecs;
		requestTimestamp = SteadyClock::now();
	}

	DebugAPIShape::DebugAPIShape() :
		origin(0.0f),
		offsets({}),
		rgba(0xFFFFFFFF),
		fill(true),
		lineThickness(1.0f),
		durationSecs(0.0f),
		requestTimestamp(SteadyClock::now())
	{ }

	DebugAPIShape::DebugAPIShape(glm::vec2 a_origin, std::vector<glm::vec2> a_offsets, uint32_t a_rgba, bool a_fill, float a_lineThickness, float a_durationSecs)
	{
		origin = a_origin;
		offsets = a_offsets;
		rgba = a_rgba;
		fill = a_fill;
		lineThickness = a_lineThickness;
		durationSecs = a_durationSecs;
		requestTimestamp = SteadyClock::now();
	}

	void DebugAPI::Update()
	{
		auto hud = GetHUD();
		if (!hud || !hud->uiMovie)
		{
			DebugOverlayMenu::Load();
			logger::error("[ALYSLC] ERR: could not get HUD.");
			return;
		}

		CacheMenuData();
		ClearOverlay(hud->uiMovie);

		float lifetimeSecs = 0.0f;
		for (int i = 0; i < pointsToDraw.size(); i++)
		{
			auto& point = pointsToDraw[i];
			if (!point || !point.get())
			{
				logger::error("[ALYSLC] ERR: Could not get point to draw.");
				return;
			}

			DrawPoint(hud->uiMovie, point->center, point->rgba, point->size);
			lifetimeSecs = Util::GetElapsedSeconds(point->requestTimestamp);
			// Erase zero-duration or expired points.
			if (point->durationSecs == 0.0f || lifetimeSecs > point->durationSecs)
			{
				pointsToDraw.erase(pointsToDraw.begin() + i);
				--i;
				continue;
			}
		}

		for (int i = 0; i < linesToDraw.size(); i++)
		{
			auto& line = linesToDraw[i];
			if (!line || !line.get())
			{
				logger::error("[ALYSLC] ERR: Could not get line to draw.");
				return;
			}

			DrawLine(hud->uiMovie, line->from, line->to, line->rgba, line->lineThickness);
			lifetimeSecs = Util::GetElapsedSeconds(line->requestTimestamp);
			// Erase zero-duration or expired lines.
			if (line->durationSecs == 0.0f || lifetimeSecs > line->durationSecs)
			{
				linesToDraw.erase(linesToDraw.begin() + i);
				--i;
				continue;
			}
		}

		for (int i = 0; i < shapesToDraw.size(); i++)
		{
			auto& shape = shapesToDraw[i];
			if (!shape || !shape.get())
			{
				logger::error("[ALYSLC] ERR: Could not get shape to draw.");
				return;
			}

			DrawShape(hud->uiMovie, shape->origin, shape->offsets, shape->rgba, shape->fill, shape->lineThickness, shape->durationSecs);
			lifetimeSecs = Util::GetElapsedSeconds(shape->requestTimestamp);
			// Erase zero-duration or expired shapes.
			if (shape->durationSecs == 0.0f || lifetimeSecs > shape->durationSecs)
			{
				shapesToDraw.erase(shapesToDraw.begin() + i);
				--i;
				continue;
			}
		}
	}

	void DebugAPI::QueueArrow2D(glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_headLength, float a_lineThickness, float a_durationSecs)
	{
		glm::vec2 arrowRay = a_to - a_from;
		float arrowAng = atan2f(arrowRay.y, arrowRay.x);
		// Converging lines at 45 degrees that make up the tip of the arrow.
		float headRay1Ang = Util::NormalizeAng0To2Pi(arrowAng - PI / 4.0f);
		float headRay2Ang = Util::NormalizeAng0To2Pi(arrowAng + PI / 4.0f);
		glm::vec2 headRay1 = a_to - (a_headLength * glm::vec2(cosf(headRay1Ang), sinf(headRay1Ang)));
		glm::vec2 headRay2 = a_to - (a_headLength * glm::vec2(cosf(headRay2Ang), sinf(headRay2Ang)));
		linesToDraw.push_back(std::make_unique<DebugAPILine>(a_from, a_to, a_rgba, a_lineThickness, a_durationSecs));
		linesToDraw.push_back(std::make_unique<DebugAPILine>(headRay1, a_to, a_rgba, a_lineThickness, a_durationSecs));
		linesToDraw.push_back(std::make_unique<DebugAPILine>(headRay2, a_to, a_rgba, a_lineThickness, a_durationSecs));
	}

	void DebugAPI::QueueArrow3D(glm::vec3 a_from, glm::vec3 a_to, uint32_t a_rgba, float a_headLength, float a_lineThickness, float a_durationSecs)
	{
		glm::vec3 arrowRay = a_to - a_from;
		RE::NiPoint3 fromPoint = ToNiPoint3(a_from);
		RE::NiPoint3 toPoint = ToNiPoint3(a_to);
		float arrowYaw = Util::GetYawBetweenPositions(fromPoint, toPoint);
		float headRayPitch = -Util::GetPitchBetweenPositions(fromPoint, toPoint);
		// Converging lines at 45 degrees that make up the tip of the arrow.
		float headRayYaw1 = Util::ConvertAngle(Util::NormalizeAng0To2Pi(arrowYaw - PI / 4.0f));
		float headRayYaw2 = Util::ConvertAngle(Util::NormalizeAng0To2Pi(arrowYaw + PI / 4.0f));
		RE::NiPoint3 headRay1NiP3 = Util::RotationToDirectionVect(headRayPitch, headRayYaw1);
		RE::NiPoint3 headRay2NiP3 = Util::RotationToDirectionVect(headRayPitch, headRayYaw2);
		glm::vec3 headRay1 = a_to - (a_headLength * glm::vec3(headRay1NiP3.x, headRay1NiP3.y, headRay1NiP3.z));
		glm::vec3 headRay2 = a_to - (a_headLength * glm::vec3(headRay2NiP3.x, headRay2NiP3.y, headRay2NiP3.z));

		// Body ray.
		glm::vec2 from = WorldToScreenPoint(a_from);
		glm::vec2 to = WorldToScreenPoint(a_to);
		linesToDraw.push_back(std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs));
		// Head 1.
		from = WorldToScreenPoint(headRay1);
		to = WorldToScreenPoint(a_to);
		linesToDraw.push_back(std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs));
		// Head 2.
		from = WorldToScreenPoint(headRay2);
		to = WorldToScreenPoint(a_to);
		linesToDraw.push_back(std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs));
	}

	void DebugAPI::QueueCircle2D(glm::vec2 a_center, uint32_t a_rgba, uint32_t a_segments, float a_radius, float a_lineThickness, float a_durationSecs)
	{
		const float angleDelta = 2.0f * PI / a_segments;
		// Right and down.
		glm::vec2 xAxis = glm::vec2(1.0f, 0.0f);
		glm::vec2 yAxis = glm::vec2(0.0f, 1.0f);
		// Connect this vertex to the next one when drawing lines.
		glm::vec2 lastVertex = a_center + xAxis * a_radius;
		for (uint32_t sideIndex = 0; sideIndex < a_segments; sideIndex++)
		{
			glm::vec2 vertex = a_center + (xAxis * cosf(angleDelta * (sideIndex + 1)) + yAxis * sinf(angleDelta * (sideIndex + 1))) * a_radius;
			linesToDraw.push_back(std::make_unique<DebugAPILine>(lastVertex, vertex, a_rgba, a_lineThickness, a_durationSecs));
			lastVertex = vertex;
		}
	}

	void DebugAPI::QueueCircle3D(glm::vec3 a_center, uint32_t a_rgba, uint32_t a_segments, float a_radius, float a_lineThickness, float a_durationSecs)
	{
		const float angleDelta = 2.0f * PI / a_segments;
		// Right and down.
		glm::vec2 right = glm::vec2(1.0f, 0.0f);
		glm::vec2 up = glm::vec2(0.0f, 1.0f);
		glm::vec2 center2D = WorldToScreenPoint(a_center);
		// Connect this vertex to the next one when drawing lines.
		glm::vec2 lastVertex = center2D + right * a_radius;

		for (uint32_t sideIndex = 0; sideIndex < a_segments; sideIndex++)
		{
			glm::vec2 vertex = center2D + (right * cosf(angleDelta * (sideIndex + 1)) + up * sinf(angleDelta * (sideIndex + 1))) * a_radius;
			QueueLine2D(lastVertex, vertex, a_rgba, a_lineThickness, a_durationSecs);
			lastVertex = vertex;
		}
	}

	void DebugAPI::QueueLine2D(glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_lineThickness, float a_durationSecs)
	{
		linesToDraw.push_back(std::make_unique<DebugAPILine>(a_from, a_to, a_rgba, a_lineThickness, a_durationSecs));
	}

	void DebugAPI::QueueLine3D(glm::vec3 a_from, glm::vec3 a_to, uint32_t a_rgba, float a_lineThickness, float a_durationSecs)
	{
		glm::vec2 from = WorldToScreenPoint(a_from);
		glm::vec2 to = WorldToScreenPoint(a_to);
		linesToDraw.push_back(std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs));
	}

	void DebugAPI::QueuePoint2D(glm::vec2 a_center, uint32_t a_rgba, float a_size, float a_durationSecs)
	{
		pointsToDraw.push_back(std::make_unique<DebugAPIPoint>(a_center, a_rgba, a_size, a_durationSecs));
	}

	void DebugAPI::QueuePoint3D(glm::vec3 a_center, uint32_t a_rgba, float a_size, float a_durationSecs)
	{
		glm::vec2 center = WorldToScreenPoint(a_center);
		pointsToDraw.push_back(std::make_unique<DebugAPIPoint>(center, a_rgba, a_size, a_durationSecs));
	}

	void DebugAPI::QueueShape2D(const glm::vec2& a_origin, const std::vector<glm::vec2>& a_offsets, const uint32_t& a_rgba, bool&& a_fill, const float& a_lineThickness, const float& a_durationSecs)
	{
		shapesToDraw.push_back(std::make_unique<DebugAPIShape>(a_origin, a_offsets, a_rgba, a_fill, a_lineThickness, a_durationSecs));
	}

	void DebugAPI::RotateLine2D(std::pair<glm::vec2, glm::vec2>& a_line, const glm::vec2& a_pivotPoint, const float& a_ang)
	{
		a_line.first = a_line.first - a_pivotPoint;
		a_line.second = a_line.second - a_pivotPoint;
		// https://en.wikipedia.org/wiki/Rotation_matrix
		// Counter-clockwise about origin.
		// First column, second column.
		const glm::mat2 rotMat{
			cosf(a_ang), -sinf(a_ang),
			sinf(a_ang), cosf(a_ang)
		};

		a_line.first = (rotMat * a_line.first) + a_pivotPoint;
		a_line.second = (rotMat * a_line.second) + a_pivotPoint;
	}

	void DebugAPI::RotateLine3D(std::pair<glm::vec4, glm::vec4>& a_line, const glm::vec4& a_pivotPoint, const float& a_pitch, const float& a_yaw)
	{
		a_line.first = a_line.first - a_pivotPoint;
		a_line.second = a_line.second - a_pivotPoint;
		auto rightAxis = glm::vec3(1.0f, 0.0f, 0.0f);
		auto upAxis = glm::vec3(0.0f, 0.0f, 1.0f);

		// Credits to mwilsnd for the rotation matrix construction:
		// https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/mmath.cpp#L222
		glm::mat4 rotMat = glm::identity<glm::mat4>();
		rotMat = glm::rotate(rotMat, -a_yaw, glm::vec3(0.0f, 0.0f, 1.0f));
		rotMat = glm::rotate(rotMat, -a_pitch, glm::vec3(1.0f, 0.0f, 0.0f));

		a_line.first = (rotMat * a_line.first) + a_pivotPoint;
		a_line.second = (rotMat * a_line.second) + a_pivotPoint;
	}

	void DebugAPI::RotateOffsetPoints2D(std::vector<glm::vec2>& a_points, const float& a_ang)
	{
		// https://en.wikipedia.org/wiki/Rotation_matrix
		// Counter-clockwise about origin.
		// First column, second column.
		const glm::mat2 rotMat{
			cosf(a_ang), -sinf(a_ang),
			sinf(a_ang), cosf(a_ang)
		};

		for (auto& point : a_points)
		{
			point = (rotMat * point);
		}
	}

	void DebugAPI::DrawLine(RE::GPtr<RE::GFxMovieView> a_movie, glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_lineThickness)
	{
		uint32_t rgb = a_rgba >> 8;
		uint32_t alpha = a_rgba & 0x000000FF;
		// https://homepage.divms.uiowa.edu/~slonnegr/flash/ActionScript2Reference.pdf
		// Pages 885-887, or search "lineStyle".
		RE::GFxValue argsLineStyle[6]{ a_lineThickness, rgb, alpha, true, "normal", "none" };
		a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 6);

		RE::GFxValue argsStartPos[2]{ a_from.x, a_from.y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);

		RE::GFxValue argsEndPos[2]{ a_to.x, a_to.y };
		a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);

		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	// Credits to ersh1:
	// https://github.com/ersh1/TrueHUD/blob/master/src/Scaleform/TrueHUDMenu.cpp#L1744
	void DebugAPI::DrawPoint(RE::GPtr<RE::GFxMovieView> a_movie, glm::vec2 a_center, uint32_t a_rgba, float a_size)
	{
		uint32_t rgb = a_rgba >> 8;
		uint32_t alpha = a_rgba & 0x000000FF;
		// The angle of each of the eight segments is 45 degrees (360 divided by 8), which
		// equals π/4 radians.
		constexpr float angleDelta = PI / 4;
		// Find the distance from the circle's center to the control points for the curves.
		float ctrlDist = a_size / cosf(angleDelta / 2.0f);
		// Initialize the angle
		float angle = 0.0f;
		RE::GFxValue argsLineStyle[3]{ 0, 0, 0 };
		a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 3);

		RE::GFxValue argsFill[2]{ rgb, alpha };
		a_movie->Invoke("beginFill", nullptr, argsFill, 2);

		// Move to the starting point, one radius to the right of the circle's center.
		RE::GFxValue argsStartPos[2]{ a_center.x + a_size, a_center.y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);

		// Repeat eight times to create eight segments.
		for (int i = 0; i < 8; ++i)
		{
			// Increment the angle by angleDelta (π/4) to create the whole circle (2π).
			angle += angleDelta;

			// The control points are derived using sine and cosine.
			float rx = a_center.x + cosf(angle - (angleDelta / 2)) * (ctrlDist);
			float ry = a_center.y + sinf(angle - (angleDelta / 2)) * (ctrlDist);

			// The anchor points (end points of the curve) can be found similarly to the
			// control points.
			float ax = a_center.x + cosf(angle) * a_size;
			float ay = a_center.y + sinf(angle) * a_size;

			// Draw the segment.
			RE::GFxValue argsCurveTo[4]{ rx, ry, ax, ay };
			a_movie->Invoke("curveTo", nullptr, argsCurveTo, 4);
		}

		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	void DebugAPI::DrawShape(RE::GPtr<RE::GFxMovieView> a_movie, const glm::vec2& a_origin, const std::vector<glm::vec2>& a_offsets, const uint32_t& a_rgba, const bool& a_fill, const float& a_lineThickness, const float& a_durationSecs)
	{
		uint32_t rgb = a_rgba >> 8;
		uint32_t alpha = a_rgba & 0x000000FF;
		if (a_fill)
		{
			RE::GFxValue argsLineStyle[3]{ 0, 0, 0 };
			a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 3);
		}
		else
		{
			// https://homepage.divms.uiowa.edu/~slonnegr/flash/ActionScript2Reference.pdf
			// Pages 885-887, or search "lineStyle".
			RE::GFxValue argsLineStyle[8]{ a_lineThickness, rgb, alpha, true, "normal", "none", "miter", 1.414f };
			a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 8);
		}

		RE::GFxValue argsFill[2]{ rgb, alpha };
		a_movie->Invoke("beginFill", nullptr, argsFill, 2);
		// Start from the first offset point.
		RE::GFxValue argsStartPos[2]{ a_origin.x + a_offsets[0].x, a_origin.y + a_offsets[0].y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);
		for (int32_t i = 1; i < a_offsets.size(); ++i)
		{
			RE::GFxValue argsEndPos[2]{ a_origin.x + a_offsets[i].x, a_origin.y + a_offsets[i].y };
			a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		}

		// Connect back to the first offset point.
		RE::GFxValue argsEndPos[2]{ a_origin.x + a_offsets[0].x, a_origin.y + a_offsets[0].y };
		a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	void DebugAPI::ClearOverlay(RE::GPtr<RE::GFxMovieView> a_movie)
	{
		a_movie->Invoke("clear", nullptr, nullptr, 0);
	}

	glm::vec2 DebugAPI::WorldToScreenPoint(glm::vec3 a_worldPos)
	{
		auto hud = GetHUD();
		if (!hud || !hud->uiMovie)
			return { 0.0f, 0.0f };

		glm::vec2 screenPoint{ 0.0f, 0.0f };
		if (RE::NiPointer<RE::NiCamera> niCam = Util::GetNiCamera(); niCam)
		{
			RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
			const float rectWidth = abs(gRect.right - gRect.left);
			const float rectHeight = abs(gRect.bottom - gRect.top);
			RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };

			float x = 0.0f, y = 0.0f, z = 0.0f;
			RE::NiCamera::WorldPtToScreenPt3(niCam->worldToCam, port, ToNiPoint3(a_worldPos), x, y, z, 1e-5f);
			// Clamp to frame dimensions.
			screenPoint.x = std::clamp(x, gRect.left, gRect.right);
			screenPoint.y = std::clamp(y, gRect.top, gRect.bottom);
		}

		return screenPoint;
	}

	RE::GPtr<RE::IMenu> DebugAPI::GetHUD()
	{
		if (auto ui = RE::UI::GetSingleton(); ui)
		{
			return ui->GetMenu(DebugOverlayMenu::MENU_NAME);
		}

		return nullptr;
	}

	DebugOverlayMenu::DebugOverlayMenu()
	{
		auto scaleformManager = RE::BSScaleformManager::GetSingleton();
		if (!scaleformManager)
		{
			logger::error("[ALYSLC] ERR: Failed to initialize DebugOverlayMenu. ScaleformManager not found.");
			return;
		}

		scaleformManager->LoadMovieEx(this, MENU_PATH, RE::GFxMovieView::ScaleModeType::kExactFit, 0.0f, [](RE::GFxMovieDef* a_def) -> void {
			a_def->SetState(RE::GFxState::StateType::kLog,
				RE::make_gptr<Logger>().get());
		});

		// Rendered above other menus.
		depthPriority = 19;
		// Can save while open. Not a menu that is opened temporarily.
		menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving, RE::UI_MENU_FLAGS::kAlwaysOpen);
		// No input.
		inputContext = RE::IMenu::Context::kNone;
		// Scale to fit the screen from the top left corner.
		uiMovie->SetViewScaleMode(RE::GFxMovieView::ScaleModeType::kExactFit);
		uiMovie->SetViewAlignment(RE::GFxMovieView::AlignType::kTopLeft);
		// Disable input.
		uiMovie->SetMouseCursorCount(0);
		uiMovie->SetControllerCount(0);
		uiMovie->SetPause(true);
	}

	void DebugOverlayMenu::Register()
	{
		logger::info("[ALYSLC] Registering DebugOverlayMenu.");
		if (auto ui = RE::UI::GetSingleton(); ui)
		{
			ui->Register(MENU_NAME, Creator);
			DebugOverlayMenu::Load();
			logger::info("[ALYSLC] Successfully registered DebugOverlayMenu.");
		}
		else
		{
			logger::error("[ALYSLC] Failed to register DebugOverlayMenu.");
		}
	}

	void DebugOverlayMenu::Load()
	{
		if (auto msgQ = RE::UIMessageQueue::GetSingleton(); msgQ)
		{
			msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
			logger::info("[ALYSLC] Successfully messaged DebugOverlayMenu to show.");
		}
		else
		{
			logger::warn("[ALYSLC] Failed to show DebugOverlayMenu.");
		}
	}

	void DebugOverlayMenu::Unload()
	{
		if (auto msgQ = RE::UIMessageQueue::GetSingleton(); msgQ)
		{
			msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			logger::info("[ALYSLC] Successfully messaged DebugOverlayMenu to unload.");
		}
		else
		{
			logger::warn("[ALYSLC] Failed to hide DebugOverlayMenu.");
		}
	}

	void DebugOverlayMenu::Show(std::string a_source)
	{
		auto sourceIdx = std::find(hiddenSources.begin(), hiddenSources.end(), a_source);
		if (sourceIdx != hiddenSources.end()) 
		{
			hiddenSources.erase(sourceIdx);
		}

		if (hiddenSources.empty())
		{
			ToggleVisibility(true);
		}
	}

	void DebugOverlayMenu::Hide(std::string a_source)
	{
		auto sourceIdx = std::find(hiddenSources.begin(), hiddenSources.end(), a_source);
		if (sourceIdx == hiddenSources.end())
		{
			hiddenSources.push_back(a_source);
		}

		if (!hiddenSources.empty())
		{
			ToggleVisibility(false);
		}
	}

	void DebugOverlayMenu::ToggleVisibility(bool a_mode)
	{
		logger::info("[ALYSLC] Toggling visibility of DebugOverlayMenu to {}.", a_mode);
		auto ui = RE::UI::GetSingleton();
		if (!ui)
		{
			return;
		}

		auto menu = ui->GetMenu(DebugOverlayMenu::MENU_NAME);
		if (!menu || !menu->uiMovie)
		{
			return;
		}

		menu->uiMovie->SetVisible(a_mode);
	}

	void DebugAPI::CacheMenuData()
	{
		auto ui = RE::UI::GetSingleton();
		if (!ui)
		{
			return;
		}

		// Already cached.
		if (cachedMenuData)
		{
			return;
		}

		RE::GPtr<RE::IMenu> menu = ui->GetMenu(DebugOverlayMenu::MENU_NAME);
		if (!menu || !menu->uiMovie)
		{
			return;
		}

		RE::GRectF rect = menu->uiMovie->GetVisibleFrameRect();
		screenResX = abs(rect.right - rect.left);
		screenResY = abs(rect.bottom - rect.top);
		cachedMenuData = true;
	}

	void DebugOverlayMenu::AdvanceMovie(float a_interval, std::uint32_t a_currentTime)
	{
		if (auto ui = RE::UI::GetSingleton(); ui)
		{
			auto menu = ui->GetMenu(DebugOverlayMenu::MENU_NAME);
			if (!menu || !menu->uiMovie)
			{
				return;
			}
		}

		RE::IMenu::AdvanceMovie(a_interval, a_currentTime);
		DebugAPI::Update();
	}
}
