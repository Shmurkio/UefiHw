#pragma once

#include <UefiHw/Driver/Controller.hpp>
#include <memory>

namespace UefiHw::Driver
{
	class Binding
	{
	public:
		virtual ~Binding() noexcept = default;

		[[nodiscard]]
		virtual auto Supported(
			EFI_HANDLE Agent,
			EFI_HANDLE Controller,
			EFI_DEVICE_PATH_PROTOCOL* RemainingPath
		) -> EFI_STATUS = 0;

		[[nodiscard]]
		virtual auto CreateController(
			EFI_HANDLE Agent,
			EFI_HANDLE Controller
		) -> std::unique_ptr<Driver::Controller> = 0;
	};
}
