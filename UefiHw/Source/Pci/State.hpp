#pragma once

#include <UefiHw/Pci/Device.hpp>
#include <UefiHw/Protocol/Open.hpp>
#include <UefiHw/Checked.hpp>
#include <optional>
#include <variant>

namespace UefiHw::Pci::Detail
{
	struct DeviceState final
	{
		using OwnedProtocol = std::variant<Protocol::OpenedProtocol<EFI_PCI_IO_PROTOCOL>, Protocol::OpenedProtocol<EFI_PCI_IO_PROTOCOL, EFI_OPEN_PROTOCOL_EXCLUSIVE>>;
		std::optional<OwnedProtocol> Relationship;

		ConfigurationSpace Space
		{
			ConfigurationSpace::Conventional
		};

		std::size_t Buffers{};
		bool AttributeOwner{};

		[[nodiscard]]
		auto IsOpen() const noexcept -> bool
		{
			return Relationship && std::visit([](const auto& Open) { return Open.IsOpen(); }, *Relationship);
		}

		[[nodiscard]]
		auto Interface() const -> EFI_PCI_IO_PROTOCOL&
		{
			if (!IsOpen())
			{
				throw std::logic_error(
					"PCI device is closed"
				);
			}

			return *std::visit([](const auto& Open) { return Open.Get(); }, *Relationship);
		}
	};

	[[nodiscard]]
	auto Width(
		std::size_t Size
	) -> EFI_PCI_IO_PROTOCOL_WIDTH;

	auto ValidateSpace(
		ConfigurationSpace Space
	) -> void;

	auto Configuration(
		EFI_PCI_IO_PROTOCOL& Interface,
		ConfigurationSpace Space,
		bool Write,
		UINT32 Offset,
		std::size_t Width,
		void* Value
	) -> void;

	[[nodiscard]]
	auto GetLocation(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> Location;

	[[nodiscard]]
	auto GetIdentity(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> Identity;

	[[nodiscard]]
	auto Capabilities(
		EFI_PCI_IO_PROTOCOL& Interface,
		ConfigurationSpace Space,
		bool Extended
	) -> std::vector<Capability>;

	[[nodiscard]]
	auto GetBar(
		EFI_PCI_IO_PROTOCOL& Interface,
		UINT8 Index
	) -> std::optional<BarInfo>;

	[[nodiscard]]
	auto Bars(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> std::vector<BarInfo>;

	[[nodiscard]]
	auto Attributes(
		EFI_PCI_IO_PROTOCOL& Interface,
		EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION Operation,
		UINT64 Bits = 0
	) -> UINT64;

	auto Flush(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> void;
}
