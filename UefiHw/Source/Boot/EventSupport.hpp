#pragma once

#include <UefiHw/Firmware.hpp>
#include <stdexcept>

namespace UefiHw::Boot::Detail
{
	[[nodiscard]]
	inline auto CreateEvent(
		UINT32 Type,
		EFI_EVENT_NOTIFY Notify = nullptr,
		void* Context = nullptr
	) -> EFI_EVENT
	{
		EFI_EVENT Handle{};
		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.CreateEvent(
						Type,
						Notify ? TPL_CALLBACK : TPL_APPLICATION,
						Notify,
						Context,
						&Handle
					);
				}
			),
			"CreateEvent"
		);

		if (!Handle)
		{
			ContractViolation(
				"CreateEvent returned null"
			);
		}

		return Handle;
	}

	inline auto RequireEvent(
		EFI_EVENT Handle
	) -> void
	{
		if (!Handle)
		{
			throw std::logic_error(
				"Event is closed"
			);
		}
	}

	inline auto CloseEvent(
		EFI_EVENT& Handle
	) -> void
	{
		if (!Handle)
		{
			return;
		}

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[Handle](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.CloseEvent(
						Handle
					);
				}
			),
			"CloseEvent"
		);

		Handle = nullptr;
	}

	inline auto SignalEvent(
		EFI_EVENT Handle
	) -> void
	{
		RequireEvent(
			Handle
		);

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[Handle](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.SignalEvent(
						Handle
					);
				}
			),
			"SignalEvent"
		);
	}

	[[nodiscard]]
	inline auto ConsumeEvent(
		EFI_EVENT Handle
	) -> bool
	{
		RequireEvent(
			Handle
		);

		const auto Status = FirmwareStatus(
			UefiCpp::Firmware::TryCall(
				[Handle](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.CheckEvent(
						Handle
					);
				}
			),
			"CheckEvent"
		);

		if (Status == EFI_NOT_READY)
		{
			return false;
		}

		RequireSuccess(
			Status,
			"CheckEvent"
		);

		return true;
	}

	inline auto SetTimer(
		EFI_EVENT Handle,
		EFI_TIMER_DELAY Mode,
		UINT64 Ticks
	) -> void
	{
		RequireEvent(
			Handle
		);

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[=](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.SetTimer(
						Handle,
						Mode,
						Ticks
					);
				}
			),
			"SetTimer"
		);
	}
}
