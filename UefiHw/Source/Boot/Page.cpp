#include <UefiHw/Boot/Page.hpp>
#include <UefiHw/Checked.hpp>
#include <UefiHw/Firmware.hpp>
#include <utility>

namespace UefiHw::Boot
{
	PageAllocation::PageAllocation(
		std::size_t PageCount,
		EFI_MEMORY_TYPE MemoryType,
		PageRequest Request
	)
	{
		ValidateMemoryType(
			MemoryType
		);

		if (!PageCount || Request.Policy < AllocateAnyPages || Request.Policy >= MaxAllocateType || (Request.Policy == AllocateAddress && Request.Address % EFI_PAGE_SIZE))
		{
			throw std::invalid_argument(
				"Invalid page allocation request"
			);
		}

		const auto Size = CheckedMultiply(
			PageCount,
			EFI_PAGE_SIZE
		);

		if (Request.Policy == AllocateAddress && Size - 1 > UINT64_MAX - Request.Address)
		{
			throw std::length_error(
				"Physical allocation address overflow"
			);
		}

		auto Address = Request.Address;

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.AllocatePages(
						Request.Policy,
						MemoryType,
						PageCount,
						&Address
					);
				}
			),
			"AllocatePages"
		);

		PhysicalAddress = Address;
		Pages = PageCount;

		if (Address % EFI_PAGE_SIZE || Size - 1 > UINT64_MAX - Address || (Request.Policy == AllocateAddress && Address != Request.Address) || (Request.Policy == AllocateMaxAddress && (Address > Request.Address || Size - 1 > Request.Address - Address)))
		{
			try
			{
				Close();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"AllocatePages rollback"
				);
			}

			ContractViolation(
				"AllocatePages address"
			);
		}
	}

	PageAllocation::~PageAllocation() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"FreePages"
			);
		}
	}

	PageAllocation::PageAllocation(
		PageAllocation&& Other
	) noexcept : PhysicalAddress(std::exchange(Other.PhysicalAddress, 0)), Pages(std::exchange(Other.Pages, 0))
	{
	}

	auto PageAllocation::Address() const noexcept -> EFI_PHYSICAL_ADDRESS
	{
		return PhysicalAddress;
	}

	auto PageAllocation::PageCount() const noexcept -> std::size_t
	{
		return Pages;
	}

	auto PageAllocation::SizeInBytes() const noexcept -> std::size_t
	{
		return Pages * EFI_PAGE_SIZE;
	}

	auto PageAllocation::Close() -> void
	{
		if (!Pages)
		{
			return;
		}

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[this](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.FreePages(
						PhysicalAddress,
						Pages
					);
				}
			),
			"FreePages"
		);

		PhysicalAddress = 0;
		Pages = 0;
	}
}
