#pragma once

#include <Uefi.h>
#include <concepts>
#include <cstdint>
#include <optional>

namespace UefiHw::Pci
{
	template <typename T>
	concept RegisterValue = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t> || std::same_as<T, std::uint64_t>;

	template <typename T>
	concept ConfigurationValue = RegisterValue<T> && (sizeof(T) <= 4);

	enum class ConfigurationSpace : UINT32
	{
		Conventional = 256,
		Extended = 4096
	};

	struct Location final
	{
		UINTN Segment{};
		UINT8 Bus{};
		UINT8 Device{};
		UINT8 Function{};
	};

	struct Subsystem final
	{
		UINT16 Vendor{};
		UINT16 Device{};
	};

	struct Identity final
	{
		UINT16 Vendor{};
		UINT16 Device{};
		UINT8 Revision{};
		UINT8 ProgrammingInterface{};
		UINT8 Subclass{};
		UINT8 BaseClass{};
		UINT8 HeaderType{};
		std::optional<Subsystem> SubsystemIds;
	};

	struct Capability final
	{
		UINT16 Id{};
		UINT16 Offset{};
		UINT8 Version{};
	};

	enum class ResourceKind
	{
		Memory32,
		Memory64,
		Io
	};

	struct BarInfo final
	{
		UINT8 Index{};
		ResourceKind Kind{};
		UINT64 HostAddress{};
		UINT64 Length{};
		UINT64 TranslationOffset{};
		UINT64 SupportedAttributes{};
		bool Prefetchable{};
	};
}
