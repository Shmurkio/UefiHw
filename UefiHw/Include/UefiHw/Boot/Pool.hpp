#pragma once

#include <Uefi.h>
#include <cstddef>
#include <span>

namespace UefiHw::Boot
{
	class PoolAllocation final
	{
	public:
		PoolAllocation() noexcept = default;

		explicit PoolAllocation(
			std::size_t Size,
			EFI_MEMORY_TYPE MemoryType = EfiBootServicesData
		);

		~PoolAllocation() noexcept;
		PoolAllocation(const PoolAllocation&) = delete;
		auto operator=(const PoolAllocation&) -> PoolAllocation& = delete;
		PoolAllocation(PoolAllocation&& Other) noexcept;
		auto operator=(PoolAllocation&&) -> PoolAllocation& = delete;

		[[nodiscard]]
		static auto Adopt(
			void* Memory,
			std::size_t Size
		) -> PoolAllocation;

		[[nodiscard]]
		auto Bytes() noexcept -> std::span<std::byte>;

		[[nodiscard]]
		auto Bytes() const noexcept -> std::span<const std::byte>;

		[[nodiscard]]
		auto Size() const noexcept -> std::size_t;

		auto Close() -> void;

	private:
		void* Memory{};
		std::size_t SizeInBytes{};
	};
}
