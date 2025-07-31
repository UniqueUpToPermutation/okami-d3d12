#pragma once

#include <glm/mat4x4.hpp>

namespace okami {
	struct DummyTriangleComponent {};

	struct CameraInfo {
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
	};
}