#pragma once

#include <UefiHw/Pci/Types.hpp>
#include <vector>

namespace UefiHw::Pci
{
	class View final
	{
	public:
		explicit View(
			EFI_HANDLE Handle,
			ConfigurationSpace Space = ConfigurationSpace::Conventional
		);

		[[nodiscard]]
		static auto Enumerate() -> std::vector<View>;

		[[nodiscard]]
		auto GetLocation() const -> Location;

		[[nodiscard]]
		auto GetIdentity() const -> Identity;

		[[nodiscard]]
		auto Capabilities() const -> std::vector<Capability>;

		[[nodiscard]]
		auto ExtendedCapabilities() const -> std::vector<Capability>;

		[[nodiscard]]
		auto Bars() const -> std::vector<BarInfo>;

		template <ConfigurationValue T>
		[[nodiscard]]
		auto Read(
			UINT32 Offset
		) const -> T
		{
			T Value{};

			ReadConfiguration(
				Offset,
				sizeof(T),
				&Value
			);

			return Value;
		}

	private:
		auto ReadConfiguration(
			UINT32 Offset,
			std::size_t Width,
			void* Value
		) const -> void;

		EFI_HANDLE ProtocolHandle{};
		ConfigurationSpace Space{};
	};
}
