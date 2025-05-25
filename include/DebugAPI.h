#pragma once

namespace ALYSLC
{
	// Full credits to Shrimperator and ersh1.
	// Code was pieced together from the mods BTPS:
	// https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection
	// and TrueHUD:
	// https://github.com/ersh1/TrueHUD

	class DebugAPIDrawRequest
	{
	public:
		DebugAPIDrawRequest();

		virtual ~DebugAPIDrawRequest() = default;
		virtual void Draw(RE::GPtr<RE::GFxMovieView> a_movie) = 0;

		// Duration the line should be drawn for.
		float durationSecs;
		// Time point indicating when this draw request was made.
		std::chrono::steady_clock::time_point requestTimestamp;
	};

	class DebugAPICurve : public DebugAPIDrawRequest
	{
	public:
		DebugAPICurve();
		DebugAPICurve
		(
			std::vector<glm::vec3> a_xyCoordsAndThickness,
			uint32_t a_rgbaStart,
			uint32_t a_rgbaEnd,
			float a_durationSecs
		);
		
		// Implements ALYSLC::DebugAPIDrawRequest:
		void Draw(RE::GPtr<RE::GFxMovieView> a_movie) override;

		// Screen coords for all points to connect along the curve
		// and the thickness of the line to connect between the current and next points.
		// (X coord, Y coord, Thickness connecting to next point).
		std::vector<glm::vec3> xyCoordsAndThickness;
		// Red, Green, Blue, Alpha to create a gradient between the start 
		// and end points of the curve.
		uint32_t rgbaEnd;
		uint32_t rgbaStart;
	};

	class DebugAPILine : public DebugAPIDrawRequest
	{
	public:
		DebugAPILine();
		DebugAPILine
		(
			glm::vec2 a_from,
			glm::vec2 a_to,
			uint32_t a_rgba,
			float a_lineThickness, 
			float a_durationSecs
		);
		
		// Implements ALYSLC::DebugAPIDrawRequest:
		void Draw(RE::GPtr<RE::GFxMovieView> a_movie) override;

		// Screen coords to start drawing from.
		glm::vec2 from;
		// Screen coords to draw to.
		glm::vec2 to;
		// Red, Green, Blue, Alpha.
		uint32_t rgba;
		// Thickness of the line in pixels.
		float lineThickness;
	};

	class DebugAPIPoint : public  DebugAPIDrawRequest
	{
	public:
		DebugAPIPoint();
		DebugAPIPoint(glm::vec2 a_center, uint32_t a_rgba, float a_size, float a_durationSecs);
		
		// Implements ALYSLC::DebugAPIDrawRequest:
		void Draw(RE::GPtr<RE::GFxMovieView> a_movie) override;

		// Screen coords for the center of the point.
		glm::vec2 center;
		// Red, Green, Blue, Alpha.
		uint32_t rgba;
		// Size of the point in pixels.
		float size;
	};

	class DebugAPIShape : public  DebugAPIDrawRequest
	{
	public:
		DebugAPIShape();
		DebugAPIShape
		(
			glm::vec2 a_origin, 
			std::vector<glm::vec2> a_offsets, 
			uint32_t a_rgba, 
			bool a_fill, 
			float a_lineThickness, 
			float a_durationSecs
		);
		
		// Implements ALYSLC::DebugAPIDrawRequest:
		void Draw(RE::GPtr<RE::GFxMovieView> a_movie) override;

		// Screen coords for the origin point of the shape.
		glm::vec2 origin;
		// Boundary points of the shape expressed as points offset from the origin.
		std::vector<glm::vec2> offsets;
		// Red, Green, Blue, Alpha.
		uint32_t rgba;
		// Should the shape be filled after tracing out its boundary?
		bool fill;
		// Thickness of the shape's boundary in pixels.
		float lineThickness;
	};

	class DebugAPI
	{
	public:
		// Get and save menu dimensions.
		static void CacheMenuData();
		
		// Clamp the given line endpoints to the visible frame.
		static void ClampLineToScreen(glm::vec2& a_startPos, glm::vec2& a_endPos);

		// Clamp the given screen point to the visible frame.
		static void ClampPointToScreen(glm::vec2& a_point);

		// Get the UI menu.
		static RE::GPtr<RE::IMenu> GetHUD();
		
		// Return true if the screen point is within the dimensions of the menu's frame.
		static bool PointIsOnScreen(const glm::vec2& a_screenPoint);

		// Queue arrows, circles, lines, points, and shapes given 2D (screenspace) 
		// and 3D (worldspace) coordinates.
		// Drawn during the next update.
		static void QueueArrow2D
		(
			glm::vec2 a_from, 
			glm::vec2 a_to,
			uint32_t a_rgba,
			float a_headLength, 
			float a_lineThickness, 
			float a_durationSecs = 0
		);
		
		static void QueueArrow3D
		(
			glm::vec3 a_from, 
			glm::vec3 a_to, 
			uint32_t a_rgba, 
			float a_headLength, 
			float a_lineThickness, 
			float a_durationSecs = 0
		);
		
