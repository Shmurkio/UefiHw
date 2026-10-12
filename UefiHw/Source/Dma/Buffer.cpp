#include <UefiHw/Dma/Buffer.hpp>
#include "../Pci/State.hpp"
#include <utility>

namespace UefiHw::Dma
{
	namespace Detail
	{
		struct Storage final
		{
			std::shared_ptr<Pci::Detail::DeviceState> Device;
			void* Memory{};
			std::size_t Pages{};
			std::size_t Size{};
			bool Mapped{};

			~Storage() noexcept
			{
				try
				{
					Close();
				}
				catch (...)
				{
					FailCurrentCleanup(
						"Pci.FreeBuffer"
					);
				}
			}

			auto Close() -> void
			{
				if (!Pages)
				{
					return;
				}

				if (Mapped)
				{
					throw std::logic_error(
						"DMA buffer has an active mapping"
					);
				}

				auto& Interface = Device->Interface();

				if (!Interface.FreeBuffer)
				{
					ContractViolation(
						"Missing PCI FreeBuffer"
					);
				}

				RequireSuccess(
					UefiCpp::Firmware::TryCall(
						[&](
							EFI_BOOT_SERVICES&
						) noexcept -> EFI_STATUS
						{
							return Interface.FreeBuffer(
								&Interface,
								Pages,
								Memory
							);
						}
					),
					"Pci.FreeBuffer"
				);

				--Device->Buffers;
				Memory = nullptr;
				Pages = 0;
				Size = 0;
			}
		};
	}

