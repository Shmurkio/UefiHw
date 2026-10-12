#pragma once

#include <Uefi.h>

#include <functional>
#include <memory>

namespace UefiHw::Boot
{
	class NotificationEvent final
	{
	public:
		NotificationEvent();

		explicit NotificationEvent(std::move_only_function<void()> Callback);

		~NotificationEvent() noexcept;

		NotificationEvent(const NotificationEvent&) = delete;
		auto operator=(const NotificationEvent&) -> NotificationEvent& = delete;

		NotificationEvent(NotificationEvent&& Other) noexcept;
		auto operator=(NotificationEvent&& Other) -> NotificationEvent& = delete;

		auto Signal() -> void;

		[[nodiscard]]
		auto ConsumeNotification() noexcept -> bool;

		[[nodiscard]]
		auto DispatchPending() -> bool;

		auto Close() -> void;

	private:
		struct State;

		auto Create() -> void;

		std::shared_ptr<State> Context{};
		EFI_EVENT Handle{};
	};
}
