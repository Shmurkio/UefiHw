#pragma once

#include <Uefi.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace UefiHw
{
	[[nodiscard]]
	constexpr auto CheckedMultiply(
		std::size_t Count,
		std::size_t Size
	) -> std::size_t
	{
		if (Size && Count > std::numeric_limits<std::size_t>::max() / Size)
		{
			throw std::length_error(
				"Byte count overflow"
			);
		}

		return Count * Size;
	}

	[[nodiscard]]
	constexpr auto PagesForBytes(
		std::size_t Size
	) -> std::size_t
	{
		const auto Pages = Size / EFI_PAGE_SIZE + (Size % EFI_PAGE_SIZE != 0);

		[[maybe_unused]]
		const auto Status = CheckedMultiply(
			Pages,
			EFI_PAGE_SIZE
		);

		return Pages;
	}

	inline auto ValidateMemoryType(
		EFI_MEMORY_TYPE Type
	) -> void
	{
		if (Type != EfiLoaderCode && Type != EfiLoaderData && Type != EfiBootServicesCode && Type != EfiBootServicesData && Type != EfiRuntimeServicesCode && Type != EfiRuntimeServicesData && Type != EfiACPIReclaimMemory && Type != EfiACPIMemoryNVS && static_cast<UINT32>(Type) < 0x80000000U)
		{
			throw std::invalid_argument(
				"Memory type is not allocatable"
			);
		}
	}

	inline auto ValidateRange(
		std::uint64_t Offset,
		std::size_t Width,
		std::size_t Count,
		std::uint64_t Limit
	) -> void
	{
		if ((Width != 1 && Width != 2 && Width != 4 && Width != 8) || Offset % Width)
		{
			throw std::invalid_argument(
				"Invalid access width or alignment"
			);
		}

		const auto Size = CheckedMultiply(
			Width,
			Count
		);

		if (Offset > Limit || Size > Limit - Offset)
		{
			throw std::out_of_range(
				"Access exceeds resource bounds"
			);
		}
	}
}