	Buffer::Buffer(
		Pci::Device& Device,
		std::size_t Size,
		UINT64 Attributes,
		std::size_t Alignment
	) : Context(std::make_shared<Detail::Storage>())
	{
		constexpr UINT64 Allowed = EFI_PCI_IO_ATTRIBUTE_MEMORY_CACHED | EFI_PCI_IO_ATTRIBUTE_MEMORY_WRITE_COMBINE | EFI_PCI_IO_ATTRIBUTE_DUAL_ADDRESS_CYCLE;

		if (!Size || !Alignment || Alignment > EFI_PAGE_SIZE || (Alignment & (Alignment - 1)) || (Attributes & ~Allowed))
		{
			throw std::invalid_argument(
				"Invalid DMA size, alignment or attributes"
			);
		}

		if (!Device.IsOpen())
		{
			throw std::logic_error(
				"DMA requires an owned PCI device"
			);
		}

		Context->Device = Device.Context;
		const auto Pages = PagesForBytes(Size);
		auto& Interface = Context->Device->Interface();

		if (!Interface.AllocateBuffer || !Interface.FreeBuffer || !Interface.Map || !Interface.Unmap)
		{
			ContractViolation(
				"Missing PCI DMA operations"
			);
		}
		void* Memory{};

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.AllocateBuffer(
						&Interface,
						AllocateAnyPages,
						EfiBootServicesData,
						Pages,
						&Memory,
						Attributes
					);
				}
			),
			"Pci.AllocateBuffer"
		);

		Context->Memory = Memory;
		Context->Pages = Pages;
		Context->Size = Size;
		++Context->Device->Buffers;

		if (!Memory || reinterpret_cast<std::uintptr_t>(Memory) % EFI_PAGE_SIZE)
		{
			ContractViolation(
				"Invalid DMA buffer address"
			);
		}
	}

	Buffer::~Buffer() noexcept = default;

	Buffer::Buffer(
		Buffer&& Other
	) noexcept : Context(std::move(Other.Context))
	{
	}

	auto Buffer::Bytes() -> std::span<std::byte>
	{
		if (!Context || !Context->Pages || Context->Mapped)
		{
			throw std::logic_error(
				"DMA buffer is closed or mapped"
			);
		}

		return
		{
			static_cast<std::byte*>(Context->Memory),
			Context->Size
		};
	}

	auto Buffer::Size() const noexcept -> std::size_t
	{
		return Context ? Context->Size : 0;
	}

	auto Buffer::Map(
		Direction Transfer,
		std::size_t Offset,
		std::size_t Length
	) -> Mapping
	{
		return Mapping
		{
			Context,
			Transfer,
			Offset,
			Length
		};
	}

	auto Buffer::Close() -> void
	{
		if (Context)
		{
			Context->Close();
			Context.reset();
		}
	}

	Mapping::Mapping(
		std::shared_ptr<Detail::Storage> Storage,
		Direction Transfer,
		std::size_t Offset,
		std::size_t Length
	) : Context(std::move(Storage)), Length(Length), Offset(Offset), Transfer(Transfer)
	{
		if (!Context || !Context->Pages || Context->Mapped)
		{
			throw std::logic_error(
				"DMA mapping requires an unmapped live buffer"
			);
		}

		if (!Length || Offset > Context->Size || Length > Context->Size - Offset)
		{
			throw std::out_of_range(
				"DMA mapping range"
			);
		}

		EFI_PCI_IO_PROTOCOL_OPERATION Operation{};

		switch (Transfer)
		{
		case Direction::DeviceReads:
		{
			Operation = EfiPciIoOperationBusMasterRead;
			break;
		}
		case Direction::DeviceWrites:
		{
			Operation = EfiPciIoOperationBusMasterWrite;
			break;
		}
		case Direction::CommonBuffer:
		{
			Operation = EfiPciIoOperationBusMasterCommonBuffer;
			break;
		}
		default:
		{
			throw std::invalid_argument(
				"Invalid DMA direction"
			);
		}
		}

		auto& Interface = Context->Device->Interface();
		auto Actual = Length;

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.Map(
						&Interface,
						Operation,
						static_cast<std::byte*>(Context->Memory) + Offset,
						&Actual,
						&BusAddress,
						&Token
					);
				}
			),
			"Pci.Map"
		);

		State = Phase::Mapped;
		Context->Mapped = true;
		this->Length = Actual;

		if (!Actual || Actual > Length || Actual - 1 > UINT64_MAX - BusAddress || (Transfer == Direction::CommonBuffer && Actual != Length))
		{
			try
			{
				Close();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"Pci.Map rollback"
				);
			}

			ContractViolation(
				"Invalid DMA mapped length or address"
			);
		}
	}

	Mapping::~Mapping() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"Pci.Unmap"
			);
		}
	}

	Mapping::Mapping(
		Mapping&& Other
	) noexcept : Context(std::move(Other.Context)), Token(std::exchange(Other.Token, nullptr)), BusAddress(Other.BusAddress), Length(std::exchange(Other.Length, 0)), Offset(Other.Offset), Transfer(Other.Transfer), State(std::exchange(Other.State, Phase::Empty))
	{
	}

	auto Mapping::Address(
		std::size_t ByteOffset
	) const -> DeviceAddress
	{
		if (State != Phase::Mapped && State != Phase::InFlight)
		{
			throw std::logic_error(
				"DMA mapping is not valid"
			);
		}

		if (ByteOffset >= Length)
		{
			throw std::out_of_range(
				"DMA device address offset"
			);
		}

		return
		{
			BusAddress + ByteOffset
		};
	}

	auto Mapping::Size() const noexcept -> std::size_t
	{
		return Length;
	}

	auto Mapping::TransferDirection() const noexcept -> Direction
	{
		return Transfer;
	}

	auto Mapping::CommonBytes() -> std::span<std::byte>
	{
		if (Transfer != Direction::CommonBuffer || (State != Phase::Mapped && State != Phase::InFlight))
		{
			throw std::logic_error(
				"CPU access requires an active common-buffer mapping"
			);
		}

		return
		{
			static_cast<std::byte*>(Context->Memory) + Offset,
			Length
		};
	}

	auto Mapping::BeginDeviceAccess() -> void
	{
		if (State != Phase::Mapped)
		{
			throw std::logic_error(
				"DMA mapping cannot begin access"
			);
		}

		State = Phase::InFlight;
	}

	auto Mapping::CompleteDeviceAccess() -> void
	{
		if (State != Phase::InFlight)
		{
			throw std::logic_error(
				"DMA mapping has no device access in flight"
			);
		}

		if (Transfer != Direction::DeviceReads)
		{
			Pci::Detail::Flush(
				Context->Device->Interface()
			);
		}

		State = Phase::Mapped;
	}

	auto Mapping::Close() -> void
	{
		if (State == Phase::Empty)
		{
			if (Context && Context.use_count() == 1)
			{
				Context->Close();
			}

			Context.reset();

			return;
		}

		if (State != Phase::Mapped)
		{
			throw std::logic_error(
				"DMA is in flight or Unmap outcome is indeterminate"
			);
		}

		auto& Interface = Context->Device->Interface();

		const auto Status = FirmwareStatus(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES&
				) noexcept -> EFI_STATUS
				{
					return Interface.Unmap(
						&Interface,
						Token
					);
				}
			),
			"Pci.Unmap"
		);

		if (Status != EFI_SUCCESS)
		{
			State = Phase::Indeterminate;

			RequireSuccess(
				Status,
				"Pci.Unmap"
			);
		}
		Context->Mapped = false;
		State = Phase::Empty;
		Token = nullptr;
		Length = 0;

		if (Context.use_count() == 1)
		{
			Context->Close();
		}

		Context.reset();
	}
}