		static void QueueCircle2D
		(
			glm::vec2 a_center, 
			uint32_t a_rgba,
			uint32_t a_segments,
			float a_radius, 
			float a_lineThickness, 
			float a_durationSecs = 0
		);
		
		static void QueueCircle3D
		(
			glm::vec3 a_center, 
			glm::vec3 a_worldNormal,
			uint32_t a_rgba,
			uint32_t a_segments,
			float a_radius, 
			float a_lineThickness, 
			bool a_connectCenterToVertices = false,
			bool a_screenspaceRadius = false,
			float a_durationSecs = 0
		);
		
		static void QueueCurve2D
		(
			std::vector<glm::vec3> a_xyCoordsAndThickness,
			uint32_t a_rgbaStart,
			uint32_t a_rgbaEnd,
			float a_durationSecs = 0
		);
		
		static void QueueCurve3D
		(
			std::vector<glm::vec4> a_xyzCoordsAndThickness,
			uint32_t a_rgbaStart,
			uint32_t a_rgbaEnd,
			float a_durationSecs = 0
		);

		static void QueueLine2D
		(
			glm::vec2 a_from, 
			glm::vec2 a_to,
			uint32_t a_rgba, 
			float a_lineThickness, 
			float a_durationSecs = 0
		);
		
		static void QueueLine3D
		(
			glm::vec3 a_from,
			glm::vec3 a_to, 
			uint32_t a_rgba,
			float a_lineThickness, 
			float a_durationSecs = 0
		);
		
		static void QueuePoint2D
		(
			glm::vec2 a_center,
			uint32_t a_rgba,
			float a_size, 
			float a_durationSecs = 0
		);
		
		static void QueuePoint3D
		(
			glm::vec3 a_center, 
			uint32_t a_rgba,
			float a_size, 
			float a_durationSecs = 0
		);
		
		static void QueueShape2D
		(
			const glm::vec2& a_origin,
			const std::vector<glm::vec2>& a_offsets, 
			const uint32_t& a_rgba, 
			bool&& a_fill = true,
			const float& a_lineThickness = 1.0f,
			const float& a_durationSecs = 0
		);
		
		// Rotate lines and points.
		static void RotateLine2D
		(
			std::pair<glm::vec2, glm::vec2>& a_line,
			const glm::vec2& a_pivotPoint, 
			const float& a_angle
		);
		
		static void RotateLine3D
		(
			std::pair<glm::vec4, glm::vec4>& a_line, 
			const glm::vec4& a_pivotPoint, 
			const float& a_pitch, 
			const float& a_yaw
		);
		
		static void RotateOffsetPoints2D(std::vector<glm::vec2>& a_points, const float& a_angle);
		
		// Run at an interval from AdvanceMovie().
		// Clears and re-draws queued lines, points, and shapes.
		static void Update();
		
		// Get the corresponding screenspace point from the given worldspace position.
		// Clamps points beyond the visible frame to the boundary of the frame
		// to keep them on-screen.
		static glm::vec2 WorldToScreenPoint(glm::vec3 a_worldPos);

		// Queued lines, points, and shapes to draw during the next update.
		static std::vector<std::unique_ptr<DebugAPIDrawRequest>> drawRequests;
		// Has the menu's data been cached?
		static bool cachedMenuData;
		// Menu overlay dimensions.
		static float screenResX;
		static float screenResY;
		// Max number of draw requests per frame.
		static constexpr uint32_t MAX_DRAW_REQUESTS = 5000;

	private:
		// NOTE:
		// Unused at the moment.
		
		// Draw a queued line, point, or shape on the Scaleform overlay.
		static void DrawLine
		(
			RE::GPtr<RE::GFxMovieView> a_movie,
			glm::vec2 a_from,
			glm::vec2 a_to,
			uint32_t a_rgba,
			float a_lineThickness
		);
		
		static void DrawPoint
		(
			RE::GPtr<RE::GFxMovieView> a_movie, 
			glm::vec2 a_center, 
			uint32_t a_rgba,
			float a_size
		);
		
		static void DrawShape
		(
			RE::GPtr<RE::GFxMovieView> a_movie, 
			const glm::vec2& a_origin, 
			const std::vector<glm::vec2>& a_offsets,
			const uint32_t& a_rgba, 
			const bool& a_fill = true, 
			const float& a_lineThickness = 1.0f
		);
		
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
		
		// Toggle visibility to false.
		static void Hide(std::string a_source);

		// Message the menu to show.
		static void Load();
		
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
				std::vector<char> buf
				(
					static_cast<std::size_t>
					(
						std::vsnprintf(0, 0, fmt.c_str(), a_argList) + 1
					)
				);
				std::vsnprintf(buf.data(), buf.size(), fmt.c_str(), args);
				va_end(args);

				SKSE::log::info(std::string_view("{}"), buf.data());
			}
		};
	};
}
