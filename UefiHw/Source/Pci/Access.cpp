#include "State.hpp"
#include <UefiHw/Boot/Pool.hpp>
#include <IndustryStandard/Acpi10.h>
#include <array>
#include <cstring>

namespace UefiHw::Pci::Detail
{
	auto Width(
		std::size_t Size
	) -> EFI_PCI_IO_PROTOCOL_WIDTH
	{
		switch (Size)
		{
		case 1:
		{
			return EfiPciIoWidthUint8;
		}
		case 2:
		{
			return EfiPciIoWidthUint16;
		}
		case 4:
		{
			return EfiPciIoWidthUint32;
		}
		case 8:
		{
			return EfiPciIoWidthUint64;
		}
		default:
		{
			throw std::invalid_argument("Invalid PCI width");
		}
		}
	}

	auto ValidateSpace(
		ConfigurationSpace Space
	) -> void
	{
		if (Space != ConfigurationSpace::Conventional && Space != ConfigurationSpace::Extended)
		{
			throw std::invalid_argument(
				"Invalid PCI configuration space"
			);
		}
	}

	auto Configuration(
		EFI_PCI_IO_PROTOCOL& Interface,
		ConfigurationSpace Space,
		bool Write,
		UINT32 Offset,
		std::size_t Size,
		void* Value
	) -> void
	{
		ValidateRange(
			Offset,
			Size,
			1,
			static_cast<UINT32>(Space)
		);

		if (Size > 4 || !Value)
		{
			throw std::invalid_argument(
				"Invalid PCI configuration access"
			);
		}

		const auto Operation = Write ? Interface.Pci.Write : Interface.Pci.Read;

		if (!Operation)
		{
			ContractViolation(
				"Missing PCI configuration operation"
			);
		}

		const auto AccessWidth = Width(
			Size
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
						Offset,
						1,
						Value
					);
				}
			),
			Write ? "Pci.Write" : "Pci.Read"
		);
	}

	template <ConfigurationValue T>
	[[nodiscard]]
	auto Read(
		EFI_PCI_IO_PROTOCOL& Interface,
		UINT32 Offset
	) -> T
	{
		T Value{};

		Configuration(
			Interface,
			ConfigurationSpace::Extended,
			false,
			Offset,
			sizeof(T),
			&Value
		);

		return Value;
	}

	auto GetLocation(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> Location
	{
		UINTN Segment{}, Bus{}, Device{}, Function{};

		if (!Interface.GetLocation)
		{
			ContractViolation(
				"Missing PCI GetLocation"
			);
		}

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.GetLocation(
						&Interface,
						&Segment,
						&Bus,
						&Device,
						&Function
					);
				}
			),
			"Pci.GetLocation"
		);

		if (Bus > 255 || Device > 31 || Function > 7)
		{
			ContractViolation(
				"Invalid PCI location"
			);
		}

		return
		{
			Segment,
			static_cast<UINT8>(Bus),
			static_cast<UINT8>(Device),
			static_cast<UINT8>(Function)
		};
	}

	auto GetIdentity(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> Identity
	{
		Identity Result{};

		Result.Vendor = Read<std::uint16_t>(
			Interface,
			0
		);

		if (Result.Vendor == 0xffff)
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				EFI_NO_RESPONSE,
				"PCI device absent"
			};
		}

		Result.Device = Read<std::uint16_t>(
			Interface,
			2
		);

		Result.Revision = Read<std::uint8_t>(
			Interface,
			8
		);

		Result.ProgrammingInterface = Read<std::uint8_t>(
			Interface,
			9
		);

		Result.Subclass = Read<std::uint8_t>(
			Interface,
			10
		);

		Result.BaseClass = Read<std::uint8_t>(
			Interface,
			11
		);

		Result.HeaderType = Read<std::uint8_t>(
			Interface,
			14
		);

		const auto Type = Result.HeaderType & 0x7f;

		if (Type == 0 || Type == 2)
		{
			const UINT32 Offset = Type == 0 ? 0x2c : 0x40;

			Result.SubsystemIds = Subsystem
			{
				Read<std::uint16_t>(Interface, Offset),
				Read<std::uint16_t>(Interface, Offset + 2)
			};
		}
		else if (Type != 1)
		{
			ContractViolation(
				"Invalid PCI header type"
			);
		}

		return Result;
	}

	auto Capabilities(
		EFI_PCI_IO_PROTOCOL& Interface,
		ConfigurationSpace Space,
		bool Extended
	) -> std::vector<Capability>
	{
		if (Extended && Space != ConfigurationSpace::Extended)
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				EFI_UNSUPPORTED,
				"Extended PCI configuration not enabled"
			};
		}
		std::vector<Capability> Result;
		std::array<bool, 1024> Seen{};
		UINT32 Offset{};
		UINT32 Minimum = Extended ? 0x100 : 0x40;

		if (Extended)
		{
			Offset = 0x100;
		}
		else
		{
			if (!(Read<std::uint16_t>(Interface, 6) & 0x10))
			{
				return Result;
			}

			const auto Header = Read<std::uint8_t>(
				Interface, 14
			) & 0x7f;

			if (Header > 2)
			{
				ContractViolation(
					"Invalid PCI capability header"
				);
			}

			Offset = Read<std::uint8_t>(
				Interface,
				Header == 2 ? 0x14 : 0x34
			);

			Minimum = Header == 2 ? 0x48 : 0x40;
		}

		while (Offset)
		{
			const UINT32 Maximum = Extended ? 0xffc : 0xfc;

			if (Offset < Minimum || Offset > Maximum || Offset % 4 || Seen[Offset / 4])
			{
				ContractViolation(
					"Malformed or cyclic PCI capability list"
				);
			}
			Seen[Offset / 4] = true;

			const UINT32 Header = Extended ? Read<std::uint32_t>(Interface, Offset) : Read<std::uint16_t>(Interface, Offset);

			if (Extended && Offset == 0x100 && (Header == 0 || Header == UINT32_MAX))
			{
				break;
			}
			const auto Id = Extended ? Header & 0xffff : Header & 0xff;

			if (Id == (Extended ? 0xffffU : 0xffU) || (Extended && Id && !((Header >> 16) & 0xf)))
			{
				ContractViolation(
					"Invalid PCI capability identifier"
				);
			}

			Result.push_back(
				{
					static_cast<UINT16>(Id),
					static_cast<UINT16>(Offset),
					static_cast<UINT8>(Extended ? (Header >> 16) & 0xf : 0)
				}
			);

			Offset = Extended ? Header >> 20 : (Header >> 8) & 0xff;
		}

		return Result;
	}

	auto GetBar(
		EFI_PCI_IO_PROTOCOL& Interface,
		UINT8 Index
	) -> std::optional<BarInfo>
	{
		if (Index >= 6)
		{
			throw std::invalid_argument(
				"Invalid PCI BAR index"
			);
		}
		if (!Interface.GetBarAttributes)
		{
			ContractViolation(
				"Missing PCI GetBarAttributes"
			);
		}

		void* Resources{};
		UINT64 Supports{};

		const auto Status = FirmwareStatus(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.GetBarAttributes(
						&Interface,
						Index,
						&Supports,
						&Resources
					);
				}
			),
			"Pci.GetBarAttributes"
		);

		if (Status == EFI_UNSUPPORTED)
		{
			return std::nullopt;
		}

		RequireSuccess(
			Status,
			"Pci.GetBarAttributes"
		);

		if (!Resources)
		{
			ContractViolation(
				"Null BAR resources"
			);
		}

		auto Storage = Boot::PoolAllocation::Adopt(
			Resources,
			0
		);

		EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR Descriptor{};
		const auto* Bytes = static_cast<const UINT8*>(Resources);

		if (Bytes[0] == ACPI_END_TAG_DESCRIPTOR)
		{
			Storage.Close();
			return std::nullopt;
		}

		if (Bytes[0] != ACPI_ADDRESS_SPACE_DESCRIPTOR)
		{
			ContractViolation(
				"Unsupported BAR descriptor"
			);
		}

		UINT16 Length{};

		std::memcpy(
			&Length,
			Bytes + 1,
			sizeof(Length)
		);

		if (Length != sizeof(Descriptor) - 3)
		{
			ContractViolation(
				"Invalid BAR descriptor length"
			);
		}

		std::memcpy(
			&Descriptor,
			Resources,
			sizeof(Descriptor)
		);

		if (Bytes[sizeof(Descriptor)] != ACPI_END_TAG_DESCRIPTOR || !Descriptor.AddrLen || Descriptor.AddrLen - 1 > UINT64_MAX - Descriptor.AddrRangeMin)
		{
			ContractViolation(
				"Invalid BAR resource extent"
			);
		}

		ResourceKind Kind{};

		if (Descriptor.ResType == ACPI_ADDRESS_SPACE_TYPE_IO)
		{
			Kind = ResourceKind::Io;
		}
		else if (Descriptor.ResType == ACPI_ADDRESS_SPACE_TYPE_MEM && (Descriptor.AddrSpaceGranularity == 32 || Descriptor.AddrSpaceGranularity == 64))
		{
			Kind = Descriptor.AddrSpaceGranularity == 64 ? ResourceKind::Memory64 : ResourceKind::Memory32;
		}
		else
		{
			ContractViolation(
				"Invalid BAR resource type"
			);
		}

		const BarInfo Result
		{
			Index,
			Kind,
			Descriptor.AddrRangeMin,
			Descriptor.AddrLen,
			Descriptor.AddrTranslationOffset,
			Supports,
			Kind != ResourceKind::Io && (Descriptor.SpecificFlag & 6) == 6
		};

		Storage.Close();

		return Result;
	}

	auto Bars(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> std::vector<BarInfo>
	{
		std::vector<BarInfo> Result;

		for (UINT8 Index{}; Index < 6; ++Index)
		{
			if (auto Resource = GetBar(Interface, Index))
			{
				Result.push_back(
					*Resource
				);
			}
		}

		return Result;
	}

	auto Attributes(
		EFI_PCI_IO_PROTOCOL& Interface,
		EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION Operation,
		UINT64 Bits
	) -> UINT64
	{
		if (!Interface.Attributes)
		{
			ContractViolation(
				"Missing PCI Attributes"
			);
		}

		UINT64 Result{};

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.Attributes(
						&Interface,
						Operation,
						Bits,
						&Result
					);
				}
			),
			"Pci.Attributes"
		);

		return Result;
	}

	auto Flush(
		EFI_PCI_IO_PROTOCOL& Interface
	) -> void
	{
		if (!Interface.Flush)
		{
			ContractViolation(
				"Missing PCI Flush"
			);
		}
		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.Flush(
						&Interface
					);
				}
			),
			"Pci.Flush"
		);
	}
}
