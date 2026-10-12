#include <UefiHw/Pci/View.hpp>
#include <UefiHw/Protocol/Handles.hpp>
#include <UefiHw/Protocol/Locate.hpp>
#include "State.hpp"

namespace UefiHw::Pci
{
	namespace
	{
		[[nodiscard]]
		auto Lookup(
			EFI_HANDLE Handle
		) -> EFI_PCI_IO_PROTOCOL&
		{
			auto Found = Protocol::FindOnHandle<EFI_PCI_IO_PROTOCOL>(
				Handle
			);

			if (!Found)
			{
				throw FirmwareError
				{
					ErrorStage::Firmware,
					EFI_NOT_FOUND,
					"Borrowed PCI device removed"
				};
			}

			return *Found->Get();
		}
	}

	View::View(
		EFI_HANDLE Handle,
		ConfigurationSpace Space
	) : ProtocolHandle(Handle), Space(Space)
	{
		if (!Handle)
		{
			throw std::invalid_argument(
				"Null PCI handle"
			);
		}

		Detail::ValidateSpace(
			Space
		);
	}

	auto View::Enumerate() -> std::vector<View>
	{
		std::vector<View> Result;

		for (auto Handle : Protocol::FindHandles<EFI_PCI_IO_PROTOCOL>())
		{
			Result.emplace_back(
				Handle
			);
		}

		return Result;
	}

	auto View::ReadConfiguration(
		UINT32 Offset,
		std::size_t Width,
		void* Value
	) const -> void
	{
		Detail::Configuration(
			Lookup(ProtocolHandle),
			Space,
			false,
			Offset,
			Width,
			Value
		);
	}

	auto View::GetLocation() const -> Location
	{
		return Detail::GetLocation(
			Lookup(ProtocolHandle)
		);
	}

	auto View::GetIdentity() const -> Identity
	{
		return Detail::GetIdentity(
			Lookup(ProtocolHandle)
		);
	}

	auto View::Capabilities() const -> std::vector<Capability>
	{
		return Detail::Capabilities(
			Lookup(ProtocolHandle),
			Space,
			false
		);
	}

	auto View::ExtendedCapabilities() const -> std::vector<Capability>
	{
		return Detail::Capabilities(
			Lookup(ProtocolHandle),
			Space,
			true
		);
	}

	auto View::Bars() const -> std::vector<BarInfo>
	{
		return Detail::Bars(
			Lookup(ProtocolHandle)
		);
	}
}
