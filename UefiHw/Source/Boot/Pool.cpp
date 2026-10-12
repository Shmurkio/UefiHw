#include <UefiHw/Boot/Pool.hpp>
#include <UefiHw/Checked.hpp>
#include <UefiHw/Firmware.hpp>
#include <utility>

namespace UefiHw::Boot
{
	PoolAllocation::PoolAllocation(
		std::size_t Size,
		EFI_MEMORY_TYPE MemoryType
	)
	{
		ValidateMemoryType(
			MemoryType
		);

		if (!Size)
		{
			return;
		}

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.AllocatePool(
						MemoryType,
						Size,
						&Memory
					);
				}
			),
			"AllocatePool"
		);

		if (!Memory)
		{
			ContractViolation(
				"AllocatePool returned null"
			);
		}

		SizeInBytes = Size;
	}

	PoolAllocation::~PoolAllocation() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"FreePool"
			);
		}
	}

	PoolAllocation::PoolAllocation(
		PoolAllocation&& Other
	) noexcept : Memory(std::exchange(Other.Memory, nullptr)), SizeInBytes(std::exchange(Other.SizeInBytes, 0))
	{
	}

	auto PoolAllocation::Adopt(
		void* Memory,
		std::size_t Size
	) -> PoolAllocation
	{
		if (!Memory && Size)
		{
			throw std::invalid_argument(
				"Null pool storage"
			);
		}

		PoolAllocation Result;
		Result.Memory = Memory;
		Result.SizeInBytes = Size;

		return Result;
	}

	auto PoolAllocation::Bytes() noexcept -> std::span<std::byte>
	{
		return
		{
			static_cast<std::byte*>(Memory),
			SizeInBytes
		};
	}

	auto PoolAllocation::Bytes() const noexcept -> std::span<const std::byte>
	{
		return
		{
			static_cast<const std::byte*>(Memory),
			SizeInBytes
		};
	}

	auto PoolAllocation::Size() const noexcept -> std::size_t
	{
		return SizeInBytes;
	}

	auto PoolAllocation::Close() -> void
	{
		if (!Memory)
		{
			return;
		}

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[this](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.FreePool(
						Memory
					);
				}
			),
			"FreePool"
		);

		Memory = nullptr;
		SizeInBytes = 0;
	}
}
