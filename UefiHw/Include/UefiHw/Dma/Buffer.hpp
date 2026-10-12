#pragma once

#include <UefiHw/Pci/Device.hpp>
#include <cstddef>
#include <memory>
#include <span>

namespace UefiHw::Dma
{
	enum class Direction
	{
		DeviceReads,
		DeviceWrites,
		CommonBuffer
	};

	struct DeviceAddress final
	{
		UINT64 Value{};
	};

	namespace Detail
	{
		struct Storage;
	}
	class Mapping;

	class Buffer final
	{
	public:
		Buffer(
			Pci::Device& Device,
			std::size_t Size,
			UINT64 Attributes = 0,
			std::size_t Alignment = EFI_PAGE_SIZE
		);

		~Buffer() noexcept;
		Buffer(const Buffer&) = delete;
		auto operator=(const Buffer&) -> Buffer& = delete;
		Buffer(Buffer&& Other) noexcept;
		auto operator=(Buffer&&) -> Buffer& = delete;

		[[nodiscard]]
		auto Bytes() -> std::span<std::byte>;

		[[nodiscard]]
		auto Size() const noexcept -> std::size_t;

		[[nodiscard]]
		auto Map(
			Direction Transfer,
			std::size_t Offset,
			std::size_t Length
		) -> Mapping;

		auto Close() -> void;

	private:
		std::shared_ptr<Detail::Storage> Context;
	};

	class Mapping final
	{
	public:
		~Mapping() noexcept;
		Mapping(const Mapping&) = delete;
		auto operator=(const Mapping&) -> Mapping& = delete;
		Mapping(Mapping&& Other) noexcept;
		auto operator=(Mapping&&) -> Mapping& = delete;

		[[nodiscard]]
		auto Address(
			std::size_t ByteOffset = 0
		) const -> DeviceAddress;

		[[nodiscard]]
		auto Size() const noexcept -> std::size_t;

		[[nodiscard]]
		auto TransferDirection() const noexcept -> Direction;

		[[nodiscard]]
		auto CommonBytes() -> std::span<std::byte>;

		auto BeginDeviceAccess() -> void;

		auto CompleteDeviceAccess() -> void;

		auto Close() -> void;

	private:
		friend class Buffer;

		Mapping(
			std::shared_ptr<Detail::Storage> Storage,
			Direction Transfer,
			std::size_t Offset,
			std::size_t Length
		);

		enum class Phase
		{
			Empty,
			Mapped,
			InFlight,
			Indeterminate
		};

		std::shared_ptr<Detail::Storage> Context;
		void* Token{};
		UINT64 BusAddress{};
		std::size_t Length{};
		std::size_t Offset{};
		Direction Transfer{};
		Phase State{ Phase::Empty };
	};
}
