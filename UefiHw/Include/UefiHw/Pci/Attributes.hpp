#pragma once

#include <UefiHw/Pci/Device.hpp>

namespace UefiHw::Pci
{
	class AttributeScope final
	{
	public:
		AttributeScope(
			Device& Owner,
			UINT64 Enable,
			UINT64 Disable = 0
		);

		~AttributeScope() noexcept;
		AttributeScope(const AttributeScope&) = delete;
		auto operator=(const AttributeScope&) -> AttributeScope& = delete;
		AttributeScope(AttributeScope&& Other) noexcept;
		auto operator=(AttributeScope&&) -> AttributeScope& = delete;

		auto Close() -> void;

		auto Update(
			UINT64 Enable,
			UINT64 Disable = 0
		) -> void;

	private:
		std::shared_ptr<Detail::DeviceState> Context;
		UINT64 Original{};
		UINT64 Mask{};
		bool Active{};
	};
}
