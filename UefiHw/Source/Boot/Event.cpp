#include <UefiHw/Boot/Event.hpp>
#include "EventSupport.hpp"
#include <utility>

namespace UefiHw::Boot
{
	Event::Event() : Handle(Detail::CreateEvent(0))
	{
	}

	Event::~Event() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"CloseEvent"
			);
		}
	}

	Event::Event(
		Event&& Other
	) noexcept : Handle(std::exchange(Other.Handle, nullptr))
	{
	}

	auto Event::Signal() -> void
	{
		Detail::SignalEvent(
			Handle
		);
	}

	auto Event::TryConsume() -> bool
	{
		return Detail::ConsumeEvent(
			Handle
		);
	}

	auto Event::Close() -> void
	{
		Detail::CloseEvent(
			Handle
		);
	}
}
