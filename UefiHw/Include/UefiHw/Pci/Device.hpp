#pragma once

#include <UefiHw/Pci/Types.hpp>
#include <cstddef>
#include <memory>
#include <vector>

namespace UefiHw::Io
{
	class Bar;
}
namespace UefiHw::Dma
{
	class Buffer;
}

namespace UefiHw::Pci
{
	namespace Detail
	{
		struct DeviceState;
	}

	class AttributeScope;

	class Device final
	{
	public:
		Device(
			EFI_HANDLE Controller,
			EFI_HANDLE Agent,
			ConfigurationSpace Space = ConfigurationSpace::Conventional
		);

		[[nodiscard]]
		static auto TakeOver(
			EFI_HANDLE Controller,
			EFI_HANDLE Agent,
			ConfigurationSpace Space = ConfigurationSpace::Conventional
		) -> Device;

		~Device() noexcept;
		Device(const Device&) = delete;
		auto operator=(const Device&) -> Device& = delete;
		Device(Device&& Other) noexcept;
		auto operator=(Device&&) -> Device& = delete;

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

		[[nodiscard]]
		auto Registers(
			UINT8 Index
		) const -> Io::Bar;

		[[nodiscard]]
		auto Attributes() const -> UINT64;

		[[nodiscard]]
		auto SupportedAttributes() const -> UINT64;

		[[nodiscard]]
		auto IsOpen() const noexcept -> bool;

		auto Flush() const -> void;

		auto Close() -> void;

		template <ConfigurationValue T>
		[[nodiscard]]
		auto Read(
			UINT32 Offset
		) const -> T
		{
			T Value{};

			AccessConfiguration(
				false,
				Offset,
				sizeof(T),
				&Value
			);

			return Value;
		}

		template <ConfigurationValue T>
		auto Write(
			UINT32 Offset,
			T Value
		) -> void
		{
			AccessConfiguration(
				true,
				Offset,
				sizeof(T),
				&Value
			);
		}

	private:
		explicit Device(ConfigurationSpace Space);

		friend class AttributeScope;
		friend class UefiHw::Dma::Buffer;

		auto AccessConfiguration(
			bool Write,
			UINT32 Offset,
			std::size_t Width,
			void* Value
		) const -> void;

		std::shared_ptr<Detail::DeviceState> Context;
	};
}
