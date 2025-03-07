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
	std::vector<std::unique_ptr<DebugAPIDrawRequest>> DebugAPI::drawRequests;
	bool DebugAPI::cachedMenuData;
	float DebugAPI::screenResX;
	float DebugAPI::screenResY;
	std::vector<std::string> DebugOverlayMenu::hiddenSources;
	
	DebugAPIDrawRequest::DebugAPIDrawRequest() :
		durationSecs(0.0f),
		requestTimestamp(SteadyClock::now())
	{ }

	DebugAPILine::DebugAPILine() :
		from(0.0f),
		to(0.0f),
		rgba(0xFFFFFFFF),
		lineThickness(1.0f)
	{ }

	DebugAPILine::DebugAPILine
	(
		glm::vec2 a_from,
		glm::vec2 a_to,
		uint32_t a_rgba,
		float a_lineThickness, 
		float a_durationSecs
	)
	{
		from = a_from;
		to = a_to;
		rgba = a_rgba;
		lineThickness = a_lineThickness;
		durationSecs = a_durationSecs;
		requestTimestamp = SteadyClock::now();
	}

	void DebugAPILine::Draw(RE::GPtr<RE::GFxMovieView> a_movie)
	{
		// Draw a line of the given color and thickness 
		// that connects the given start and end coordinates.

		uint32_t rgb = rgba >> 8;
		uint32_t alpha = std::lerp(0, 100, (rgba & 0x000000FF) / 255.0f);
		// https://homepage.divms.uiowa.edu/~slonnegr/flash/ActionScript2Reference.pdf
		// Pages 885-887, or search "lineStyle".
		RE::GFxValue argsLineStyle[6]{ lineThickness, rgb, alpha, true, "normal", "none" };
		a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 6);

		RE::GFxValue argsStartPos[2]{ from.x, from.y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);

		RE::GFxValue argsEndPos[2]{ to.x, to.y };
		a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	DebugAPIPoint::DebugAPIPoint() :
		center(0.0f),
		rgba(0xFFFFFFFF),
		size(1.0f)
	{ }

	DebugAPIPoint::DebugAPIPoint
	(
		glm::vec2 a_center,
		uint32_t a_rgba, 
		float a_size,
		float a_durationSecs
	)
	{
		center = a_center;
		rgba = a_rgba;
		size = a_size;
		durationSecs = a_durationSecs;
		requestTimestamp = SteadyClock::now();
	}

	void DebugAPIPoint::Draw(RE::GPtr<RE::GFxMovieView> a_movie)
	{
		// Draw a point of the given color and size centered at the given screen position.

		uint32_t rgb = rgba >> 8;
		uint32_t alpha = std::lerp(0, 100, (rgba & 0x000000FF) / 255.0f);
		// The angle of each of the eight segments is 45 degrees (360 divided by 8), which
		// equals π/4 radians.
		constexpr float angleDelta = PI / 4;
		// Find the distance from the circle's center to the control points for the curves.
		float ctrlDist = size / cosf(angleDelta / 2.0f);
		// Initialize the angle.
		float angle = 0.0f;
		RE::GFxValue argsLineStyle[3]{ 0, 0, 0 };
		a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 3);

		RE::GFxValue argsFill[2]{ rgb, alpha };
		a_movie->Invoke("beginFill", nullptr, argsFill, 2);

		// Move to the starting point, one radius to the right of the circle's center.
		RE::GFxValue argsStartPos[2]{ center.x + size, center.y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);

		// Repeat eight times to create eight segments.
		for (int i = 0; i < 8; ++i)
		{
			// Increment the angle by angleDelta (π/4) to create the whole circle (2π).
			angle += angleDelta;

			// The control points are derived using sine and cosine.
			float rx = center.x + cosf(angle - (angleDelta / 2)) * (ctrlDist);
			float ry = center.y + sinf(angle - (angleDelta / 2)) * (ctrlDist);

			// The anchor points (end points of the curve) can be found similarly to the
			// control points.
			float ax = center.x + cosf(angle) * size;
			float ay = center.y + sinf(angle) * size;

			// Draw the segment.
			RE::GFxValue argsCurveTo[4]{ rx, ry, ax, ay };
			a_movie->Invoke("curveTo", nullptr, argsCurveTo, 4);
		}

		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	DebugAPIShape::DebugAPIShape() :
		origin(0.0f),
		offsets({}),
		rgba(0xFFFFFFFF),
		fill(true),
		lineThickness(1.0f)
	{ }

	DebugAPIShape::DebugAPIShape
	(
		glm::vec2 a_origin, 
		std::vector<glm::vec2> a_offsets,
		uint32_t a_rgba, 
		bool a_fill, 
		float a_lineThickness,
		float a_durationSecs
	)
	{
		origin = a_origin;
		offsets = a_offsets;
		rgba = a_rgba;
		fill = a_fill;
		lineThickness = a_lineThickness;
		durationSecs = a_durationSecs;
		requestTimestamp = SteadyClock::now();
	}

	void DebugAPIShape::Draw(RE::GPtr<RE::GFxMovieView> a_movie)
	{
		// Draw a filled shape or shape outline of the given color and outline thickness, 
		// bound by the given points set, and centered at the given origin point.

		uint32_t rgb = rgba >> 8;
		uint32_t alpha = std::lerp(0, 100, (rgba & 0x000000FF) / 255.0f);
		if (fill)
		{
			RE::GFxValue argsLineStyle[3]{ 0, 0, 0 };
			a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 3);
		}
		else
		{
			// https://homepage.divms.uiowa.edu/~slonnegr/flash/ActionScript2Reference.pdf
			// Pages 885-887, or search "lineStyle".
			RE::GFxValue argsLineStyle[8]
			{
				lineThickness, rgb, alpha, true, "normal", "none", "miter", 1.414f 
			};
			a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 8);
		}

		RE::GFxValue argsFill[2]{ rgb, alpha };
		a_movie->Invoke("beginFill", nullptr, argsFill, 2);
		// Start from the first offset point.
		RE::GFxValue argsStartPos[2]{ origin.x + offsets[0].x, origin.y + offsets[0].y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);
		// Draw lines connecting all points.
		for (int32_t i = 1; i < offsets.size(); ++i)
		{
			RE::GFxValue argsEndPos[2]{ origin.x + offsets[i].x, origin.y + offsets[i].y };
			a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		}

		// Connect back to the first offset point to close the shape.
		RE::GFxValue argsEndPos[2]{ origin.x + offsets[0].x, origin.y + offsets[0].y };
		a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	void DebugAPI::Update()
	{
		// Wipe the overlay clean and then draw all queued points, lines, and shapes.

		auto hud = GetHUD();
		if (!hud || !hud->uiMovie)
		{
			DebugOverlayMenu::Load();
			SPDLOG_DEBUG("[DebugAPI] ERR: could not get HUD.");
			return;
		}

		CacheMenuData();
		ClearOverlay(hud->uiMovie);
		
		std::erase_if
		(
			drawRequests, 
			[&](const std::unique_ptr<DebugAPIDrawRequest>& a_request)
			{
				float lifetimeSecs = Util::GetElapsedSeconds(a_request->requestTimestamp);
				if (a_request->durationSecs == 0.0f || lifetimeSecs <= a_request->durationSecs)
				{
					a_request->Draw(hud->uiMovie);
				}

				return 
				(
					!a_request || 
					!a_request.get() || 
					a_request->durationSecs == 0.0f ||
					lifetimeSecs > a_request->durationSecs
				);
			}
		);
	}

	void DebugAPI::QueueArrow2D
	(
		glm::vec2 a_from,
		glm::vec2 a_to,
		uint32_t a_rgba,
		float a_headLength,
		float a_lineThickness, 
		float a_durationSecs
	)
	{
		// Queue a 2D arrow with the given attributes.

		glm::vec2 arrowRay = a_to - a_from;
		float arrowAng = atan2f(arrowRay.y, arrowRay.x);
		// Converging lines at 45 degrees that make up the tip of the arrow.
		float headRay1Ang = Util::NormalizeAng0To2Pi(arrowAng - PI / 4.0f);
		float headRay2Ang = Util::NormalizeAng0To2Pi(arrowAng + PI / 4.0f);
		glm::vec2 headRay1 = 
		(
			a_to - 
			(a_headLength * glm::vec2(cosf(headRay1Ang), sinf(headRay1Ang)))
		);
		glm::vec2 headRay2 = 
		(
			a_to - (a_headLength * glm::vec2(cosf(headRay2Ang), sinf(headRay2Ang)))
		);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(a_from, a_to, a_rgba, a_lineThickness, a_durationSecs)
		);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(headRay1, a_to, a_rgba, a_lineThickness, a_durationSecs)
		);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(headRay2, a_to, a_rgba, a_lineThickness, a_durationSecs)
		);
	}

	void DebugAPI::QueueArrow3D
	(
		glm::vec3 a_from, 
		glm::vec3 a_to,
		uint32_t a_rgba,
		float a_headLength, 
		float a_lineThickness, 
		float a_durationSecs
	)
	{
		// Queue a 3D arrow with the given attributes.

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
		glm::vec3 headRay1 = 
		(
			a_to - (a_headLength * glm::vec3(headRay1NiP3.x, headRay1NiP3.y, headRay1NiP3.z))
		);
		glm::vec3 headRay2 = 
		(
			a_to - (a_headLength * glm::vec3(headRay2NiP3.x, headRay2NiP3.y, headRay2NiP3.z))
		);

		// Body ray.
		glm::vec2 from = WorldToScreenPoint(a_from);
		glm::vec2 to = WorldToScreenPoint(a_to);
		ClampLineToScreen(from, to);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs)
		);
		// Head 1.
		from = WorldToScreenPoint(headRay1);
		to = WorldToScreenPoint(a_to);
		ClampLineToScreen(from, to);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs)
		);
		// Head 2.
		from = WorldToScreenPoint(headRay2);
		to = WorldToScreenPoint(a_to);
		ClampLineToScreen(from, to);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs)
		);
	}

	void DebugAPI::QueueCircle2D
	(
		glm::vec2 a_center, 
		uint32_t a_rgba, 
		uint32_t a_segments, 
		float a_radius, 
		float a_lineThickness, 
		float a_durationSecs
	)
	{
		// Queue a 2D circle with the given attributes.

		const float angleDelta = 2.0f * PI / a_segments;
		// Right and down.
		glm::vec2 xAxis = glm::vec2(1.0f, 0.0f);
		glm::vec2 yAxis = glm::vec2(0.0f, 1.0f);
		// Connect this vertex to the next one when drawing lines.
		glm::vec2 lastVertex = a_center + xAxis * a_radius;
		for (uint32_t sideIndex = 0; sideIndex < a_segments; sideIndex++)
		{
			glm::vec2 vertex = 
			(
				a_center + 
				(
					xAxis * cosf(angleDelta * (sideIndex + 1)) + 
					yAxis * sinf(angleDelta * (sideIndex + 1))
				) * a_radius
			);
			drawRequests.push_back
			(
				std::make_unique<DebugAPILine>
				(
					lastVertex, vertex, a_rgba, a_lineThickness, a_durationSecs
				)
			);
			lastVertex = vertex;
		}
	}

	void DebugAPI::QueueCircle3D
	(
		glm::vec3 a_center, 
		glm::vec3 a_worldNormal,
		uint32_t a_rgba,
		uint32_t a_segments, 
		float a_radius, 
		float a_lineThickness, 
		bool a_connectCenterToVertices,
		bool a_screenspaceRadius,
		float a_durationSecs
	)
	{
		// Queue a 3D circle with the given attributes.
		// Can draw lines from the center to each vertex,
		// and also draw the circle with a set radius in screenspace or worldspace units.
		// Worldspace radius will vary depending on the depth of the points 
		// in the camera's frustum.

		// Ensure the world vector is normalized first.
		a_worldNormal = glm::normalize(a_worldNormal);
		// Angle to rotate through to reach the next vertex point.
		const float angleDelta = 2.0f * PI / a_segments;
		// Screenspace center position.
		glm::vec2 center2D = WorldToScreenPoint(a_center);
		ClampPointToScreen(center2D);
		// Worldspace up direction.
		const glm::vec3 worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
		// Yaw angle to the right of the camera's facing direction.
		const float camRight = Util::NormalizeAng0To2Pi(glob.cam->GetCurrentYaw() + PI / 2.0f);
		// Worldspace vertex offset from center.
		glm::vec3 worldOffset = glm::cross(worldUp, a_worldNormal);
		// If the up and offset vectors coincide, use the cam right direction for the offset.
		if (glm::length(worldOffset) == 0.0f)
		{
			worldOffset = ToVec3
			(
				Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(camRight))
			);
		}

		// Set a fixed radius regardless of the circle's worldspace position.
		if (a_screenspaceRadius)
		{
			// World yaw for the first offset (pre-rotation).
			const float baseOffsetYaw = Util::DirectionToGameAngYaw(ToNiPoint3(worldOffset));
			// Difference between the offset yaw and the cam right yaw.
			const float yawDiff = Util::NormalizeAngToPi(camRight - baseOffsetYaw);
			// NiPoint3 version of the offset.
			RE::NiPoint3 worldOffsetNiP = ToNiPoint3(worldOffset);
			// Normal vector converted to NiPoint3.
			const RE::NiPoint3 normalNiP = ToNiPoint3(a_worldNormal);
			// Turn the world offset to align with the camera's right direction.
			Util::RotateVectorAboutAxis(worldOffsetNiP, normalNiP, yawDiff);
			// Set the world offsets to this newly rotated offset.
			worldOffset = ToVec3(worldOffsetNiP);
			// Base world offset to rotate about the normal and obtain each vertex position.
			const RE::NiPoint3 baseWorldOffsetNiP = ToNiPoint3(worldOffset);

			// Get the corresponding screenspace offset.
			glm::vec2 offset =
			(
				WorldToScreenPoint(a_center + worldOffset * a_radius) - center2D
			);
			// Save its length to scale all subsequent vertices' displacements 
			// from the center screenspace position.
			// The first offset always has the longest length,
			// as it is perpendicular to the camera's facing direction.
			const float baseOffsetLength = glm::length(offset);
			// Screenspace vertices about the center screenspace position.
			glm::vec2 firstVertex
			{
				center2D + glm::normalize(offset) * min(a_radius, baseOffsetLength) 
			};
			glm::vec2 prevVertex{ firstVertex };
			glm::vec2 vertex{ prevVertex };
			for (uint32_t vertexIndex = 0; vertexIndex < a_segments; vertexIndex++)
			{
				if (vertexIndex == a_segments - 1)
				{
					vertex = firstVertex;

				}
				else
				{
					// Rotate the world offset by the the angle delta multiplied by the index
					// of the vertex/side.
					Util::RotateVectorAboutAxis
					(
						worldOffsetNiP, normalNiP, angleDelta * (vertexIndex + 1)
					);
					// Get glm offset.
					worldOffset = ToVec3(worldOffsetNiP);
					// Get screenspace offset.
					offset = WorldToScreenPoint(a_center + worldOffset * a_radius) - center2D;
					// Scale down the radius based on its length relative to the base offset.
					// Failing to do this results in drawing a square
					// when the circle's normal vector is almost parallel 
					// to the camera's up direction, since all the vertices congregate 
					// around the base vertex  and its counterpart in the other direction.
					// Scale down if further away by choosing the smaller of the base screenspace
					// and worldspace-to-screenspace radii.
					float radius = 
					(
						min(a_radius, baseOffsetLength) * 
						min(1.0f, glm::length(offset) / baseOffsetLength)
					);
					// Tack on the new scaled offset to get the next vertex point to connect to.
					vertex = center2D + glm::normalize(offset) * radius;
				}

				if (a_connectCenterToVertices)
				{
					ClampLineToScreen(center2D, vertex);
					QueueLine2D
					(
						center2D, 
						vertex, 
						a_rgba, 
						a_lineThickness, 
						a_durationSecs
					);
				}
				else
				{
					ClampLineToScreen(center2D, prevVertex);
					// Draw the segment connecting the vertices.
					QueueLine2D
					(
						prevVertex, 
						vertex, 
						a_rgba, 
						a_lineThickness, 
						a_durationSecs
					);
				}

				// Restore the original offset to rotate during the next iteration.
				worldOffsetNiP = baseWorldOffsetNiP;
				// Save the vertex position we just connected to.
				prevVertex = vertex;
			}
		}
		else
		{
			// Set a radius that will vary in size based on where the circle is in the worldspace.
			
			// Base world offset to rotate about the normal and obtain each vertex position.
			RE::NiPoint3 baseWorldOffsetNiP = ToNiPoint3(worldOffset);
			// NiPoint3 version of the offset.
			RE::NiPoint3 worldOffsetNiP = baseWorldOffsetNiP;
			// Normal vector converted to NiPoint3.
			const RE::NiPoint3 normalNiP = ToNiPoint3(a_worldNormal);
			// Screenspace vertices about the center screenspace position.
			glm::vec2 firstVertex{ WorldToScreenPoint(a_center + worldOffset * a_radius) };
			glm::vec2 prevVertex{ firstVertex };
			glm::vec2 vertex{ prevVertex };
			for (uint32_t vertexIndex = 0; vertexIndex < a_segments; vertexIndex++)
			{
				// Rotate the world offset by the the angle delta multiplied by the index
				// of the vertex/side.
				Util::RotateVectorAboutAxis
				(
					worldOffsetNiP, normalNiP, angleDelta * (vertexIndex + 1)
				);
				// Get glm offset.
				worldOffset = ToVec3(worldOffsetNiP);

				// Tack on the new scaled offset to get the next vertex point to connect to.
				vertex = WorldToScreenPoint(a_center + worldOffset * a_radius);
				if (a_connectCenterToVertices)
				{
					ClampLineToScreen(center2D, vertex);
					QueueLine2D
					(
						center2D, 
						vertex, 
						a_rgba, 
						a_lineThickness, 
						a_durationSecs
					);
				}
				else
				{
					ClampLineToScreen(center2D, prevVertex);
					// Draw the segment connecting the vertices.
					QueueLine2D
					(
						prevVertex, 
						vertex, 
						a_rgba, 
						a_lineThickness, 
						a_durationSecs
					);
				}

				// Restore the original offset to rotate during the next iteration.
				worldOffsetNiP = baseWorldOffsetNiP;
				// Save the vertex position we just connected to.
				prevVertex = vertex;
			}
		}
	}

	void DebugAPI::QueueLine2D
	(
		glm::vec2 a_from,
		glm::vec2 a_to,
		uint32_t a_rgba, 
		float a_lineThickness,
		float a_durationSecs
	)
	{
		// Queue a 2D line with the given attributes.

		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(a_from, a_to, a_rgba, a_lineThickness, a_durationSecs)
		);
	}

	void DebugAPI::QueueLine3D
	(
		glm::vec3 a_from, 
		glm::vec3 a_to,
		uint32_t a_rgba, 
		float a_lineThickness, 
		float a_durationSecs
	)
	{
		// Queue a 3D line with the given attributes.

		glm::vec2 from = WorldToScreenPoint(a_from);
		glm::vec2 to = WorldToScreenPoint(a_to);
		ClampLineToScreen(from, to);
		drawRequests.push_back
		(
			std::make_unique<DebugAPILine>(from, to, a_rgba, a_lineThickness, a_durationSecs)
		);
	}

	void DebugAPI::QueuePoint2D
	(
		glm::vec2 a_center, uint32_t a_rgba, float a_size, float a_durationSecs
	)
	{
		// Queue a 2D point with the given attributes.

		drawRequests.push_back
		(
			std::make_unique<DebugAPIPoint>(a_center, a_rgba, a_size, a_durationSecs)
		);
	}

	void DebugAPI::QueuePoint3D
	(
		glm::vec3 a_center, uint32_t a_rgba, float a_size, float a_durationSecs
	)
	{
		// Queue a 3D point with the given attributes.

		glm::vec2 center = WorldToScreenPoint(a_center);
		drawRequests.push_back
		(
			std::make_unique<DebugAPIPoint>(center, a_rgba, a_size, a_durationSecs)
		);
	}

	void DebugAPI::QueueShape2D
	(
		const glm::vec2& a_origin,
		const std::vector<glm::vec2>& a_offsets, 
		const uint32_t& a_rgba, 
		bool&& a_fill, 
		const float& a_lineThickness, 
		const float& a_durationSecs
	)
	{
		// Queue a 2D shape with the given attributes.

		drawRequests.push_back
		(
			std::make_unique<DebugAPIShape>
			(
				a_origin, a_offsets, a_rgba, a_fill, a_lineThickness, a_durationSecs
			)
		);
	}

	void DebugAPI::RotateLine2D
	(
		std::pair<glm::vec2, 
		glm::vec2>& a_line,
		const glm::vec2& a_pivotPoint, 
		const float& a_angle
	)
	{
		// Rotate the line given by the pair of endpoints
		// about the given pivot point by the desired angle.

		// Shift the line to get its new coordinates relative to the pivot point.
		a_line.first = a_line.first - a_pivotPoint;
		a_line.second = a_line.second - a_pivotPoint;
		// https://en.wikipedia.org/wiki/Rotation_matrix
		// Counter-clockwise about origin.
		// First column, second column.
		const glm::mat2 rotMat
		{
			cosf(a_angle), -sinf(a_angle),
			sinf(a_angle), cosf(a_angle)
		};

		// Rotate both endpoints and then shift back by the original offset 
		// relative to the pivot point.
		a_line.first = (rotMat * a_line.first) + a_pivotPoint;
		a_line.second = (rotMat * a_line.second) + a_pivotPoint;
	}

	void DebugAPI::RotateLine3D
	(
		std::pair<glm::vec4, glm::vec4>& a_line, 
		const glm::vec4& a_pivotPoint, 
		const float& a_pitch, 
		const float& a_yaw
	)
	{
		// Rotate the line given by the pair of endpoints
		// about the given pivot point by the desired pitch and yaw angles.
		
		// Shift the line to get its new coordinates relative to the pivot point.
		a_line.first = a_line.first - a_pivotPoint;
		a_line.second = a_line.second - a_pivotPoint;
		auto rightAxis = glm::vec3(1.0f, 0.0f, 0.0f);
		auto upAxis = glm::vec3(0.0f, 0.0f, 1.0f);

		// Credits to mwilsnd for the rotation matrix construction:
		// https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/mmath.cpp#L222
		glm::mat4 rotMat = glm::identity<glm::mat4>();
		rotMat = glm::rotate(rotMat, -a_yaw, glm::vec3(0.0f, 0.0f, 1.0f));
		rotMat = glm::rotate(rotMat, -a_pitch, glm::vec3(1.0f, 0.0f, 0.0f));

		// Rotate both endpoints and then shift back by the original offset 
		// relative to the pivot point.
		a_line.first = (rotMat * a_line.first) + a_pivotPoint;
		a_line.second = (rotMat * a_line.second) + a_pivotPoint;
	}

	void DebugAPI::RotateOffsetPoints2D(std::vector<glm::vec2>& a_points, const float& a_angle)
	{
		// Rotate the set of points about the origin (0, 0)
		// by the desired angle.

		// https://en.wikipedia.org/wiki/Rotation_matrix
		// Counter-clockwise about origin.
		// First column, second column.
		const glm::mat2 rotMat
		{
			cosf(a_angle), -sinf(a_angle),
			sinf(a_angle), cosf(a_angle)
		};

		for (auto& point : a_points)
		{
			point = (rotMat * point);
		}
	}

	void DebugAPI::DrawLine
	(
		RE::GPtr<RE::GFxMovieView> a_movie,
		glm::vec2 a_from,
		glm::vec2 a_to,
		uint32_t a_rgba,
		float a_lineThickness
	)
	{
		// Draw a line of the given color and thickness 
		// that connects the given start and end coordinates.

		uint32_t rgb = a_rgba >> 8;
		uint32_t alpha = std::lerp(0, 100, (a_rgba & 0x000000FF) / 255.0f);
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
	void DebugAPI::DrawPoint
	(
		RE::GPtr<RE::GFxMovieView> a_movie, glm::vec2 a_center, uint32_t a_rgba, float a_size
	)
	{
		// Draw a point of the given color and size centered at the given screen position.

		uint32_t rgb = a_rgba >> 8;
		uint32_t alpha = std::lerp(0, 100, (a_rgba & 0x000000FF) / 255.0f);
		// The angle of each of the eight segments is 45 degrees (360 divided by 8), which
		// equals π/4 radians.
		constexpr float angleDelta = PI / 4;
		// Find the distance from the circle's center to the control points for the curves.
		float ctrlDist = a_size / cosf(angleDelta / 2.0f);
		// Initialize the angle.
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

	void DebugAPI::DrawShape
	(
		RE::GPtr<RE::GFxMovieView> a_movie,
		const glm::vec2& a_origin, 
		const std::vector<glm::vec2>& a_offsets,
		const uint32_t& a_rgba,
		const bool& a_fill, 
		const float& a_lineThickness
	)
	{
		// Draw a filled shape or shape outline of the given color and outline thickness, 
		// bound by the given points set, and centered at the given origin point.

		uint32_t rgb = a_rgba >> 8;
		uint32_t alpha = std::lerp(0, 100, (a_rgba & 0x000000FF) / 255.0f);
		if (a_fill)
		{
			RE::GFxValue argsLineStyle[3]{ 0, 0, 0 };
			a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 3);
		}
		else
		{
			// https://homepage.divms.uiowa.edu/~slonnegr/flash/ActionScript2Reference.pdf
			// Pages 885-887, or search "lineStyle".
			RE::GFxValue argsLineStyle[8]
			{
				a_lineThickness, rgb, alpha, true, "normal", "none", "miter", 1.414f 
			};
			a_movie->Invoke("lineStyle", nullptr, argsLineStyle, 8);
		}

		RE::GFxValue argsFill[2]{ rgb, alpha };
		a_movie->Invoke("beginFill", nullptr, argsFill, 2);
		// Start from the first offset point.
		RE::GFxValue argsStartPos[2]{ a_origin.x + a_offsets[0].x, a_origin.y + a_offsets[0].y };
		a_movie->Invoke("moveTo", nullptr, argsStartPos, 2);
		// Draw lines connecting all points.
		for (int32_t i = 1; i < a_offsets.size(); ++i)
		{
			RE::GFxValue argsEndPos[2]{ a_origin.x + a_offsets[i].x, a_origin.y + a_offsets[i].y };
			a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		}

		// Connect back to the first offset point to close the shape.
		RE::GFxValue argsEndPos[2]{ a_origin.x + a_offsets[0].x, a_origin.y + a_offsets[0].y };
		a_movie->Invoke("lineTo", nullptr, argsEndPos, 2);
		a_movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	void DebugAPI::ClearOverlay(RE::GPtr<RE::GFxMovieView> a_movie)
	{
		// Clear out the overlay. Wow.

		a_movie->Invoke("clear", nullptr, nullptr, 0);
	}

	glm::vec2 DebugAPI::WorldToScreenPoint(glm::vec3 a_worldPos)
	{
		// Convert the given world positino to a 2D screenspace position.
		// NOTE:
		// Does not clamp points to the screen's boundaries.

		auto hud = GetHUD();
		if (!hud || !hud->uiMovie) 
		{
			return { 0.0f, 0.0f };
		}

		glm::vec2 screenPoint{ 0.0f, 0.0f };
		RE::NiPointer<RE::NiCamera> niCamPtr = Util::GetNiCamera(); 
		if (!niCamPtr || !niCamPtr.get())
		{
			return screenPoint;
		}

		// Get frame dimensions.
		RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
		const float rectWidth = fabsf(gRect.right - gRect.left);
		const float rectHeight = fabsf(gRect.bottom - gRect.top);
		RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };

		float x = 0.0f, y = 0.0f, z = 0.0f;
		RE::NiCamera::WorldPtToScreenPt3
		(
			niCamPtr->worldToCam, port, ToNiPoint3(a_worldPos), x, y, z, 1e-5f
		);

		screenPoint.x = x;
		screenPoint.y = y;

		return screenPoint;
	}

	RE::GPtr<RE::IMenu> DebugAPI::GetHUD()
	{
		// Get this menu.

		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return nullptr;
		}
		
		return ui->GetMenu(DebugOverlayMenu::MENU_NAME);
	}

	bool DebugAPI::PointIsOnScreen(const glm::vec2 & a_screenPoint)
	{
		// Return true if the screen point is within the dimensions of the menu's frame.

		auto hud = GetHUD();
		if (!hud || !hud->uiMovie) 
		{
			return false;
		}

		RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
		return 
		(
			a_screenPoint.x >= gRect.left &&
			a_screenPoint.x <= gRect.right &&
			a_screenPoint.y >= gRect.top &&
			a_screenPoint.y <= gRect.bottom
		);
	}

	DebugOverlayMenu::DebugOverlayMenu()
	{
		// Construct the debug overlay menu.

		auto scaleformManager = RE::BSScaleformManager::GetSingleton();
		if (!scaleformManager)
		{
			SPDLOG_ERROR
			(
				"[DebugAPI] ERR: Failed to initialize DebugOverlayMenu. "
				"ScaleformManager not found."
			);
			return;
		}

		scaleformManager->LoadMovieEx
		(
			this, MENU_PATH, RE::GFxMovieView::ScaleModeType::kExactFit, 0.0f, 
			[](RE::GFxMovieDef* a_def) -> void 
			{
				a_def->SetState
				(
					RE::GFxState::StateType::kLog,
					RE::make_gptr<Logger>().get()
				);
			}
		);

		// Rendered above other menus.
		depthPriority = 19;
		// Can save while open. Menu is always open.
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
		// Register the debug overlay menu with the UI and then load it.

		SPDLOG_INFO("[DebugAPI] Registering DebugOverlayMenu.");
		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return;
		}

		ui->Register(MENU_NAME, Creator);
		DebugOverlayMenu::Load();
		SPDLOG_INFO("[DebugAPI] Successfully registered DebugOverlayMenu.");
	}

	void DebugOverlayMenu::Load()
	{
		// Load the debug overlay menu.

		auto msgQ = RE::UIMessageQueue::GetSingleton(); 
		if (!msgQ)
		{
			return;
		}

		msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
	}

	void DebugOverlayMenu::Unload()
	{
		// Unload the debug overlay menu.

		auto msgQ = RE::UIMessageQueue::GetSingleton(); 
		if (msgQ)
		{
			return;
		}
		
		msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
	}

	void DebugOverlayMenu::Show(std::string a_source)
	{
		// Show the debug overlay menu.

		auto sourceIdx = std::find(hiddenSources.begin(), hiddenSources.end(), a_source);
		if (sourceIdx != hiddenSources.end()) 
		{
			hiddenSources.erase(sourceIdx);
		}

		if (!hiddenSources.empty())
		{
			return;
		}

		ToggleVisibility(true);
	}

	void DebugOverlayMenu::Hide(std::string a_source)
	{
		// Hide the debug overlay menu.

		auto sourceIdx = std::find(hiddenSources.begin(), hiddenSources.end(), a_source);
		if (sourceIdx == hiddenSources.end())
		{
			hiddenSources.push_back(a_source);
		}

		if (hiddenSources.empty())
		{
			return;
		}

		ToggleVisibility(false);
	}

	void DebugOverlayMenu::ToggleVisibility(bool a_mode)
	{
		// Show (true) or hide (false) the debug overlay menu.

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
		// Cache screen frame dimension data for later.

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
		screenResX = fabsf(rect.right - rect.left);
		screenResY = fabsf(rect.bottom - rect.top);
		cachedMenuData = true;
	}

	void DebugAPI::ClampLineToScreen(glm::vec2& a_startPos, glm::vec2& a_endPos)
	{
		// Clamp the given line endpoints to the visible frame.
		// Clamp x, y coordinates separately if only one of the two 
		// are outside the screen frame boundaries.

		auto hud = GetHUD();
		if (!hud || !hud->uiMovie) 
		{
			return;
		}

		RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
		// If both are off screen, simply clamp all coordinates.
		bool bothOffScreen = 
		(
			(
				a_startPos.x < gRect.left ||
				a_startPos.x > gRect.right ||
				a_startPos.y < gRect.top ||
				a_startPos.y > gRect.bottom
			) &&
			(
				a_endPos.x < gRect.left ||
				a_endPos.x > gRect.right ||
				a_endPos.y < gRect.top ||
				a_endPos.y > gRect.bottom
			) 
		);
		if (bothOffScreen) 
		{
			a_startPos.x = std::clamp(a_startPos.x, gRect.left, gRect.right);
			a_startPos.y = std::clamp(a_startPos.y, gRect.top, gRect.bottom);
			a_endPos.x = std::clamp(a_endPos.x, gRect.left, gRect.right);
			a_endPos.y = std::clamp(a_endPos.y, gRect.top, gRect.bottom);
		}
		else
		{
			// Considering the origin at the bottom left of the screen, instead of top left,
			// we will modify both points' y coordinates before the calculations below.
			// Have to adjust the y coordinates for both points afterward, 
			// so that they are once more relative to the top left of the screen.
			a_endPos.y = gRect.bottom - a_endPos.y;
			a_startPos.y = gRect.bottom - a_startPos.y;

			// One endpoint is offscreen, so set it to the last possible on-screen point
			// along the line connecting it to the other endpoint.
			bool endOffScreen = 
			(
				a_endPos.x < gRect.left ||
				a_endPos.x > gRect.right ||
				a_endPos.y < gRect.top ||
				a_endPos.y > gRect.bottom
			);
			if (endOffScreen)
			{
				float dx = (a_endPos.x - a_startPos.x);
				float dy = (a_endPos.y - a_startPos.y);
				float slope = dy / dx;
				if (isinf(slope))
				{
					// Means that the end point is off-screen along the y axis,
					// so we just clamp the y coordinate.
					a_endPos.y = std::clamp(a_endPos.y, gRect.top, gRect.bottom);
				}
				else if (slope == 0.0f)
				{
					// Means that the end point is off-screen along the x axis,
					// so we just clamp the x coordinate.
					a_endPos.x = std::clamp(a_endPos.x, gRect.left, gRect.right);
				}
				else
				{
					glm::vec2 ogEnd = a_endPos;
					// Could be off-screen along either or both axes.
					// Get the 'overflow' amount beyond the frame boundaries 
					// in the x and y directions.

					// Always >= 0.0f
					float xOverflow = 0.0f;
					if (a_endPos.x < gRect.left)
					{
						xOverflow = gRect.left - a_endPos.x;
					}
					else if (a_endPos.x > gRect.right)
					{
						xOverflow = a_endPos.x - gRect.right;
					}
					
					// Always >= 0.0f
					float yOverflow = 0.0f;
					if (a_endPos.y < gRect.top)
					{
						yOverflow = gRect.top - a_endPos.y;
					}
					else if (a_endPos.y > gRect.bottom)
					{
						yOverflow = a_endPos.y - gRect.bottom;
					}

					// If one overflow is greater than the other,
					// the line connecting the two points
					// will hit the corresponding axis's boundary first,
					// and we need to clamp the coordinate for that axis
					// and then derive the other coordinate using the slope.
					// If both overflows are non-zero and equal, we clamp both x and y coords.
					if (xOverflow > yOverflow)
					{
						// Clamp the x coordinate and get the corresponding y coordinate
						// using the slope.
						a_endPos.x = std::clamp(a_endPos.x, gRect.left, gRect.right);
						a_endPos.y = slope * (a_endPos.x - a_startPos.x) + a_startPos.y;
					}
					else if (yOverflow > xOverflow)
					{
						// Clamp the y coordinate and get the corresponding x coordinate
						// using the slope.
						a_endPos.y = std::clamp(a_endPos.y, gRect.top, gRect.bottom);
						a_endPos.x = (a_endPos.y - a_startPos.y) / slope + a_startPos.x;
					}
					else if (xOverflow > 0.0f)
					{
						// Both overflows are equal and non-zero, so clamp both coordinates.
						a_endPos.x = std::clamp(a_endPos.x, gRect.left, gRect.right);
						a_endPos.y = std::clamp(a_endPos.y, gRect.top, gRect.bottom);
					}
				}
			}
			else
			{
				// Start is off-screen.
				
				float dx = (a_startPos.x - a_endPos.x);
				float dy = (a_startPos.y - a_endPos.y);
				float slope = dy / dx;
				if (isinf(slope))
				{
					// Means that the start point is off-screen along the y axis,
					// so we just clamp the y coordinate.
					a_startPos.y = std::clamp(a_startPos.y, gRect.top, gRect.bottom);
				}
				else if (slope == 0.0f)
				{
					// Means that the start point is off-screen along the x axis,
					// so we just clamp the x coordinate.
					a_startPos.x = std::clamp(a_startPos.x, gRect.left, gRect.right);
				}
				else
				{
					// Could be off-screen along either or both axes.
					// Get the 'overflow' amount beyond the frame boundaries 
					// in the x and y directions.

					// Always >= 0.0f
					float xOverflow = 0.0f;
					if (a_startPos.x < gRect.left)
					{
						xOverflow = gRect.left - a_startPos.x;
					}
					else if (a_startPos.x > gRect.right)
					{
						xOverflow = a_startPos.x - gRect.right;
					}
					
					// Always >= 0.0f
					float yOverflow = 0.0f;
					if (a_startPos.y < gRect.top)
					{
						yOverflow = gRect.top - a_startPos.y;
					}
					else if (a_startPos.y > gRect.bottom)
					{
						yOverflow = a_startPos.y - gRect.bottom;
					}

					// If one overflow is greater than the other,
					// the line connecting the two points
					// will hit the corresponding axis's boundary first,
					// and we need to clamp the coordinate for that axis
					// and then derive the other coordinate using the slope.
					// If both overflows are non-zero and equal, we clamp both x and y coords.
					if (xOverflow > yOverflow)
					{
						// Clamp the x coordinate and get the corresponding y coordinate
						// using the slope.
						a_startPos.x = std::clamp(a_startPos.x, gRect.left, gRect.right);
						a_startPos.y = slope * (a_startPos.x - a_endPos.x) + a_endPos.y;
					}
					else if (yOverflow > xOverflow)
					{
						// Clamp the y coordinate and get the corresponding x coordinate
						// using the slope.
						a_startPos.y = std::clamp(a_startPos.y, gRect.top, gRect.bottom);
						a_startPos.x = (a_startPos.y - a_endPos.y) / slope + a_endPos.x;
					}
					else if (xOverflow > 0.0f)
					{
						// Both overflows are equal and non-zero, so clamp both coordinates.
						a_startPos.x = std::clamp(a_startPos.x, gRect.left, gRect.right);
						a_startPos.y = std::clamp(a_startPos.y, gRect.top, gRect.bottom);
					}
				}
			}

			// Convert back to Scaleform convention,
			// with the screenspace origin at the top left corner.
			a_endPos.y = gRect.bottom - a_endPos.y;
			a_startPos.y = gRect.bottom - a_startPos.y;
		}
	}

	void DebugAPI::ClampPointToScreen(glm::vec2 & a_point)
	{
		// Clamp the given screen point to the visible frame.

		auto hud = GetHUD();
		if (!hud || !hud->uiMovie) 
		{
			return;
		}

		RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
		a_point.x = std::clamp(a_point.x, gRect.left, gRect.right);
		a_point.y = std::clamp(a_point.y, gRect.top, gRect.bottom);
	}

	void DebugOverlayMenu::AdvanceMovie(float a_interval, std::uint32_t a_currentTime)
	{
		// Update function called each frame.
		// Perform all repeating tasks here.

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

		RE::IMenu::AdvanceMovie(a_interval, a_currentTime);
		DebugAPI::Update();
	}
}
