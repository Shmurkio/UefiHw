#pragma once

#include <Uefi.h>

#include <exception>
#include <array>
#include <string_view>

namespace UefiHw
{
	enum class ErrorStage
	{
		Admission,
		Firmware,
		Contract
	};

	class FirmwareError final : public std::exception
	{
	public:
		FirmwareError(
			ErrorStage Stage,
			EFI_STATUS Status,
			std::string_view Operation
		) noexcept;

		[[nodiscard]]
		auto Stage() const noexcept -> ErrorStage;

		[[nodiscard]]
		auto Status() const noexcept -> EFI_STATUS;

		[[nodiscard]]
		auto Operation() const noexcept -> std::string_view;

		[[nodiscard]]
		auto what() const noexcept -> const char* override;

	private:
		ErrorStage FailureStage{};
		EFI_STATUS StatusCode{};
		std::array<char, 96> OperationName{};
		std::size_t OperationLength{};
	};

	using CleanupFailureHandler = void (*)(const FirmwareError&) noexcept;

	auto SetCleanupFailureHandler(
		CleanupFailureHandler Handler
	) noexcept -> CleanupFailureHandler;

	[[noreturn]]
	auto FailCleanup(
		const FirmwareError& Error
	) noexcept -> void;

	[[noreturn]]
	auto FailCurrentCleanup(
		std::string_view Operation
	) noexcept -> void;

	[[nodiscard]]
	auto CurrentExceptionStatus() noexcept -> EFI_STATUS;
}
