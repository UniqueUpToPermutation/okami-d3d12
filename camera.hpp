#pragma once

#include <variant>
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace okami {
	struct PerspectiveProjection {
		float m_fovY; // Field of view in radians
		std::optional<float> m_aspectRatio; // Width / Height
		float m_nearZ; // Near plane distance
		float m_farZ; // Far plane distance

		glm::mat4 GetProjectionMatrix(int width, int height, bool usingDirectX) const;
	};
	
	struct OrthographicProjection {
		std::optional<float> m_width;
		std::optional<float> m_height;
		float m_nearZ; // Near plane distance
		float m_farZ; // Far plane distance

		glm::mat4 GetProjectionMatrix(int width, int height, bool usingDirectX) const;
	};

	struct NoProjection {
		glm::mat4 GetProjectionMatrix(int width, int height, bool usingDirectX) const;
	};

	using Projection = std::variant<
		PerspectiveProjection,
		OrthographicProjection,
		NoProjection
	>;
	
	struct Camera {
		Projection m_projection; // Projection type (perspective or orthographic)

		glm::mat4 GetProjectionMatrix(int width, int height, bool usingDirectX) const;
	
		static Camera Identity();
		static Camera Perspective(
			float fov, 
			float nearZ, 
			float farZ);
		static Camera Perspective(
			float fov,
			float aspect,
			float nearZ, 
			float farZ);
		static Camera Orthographic(
			float nearZ,
			float farZ);
		static Camera Orthographic(
			float width,
			float nearZ,
			float farZ);
		static Camera Orthographic(
			float width,
			float height,
			float nearZ,
			float farZ);
	};
}