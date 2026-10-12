#pragma once

#include <Uefi.h>
#include <Protocol/DevicePath.h>
#include <span>

namespace UefiHw::Driver
{
	class Controller
	{
	public:
		virtual ~Controller() noexcept = default;

		[[nodiscard]]
		virtual auto Start(
			EFI_DEVICE_PATH_PROTOCOL* RemainingPath
		) -> EFI_STATUS = 0;

		[[nodiscard]]
		virtual auto Stop() -> EFI_STATUS = 0;

		[[nodiscard]]
		virtual auto StopChildren(
			std::span<const EFI_HANDLE>
		) -> EFI_STATUS
		{
			return EFI_UNSUPPORTED;
		}
	};
}
