#pragma once

#include <Uefi.h>
#include <cstddef>

namespace UefiHw::Boot
{
	struct PageRequest final
	{
		EFI_ALLOCATE_TYPE Policy{AllocateAnyPages};
		EFI_PHYSICAL_ADDRESS Address{};
	};

	class PageAllocation final
	{
	public:
		explicit PageAllocation(
			std::size_t PageCount,
			EFI_MEMORY_TYPE MemoryType = EfiBootServicesData,
			PageRequest Request = {}
		);

		~PageAllocation() noexcept;

		PageAllocation(const PageAllocation&) = delete;
		auto operator=(const PageAllocation&) -> PageAllocation& = delete;
		PageAllocation(PageAllocation&& Other) noexcept;
		auto operator=(PageAllocation&&) -> PageAllocation& = delete;

		[[nodiscard]]
		auto Address() const noexcept -> EFI_PHYSICAL_ADDRESS;

		[[nodiscard]]
		auto PageCount() const noexcept -> std::size_t;

		[[nodiscard]]
		auto SizeInBytes() const noexcept -> std::size_t;

		auto Close() -> void;

	private:
		EFI_PHYSICAL_ADDRESS PhysicalAddress{};
		std::size_t Pages{};
	};
}
