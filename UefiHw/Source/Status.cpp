#include <UefiHw/Status.hpp>
#include <algorithm>
#include <atomic>
#include <new>
#include <stdexcept>

namespace
{
	std::atomic<UefiHw::CleanupFailureHandler> FailureHandler{};
}

namespace UefiHw
{
	FirmwareError::FirmwareError(
		ErrorStage Stage,
		EFI_STATUS Status,
		std::string_view Operation
	) noexcept : FailureStage(Stage), StatusCode(Status)
	{
		OperationLength = std::min(
			Operation.size(),
			OperationName.size() - 1
		);

		std::copy_n(
			Operation.data(),
			OperationLength,
			OperationName.data()
		);
	}

	auto FirmwareError::Stage() const noexcept -> ErrorStage
	{
		return FailureStage;
	}

	auto FirmwareError::Status() const noexcept -> EFI_STATUS
	{
		return StatusCode;
	}

	auto FirmwareError::Operation() const noexcept -> std::string_view
	{
		return
		{
			OperationName.data(),
			OperationLength
		};
	}

	auto FirmwareError::what() const noexcept -> const char*
	{
		switch (FailureStage)
		{
		case ErrorStage::Admission:
		{
			return "UEFI firmware call admission denied";
		}
		case ErrorStage::Firmware:
		{
			return "UEFI firmware operation failed";
		}
		case ErrorStage::Contract:
		{
			return "UEFI firmware contract violated";
		}
		default:
		{
			return "Unknown UEFI firmware error";
		}
		}
	}

	auto SetCleanupFailureHandler(
		CleanupFailureHandler Handler
	) noexcept -> CleanupFailureHandler
	{
		return FailureHandler.exchange(
			Handler,
			std::memory_order_acq_rel
		);
	}

	auto FailCleanup(
		const FirmwareError& Error
	) noexcept -> void
	{
		if (const auto Handler = FailureHandler.load(std::memory_order_acquire))
		{
			Handler(
				Error
			);
		}

		std::terminate();
	}

	auto CurrentExceptionStatus() noexcept -> EFI_STATUS
	{
		try
		{
			throw;
		}
		catch (const FirmwareError& Error)
		{
			return EFI_ERROR(Error.Status()) ? Error.Status() : EFI_DEVICE_ERROR;
		}
		catch (const std::bad_alloc&)
		{
			return EFI_OUT_OF_RESOURCES;
		}
		catch (const std::invalid_argument&)
		{
			return EFI_INVALID_PARAMETER;
		}
		catch (const std::out_of_range&)
		{
			return EFI_INVALID_PARAMETER;
		}
		catch (...)
		{
			return EFI_ABORTED;
		}
	}

	auto FailCurrentCleanup(
		std::string_view Operation
	) noexcept -> void
	{
		try
		{
			throw;
		}
		catch (const FirmwareError& Error)
		{
			FailCleanup(
				Error
			);
		}
		catch (...)
		{
			FailCleanup(
				{
					ErrorStage::Contract,
					EFI_ABORTED,
					Operation
				}
			);
		}
	}
}
