#pragma once

#include <UefiHw/Pci/Types.hpp>
#include <memory>
#include <span>

namespace UefiHw::Pci
{
	class Device;

	namespace Detail
	{
		struct DeviceState;
	}
}

namespace UefiHw::Io
{
	class Bar final
	{
	public:
		[[nodiscard]]
		auto Info() const -> Pci::BarInfo;

		template <Pci::RegisterValue T>
		[[nodiscard]]
		auto Read(
			UINT64 Offset
		) const -> T
		{
			T Value{};

			Access(
				false,
				Offset,
				sizeof(T),
				1,
				&Value
			);

			return Value;
		}

		template <Pci::RegisterValue T>
		auto Write(
			UINT64 Offset,
			T Value
		) const -> void
		{
			Access(
				true,
				Offset,
				sizeof(T),
				1,
				&Value
			);
		}

		template <Pci::RegisterValue T>
		auto Read(
			UINT64 Offset,
			std::span<T> Values
		) const -> void
		{
			Access(
				false,
				Offset,
				sizeof(T),
				Values.size(),
				Values.data()
			);
		}

		template <Pci::RegisterValue T>
		auto Write(
			UINT64 Offset,
			std::span<const T> Values
		) const -> void
		{
			Access(
				true,
				Offset,
				sizeof(T),
				Values.size(),
				const_cast<T*>(Values.data())
			);
		}

	private:
		friend class UefiHw::Pci::Device;

		Bar(
			std::weak_ptr<Pci::Detail::DeviceState> Owner,
			UINT8 Index
		) noexcept;

		auto Access(
			bool Write,
			UINT64 Offset,
			std::size_t Width,
			std::size_t Count,
			void* Buffer
		) const -> void;

		std::weak_ptr<Pci::Detail::DeviceState> Context;
		UINT8 Index{};
	};
}
