#pragma once

#include <stdexcept>
#include <atomic>
#include <memory>

#include "engine.hpp"
#include "geometry.hpp"
#include "texture.hpp"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace okami {
	using Color = glm::vec4;

	namespace color {
		constexpr Color White = Color(1.0f, 1.0f, 1.0f, 1.0f);
		constexpr Color Black = Color(0.0f, 0.0f, 0.0f, 1.0f);
		constexpr Color Red = Color(1.0f, 0.0f, 0.0f, 1.0f);
		constexpr Color Green = Color(0.0f, 1.0f, 0.0f, 1.0f);
		constexpr Color Blue = Color(0.0f, 0.0f, 1.0f, 1.0f);
		constexpr Color Yellow = Color(1.0f, 1.0f, 0.0f, 1.0f);
		constexpr Color Cyan = Color(0.0f, 1.0f, 1.0f, 1.0f);
		constexpr Color Magenta = Color(1.0f, 0.0f, 1.0f, 1.0f);
		constexpr Color Orange = Color(1.0f, 0.5f, 0.0f, 1.0f);
		constexpr Color Purple = Color(0.5f, 0.0f, 0.5f, 1.0f);
		constexpr Color Pink = Color(1.0f, 0.0f, 0.5f, 1.0f);
		constexpr Color Brown = Color(0.6f, 0.3f, 0.1f, 1.0f);
		constexpr Color Gray = Color(0.5f, 0.5f, 0.5f, 1.0f);
		constexpr Color LightGray = Color(0.8f, 0.8f, 0.8f, 1.0f);
		constexpr Color DarkGray = Color(0.3f, 0.3f, 0.3f, 1.0f);
		constexpr Color Transparent = Color(0.0f, 0.0f, 0.0f, 0.0f);
		constexpr Color CornflowerBlue = Color(0.39f, 0.58f, 0.93f, 1.0f);
	};

	struct DummyTriangleComponent {};

	struct Mesh {
		VertexFormat m_format;

		using CreationData = InitMesh;
	};

	struct StaticMeshComponent {
		ResHandle<Mesh> m_mesh;
	};

	struct Rect {
		glm::vec2 m_position = glm::vec2(0.0f, 0.0f);
		glm::vec2 m_size = glm::vec2(0.0f, 0.0f);

		inline glm::vec2 GetMin() const {
			return m_position;
		}

		inline glm::vec2 GetMax() const {
			return m_position + m_size;
		}

		inline glm::vec2 GetSize() const {
			return m_size;
		}
	};

	struct SpriteComponent {
		ResHandle<Texture> m_texture;
		std::optional<glm::vec2> m_origin;
		std::optional<Rect> m_sourceRect;
		Color m_color = color::White;
		int m_layer = 0;
	};
}
