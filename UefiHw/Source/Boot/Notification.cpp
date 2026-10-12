#include <UefiHw/Boot/Notification.hpp>
#include <UefiCpp/Execution.hpp>
#include "EventSupport.hpp"
#include <atomic>
#include <utility>

namespace UefiHw::Boot
{
	struct NotificationEvent::State final
	{
		explicit State(
			std::move_only_function<void()> Callback = {}
		) : Callback(std::move(Callback))
		{
		}

		std::atomic<bool> Pending{};
		std::atomic_flag Dispatching{};
		std::move_only_function<void()> Callback;

		static auto EFIAPI Notify(
			EFI_EVENT,
			void* Opaque
		) noexcept -> void
		{
			UefiCpp::FirmwareBoundary(
				[Opaque]() noexcept -> void
				{
					static_cast<State*>(Opaque)->Pending.store(
						true,
						std::memory_order_release
					);
				}
			);
		}
	};

	static_assert(
		std::atomic<bool>::is_always_lock_free
	);

	NotificationEvent::NotificationEvent() : Context(std::make_shared<State>())
	{
		Create();
	}

	NotificationEvent::NotificationEvent(
		std::move_only_function<void()> Callback
	) : Context(std::make_shared<State>(std::move(Callback)))
	{
		if (!Context->Callback)
		{
			throw std::invalid_argument(
				"Empty notification callback"
			);
		}

		Create();
	}

	auto NotificationEvent::Create() -> void
	{
		Handle = Detail::CreateEvent(
			EVT_NOTIFY_SIGNAL,
			&State::Notify,
			Context.get()
		);
	}

	NotificationEvent::~NotificationEvent() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"CloseEvent(notification)"
			);
		}
	}

	NotificationEvent::NotificationEvent(
		NotificationEvent&& Other
	) noexcept : Context(std::move(Other.Context)), Handle(std::exchange(Other.Handle, nullptr))
	{
	}

	auto NotificationEvent::Signal() -> void
	{
		Detail::SignalEvent(
			Handle
		);
	}

	auto NotificationEvent::ConsumeNotification() noexcept -> bool
	{
		return Context && Context->Pending.exchange(false, std::memory_order_acq_rel);
	}

	auto NotificationEvent::DispatchPending() -> bool
	{
		const auto Pinned = Context;

		if (!Pinned)
		{
			return false;
		}

		if (!Pinned->Callback)
		{
			throw std::logic_error(
				"Notification has no callable"
			);
		}

		if (Pinned->Dispatching.test_and_set(std::memory_order_acquire))
		{
			throw std::logic_error(
				"Recursive or concurrent notification dispatch"
			);
		}

		struct Guard final
		{
			State& Context;

			~Guard() noexcept
			{
				Context.Dispatching.clear(
					std::memory_order_release
				);
			}

		} Active{ *Pinned };

		if (!Pinned->Pending.exchange(false, std::memory_order_acq_rel))
		{
			return false;
		}

		Pinned->Callback();

		return true;
	}

	auto NotificationEvent::Close() -> void
	{
		Detail::CloseEvent(
			Handle
		);

		if (Context)
		{
			Context->Pending.store(
				false,
				std::memory_order_release
			);
		}

		Context.reset();
	}
}
