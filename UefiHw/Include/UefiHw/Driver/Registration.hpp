#pragma once

#include <UefiHw/Driver/Binding.hpp>
#include <UefiHw/Status.hpp>
#include <Protocol/DriverBinding.h>
#include <cstddef>
#include <memory>
#include <optional>

namespace UefiHw::Driver
{
	class Registration final
	{
	public:
		Registration(
			EFI_HANDLE Image,
			std::unique_ptr<Binding> Implementation,
			UINT32 Version = 0x10,
			EFI_HANDLE BindingHandle = nullptr
		);

		~Registration() noexcept;
		Registration(const Registration&) = delete;
		auto operator=(const Registration&) -> Registration& = delete;
		Registration(Registration&& Other) noexcept;
		auto operator=(Registration&&) -> Registration& = delete;

		auto AttachUnload() -> void;

		auto Close() -> void;

		[[nodiscard]]
		auto Handle() const noexcept -> EFI_HANDLE;

		[[nodiscard]]
		auto ControllerCount() const noexcept -> std::size_t;

		[[nodiscard]]
		auto LastFailure() const -> std::optional<FirmwareError>;

	private:
		struct State;
		std::unique_ptr<State> Context;
	};
}
