#include <UefiHw/Boot/Timer.hpp>
#include "EventSupport.hpp"
#include <utility>

namespace UefiHw::Boot
{
	TimerEvent::TimerEvent() : Handle(Detail::CreateEvent(EVT_TIMER))
	{
	}

	TimerEvent::~TimerEvent() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"CloseEvent(timer)"
			);
		}
	}

	TimerEvent::TimerEvent(
		TimerEvent&& Other
	) noexcept : Handle(std::exchange(Other.Handle, nullptr))
	{
	}

	auto TimerEvent::Arm(
		EFI_TIMER_DELAY Mode,
		UINT64 Ticks
	) -> void
	{
		Detail::SetTimer(
			Handle,
			Mode,
			Ticks
		);
	}

	auto TimerEvent::Cancel() -> void
	{
		Detail::SetTimer(
			Handle,
			TimerCancel,
			0
		);
	}

	auto TimerEvent::TryConsume() -> bool
	{
		return Detail::ConsumeEvent(
			Handle
		);
	}

	auto TimerEvent::Close() -> void
	{
		Detail::CloseEvent(
			Handle
		);
	}
}
