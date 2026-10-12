#include <UefiHw/Io/Bar.hpp>
#include "../Pci/State.hpp"
#include <utility>

namespace UefiHw::Io
{
	Bar::Bar(
		std::weak_ptr<Pci::Detail::DeviceState> Owner,
		UINT8 Index
	) noexcept : Context(std::move(Owner)), Index(Index)
	{
	}

	auto Bar::Info() const -> Pci::BarInfo
	{
		const auto Pinned = Context.lock();

		if (!Pinned)
		{
			throw std::logic_error(
				"Expired BAR owner"
			);
		}
		const auto Result = Pci::Detail::GetBar(
			Pinned->Interface(),
			Index
		);

		if (!Result)
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				EFI_UNSUPPORTED,
				"BAR no longer configured"
			};
		}

		return *Result;
	}

	auto Bar::Access(
		bool Write,
		UINT64 Offset,
		std::size_t Width,
		std::size_t Count,
		void* Buffer
	) const -> void
	{
		const auto Pinned = Context.lock();

		if (!Pinned)
		{
			throw std::logic_error(
				"Expired BAR owner"
			);
		}
		auto& Interface = Pinned->Interface();

		const auto Resource = Info();

		ValidateRange(
			Offset,
			Width,
			Count,
			Resource.Length
		);

		if (Resource.Kind == Pci::ResourceKind::Io && Width == 8)
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				EFI_UNSUPPORTED,
				"64-bit PCI port I/O"
			};
		}

		if (!Count)
		{
			return;
		}

		if (!Buffer || reinterpret_cast<std::uintptr_t>(Buffer) % Width)
		{
			throw std::invalid_argument(
				"Invalid BAR transfer buffer"
			);
		}

		const auto& Access = Resource.Kind == Pci::ResourceKind::Io ? Interface.Io : Interface.Mem;
		const auto Operation = Write ? Access.Write : Access.Read;

		if (!Operation)
		{
			ContractViolation(
				"Missing PCI BAR operation"
			);
		}

		const auto AccessWidth = Pci::Detail::Width(
			Width
		);

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Operation(
						&Interface,
						AccessWidth,
						Index,
						Offset,
						Count,
						Buffer
					);
				}
			),
			Write ? "PCI BAR write" : "PCI BAR read"
		);
	}
}
