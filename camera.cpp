#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

using namespace okami;

constexpr glm::mat4 g_openGlToDirectX = glm::mat4(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.5f, 0.0f, 
	0.0f, 0.0f, 0.5f, 1.0f
);

glm::mat4 OrthographicProjection::GetProjectionMatrix(int width, int height, bool usingDirectX) const {
	float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	float size_x = m_width.value_or(static_cast<float>(width));
	float size_y = m_height.value_or(size_x / aspectRatio);

	auto proj = glm::ortho(
		-size_x / 2.0f, size_x / 2.0f,
		-size_y / 2.0f, size_y / 2.0f,
		m_nearZ, m_farZ);

	if (usingDirectX) {
		return g_openGlToDirectX * proj;
	}
	else {
		return proj;
	}
}

glm::mat4 PerspectiveProjection::GetProjectionMatrix(int width, int height, bool usingDirectX) const {
	float aspect = m_aspectRatio.value_or(
		static_cast<float>(width) / static_cast<float>(height));

	auto proj = glm::perspective(m_fovY, aspect, m_nearZ, m_farZ);

	if (usingDirectX) {
		return g_openGlToDirectX * proj;
	}
	else {
		return proj;
	}
}

glm::mat4 NoProjection::GetProjectionMatrix(int width, int height, bool usingDirectX) const {
	if (usingDirectX) {
		return g_openGlToDirectX;
	}
	else {
		return glm::identity<glm::mat4>();
	}
}

Camera Camera::Identity() {
	return Camera{
		Projection{ NoProjection{} } };
}

Camera Camera::Perspective(
			float fov, 
			float nearZ, 
			float farZ) {
	return Camera{
		Projection{PerspectiveProjection{ 
			.m_fovY = fov, 
			.m_aspectRatio = std::nullopt, 
			.m_nearZ = nearZ, 
			.m_farZ = farZ
		}}
	};
}

Camera Camera::Perspective(
			float fov,
			float aspect,
			float nearZ, 
			float farZ) {
	return Camera{
		Projection{PerspectiveProjection{
			.m_fovY = fov,
			.m_aspectRatio = aspect,
			.m_nearZ = nearZ,
			.m_farZ = farZ
		}}
	};
}

Camera Camera::Orthographic(
			float nearZ,
			float farZ) {
	return Camera{
		Projection{OrthographicProjection{ 
			.m_nearZ = nearZ, 
			.m_farZ = farZ
		}}
	};
}

Camera Camera::Orthographic(
			float width,
			float nearZ,
			float farZ) {
	return Camera{
		Projection{OrthographicProjection{ 
			.m_width = width, 
			.m_nearZ = nearZ, 
			.m_farZ = farZ
		}}
	};
}

Camera Camera::Orthographic(
			float width,
			float height,
			float nearZ,	
			float farZ) {
	return Camera{
		Projection{OrthographicProjection{ 
			.m_width = width, 
			.m_height = height, 
			.m_nearZ = nearZ, 
			.m_farZ = farZ
		}}
	};
}

glm::mat4 Camera::GetProjectionMatrix(int width, int height, bool usingDirectX) const {
	return std::visit(
		[&](const auto& proj) { return proj.GetProjectionMatrix(width, height, usingDirectX); },
		m_projection);
}