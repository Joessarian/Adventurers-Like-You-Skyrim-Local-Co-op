#pragma once

namespace ALYSLC
{
	// Full credits to Shrimperator and ersh1.
	// Code was pieced together from the mods BTPS:
	// https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection
	// and TrueHUD:
	// https://github.com/ersh1/TrueHUD
	class DebugAPILine
	{
	public:
		DebugAPILine();
		DebugAPILine(glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_lineThickness, float a_durationSecs);

		glm::vec2 from;
		glm::vec2 to;
		uint32_t rgba;
		float lineThickness;
		float durationSecs;
		std::chrono::steady_clock::time_point requestTimestamp;
	};

	class DebugAPIPoint
	{
	public:
		DebugAPIPoint();
		DebugAPIPoint(glm::vec2 a_center, uint32_t a_rgba, float a_size, float a_durationSecs);

		glm::vec2 center;
		uint32_t rgba;
		float size;
		float durationSecs;
		std::chrono::steady_clock::time_point requestTimestamp;
	};

	class DebugAPIShape
	{
	public:
		DebugAPIShape();
		DebugAPIShape(glm::vec2 a_origin, std::vector<glm::vec2> a_offsets, uint32_t a_rgba, bool a_fill, float a_lineThickness, float a_durationSecs);

		glm::vec2 origin;
		std::vector<glm::vec2> offsets;
		uint32_t rgba;
		bool fill;
		float lineThickness;
		float durationSecs;
		std::chrono::steady_clock::time_point requestTimestamp;
	};

	class DebugAPI
	{
	public:
		// Get and save menu dimensions.
		static void CacheMenuData();
		
		// Get the UI menu.
		static RE::GPtr<RE::IMenu> GetHUD();
		
		// Queue arrows, circles, lines, points, and shapes given 2D (screenspace) and 3D (worldspace) coordinates.
		// Drawn during the next update.
		static void QueueArrow2D(glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_headLength, float a_lineThickness, float a_durationSecs = 0);
		
		static void QueueArrow3D(glm::vec3 a_from, glm::vec3 a_to, uint32_t a_rgba, float a_headLength, float a_lineThickness, float a_durationSecs = 0);
		
		static void QueueCircle2D(glm::vec2 a_center, uint32_t a_rgba, uint32_t a_segments, float a_radius, float a_lineThickness, float a_durationSecs = 0);
		
		static void QueueCircle3D(glm::vec3 a_center, uint32_t a_rgba, uint32_t a_segments, float a_radius, float a_lineThickness, float a_durationSecs = 0);
		
		static void QueueLine2D(glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_lineThickness, float a_durationSecs = 0);
		
		static void QueueLine3D(glm::vec3 a_from, glm::vec3 a_to, uint32_t a_rgba, float a_lineThickness, float a_durationSecs = 0);
		
		static void QueuePoint2D(glm::vec2 a_center, uint32_t a_rgba, float a_size, float a_durationSecs = 0);
		
		static void QueuePoint3D(glm::vec3 a_center, uint32_t a_rgba, float a_size, float a_durationSecs = 0);
		
		static void QueueShape2D(const glm::vec2& a_origin, const std::vector<glm::vec2>& a_offsets, const uint32_t& a_rgba, bool&& a_fill = true, const float& a_lineThickness = 1.0f, const float& a_durationSecs = 0);
		
		// Rotate lines and points.
		static void RotateLine2D(std::pair<glm::vec2, glm::vec2>& a_line, const glm::vec2& a_pivotPoint, const float& a_ang);
		
		static void RotateLine3D(std::pair<glm::vec4, glm::vec4>& a_line, const glm::vec4& a_pivotPoint, const float& a_pitch, const float& a_yaw);
		
		static void RotateOffsetPoints2D(std::vector<glm::vec2>& a_points, const float& a_ang);
		
		// Run at an interval from AdvanceMovie().
		// Clears and redraws queued lines, points, and shapes.
		static void Update();
		
		// Get the corresponding screenspace point from the given worldspace position.
		static glm::vec2 WorldToScreenPoint(glm::vec3 a_worldPos);
		// Queued lines, points, and shapes to draw on the next update.
		static std::vector<std::unique_ptr<DebugAPILine>> linesToDraw;
		static std::vector<std::unique_ptr<DebugAPIPoint>> pointsToDraw;
		static std::vector<std::unique_ptr<DebugAPIShape>> shapesToDraw;
		// Has the menu's data been cached?
		static bool cachedMenuData;
		// Menu overlay dimensions.
		static float screenResX;
		static float screenResY;

	private:
		// Draw a queued line, point, or shape on the Scaleform overlay.
		static void DrawLine(RE::GPtr<RE::GFxMovieView> a_movie, glm::vec2 a_from, glm::vec2 a_to, uint32_t a_rgba, float a_lineThickness);
		
		static void DrawPoint(RE::GPtr<RE::GFxMovieView> a_movie, glm::vec2 a_center, uint32_t a_rgba, float a_size);
		
		static void DrawShape(RE::GPtr<RE::GFxMovieView> a_movie, const glm::vec2& a_origin, const std::vector<glm::vec2>& a_offsets, const uint32_t& a_rgba, const bool& a_fill = true, const float& a_lineThickness = 1.0f, const float& a_durationSecs = 0);
		
		// Clear the overlay.
		static void ClearOverlay(RE::GPtr<RE::GFxMovieView> a_movie);
	};

	class DebugOverlayMenu : RE::IMenu
	{
	public:
		static constexpr const char* MENU_PATH = "ALYSLC";
		static constexpr const char* MENU_NAME = "ALYSLC";

		DebugOverlayMenu();

		static RE::stl::owner<RE::IMenu*> Creator() { return new DebugOverlayMenu(); }
		
		// Message the menu to show.
		static void Load();
		
		// Toggle visibility to false.
		static void Hide(std::string a_source);
		
		// Register the menu using its name and creator.
		// Show afterward.
		static void Register();
		
		// Toggle visibility to true.
		static void Show(std::string a_source);
		
		// Once loaded, toggle visibility.
		static void ToggleVisibility(bool a_mode);
		
		// Message the menu to hide.
		static void Unload();

		// Run at an interval.
		// Perform per-update tasks here.
		void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

		// Sources requesting to hide menu.
		static std::vector<std::string> hiddenSources;

	private:
		class Logger : public RE::GFxLog
		{
		public:
			void LogMessageVarg(LogMessageType, const char* a_fmt, std::va_list a_argList) override
			{
				std::string fmt(a_fmt ? a_fmt : "");
				while (!fmt.empty() && fmt.back() == '\n')
				{
					fmt.pop_back();
				}

				std::va_list args;
				va_copy(args, a_argList);
				std::vector<char> buf(static_cast<std::size_t>(std::vsnprintf(0, 0, fmt.c_str(), a_argList) + 1));
				std::vsnprintf(buf.data(), buf.size(), fmt.c_str(), args);
				va_end(args);

				SKSE::log::info(std::string_view("{}"), buf.data());
			}
		};
	};
}
