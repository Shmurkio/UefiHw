
#pragma once

#include <Uefi.h>

namespace UefiHw::Boot
{
	class Event final
	{
	public:
		Event();
		~Event() noexcept;

		Event(const Event&) = delete;
		auto operator=(const Event&) -> Event& = delete;

		Event(Event&& Other) noexcept;
		auto operator=(Event&& Other) -> Event& = delete;

		auto Signal() -> void;

		[[nodiscard]]
		auto TryConsume() -> bool;

		auto Close() -> void;

	private:
		EFI_EVENT Handle{};
	};
}
