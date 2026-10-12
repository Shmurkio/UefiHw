#pragma once

#include <UefiHw/Status.hpp>
#include <UefiCpp/Firmware.hpp>

namespace UefiHw
{
	[[nodiscard]]
	inline auto FirmwareStatus(
		const UefiCpp::Firmware::Outcome<EFI_STATUS>& Result,
		std::string_view Operation
	) -> EFI_STATUS
	{
		if (!Result.Executed())
		{
			throw FirmwareError
			{
				ErrorStage::Admission,
				Result.Admission(),
				Operation
			};
		}

		return *Result.Value();
	}

	inline auto RequireSuccess(
		EFI_STATUS Status,
		std::string_view Operation
	) -> void
	{
		if (Status != EFI_SUCCESS)
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				Status,
				Operation
			};
		}
	}

	inline auto RequireSuccess(
		const UefiCpp::Firmware::Outcome<EFI_STATUS>& Result,
		std::string_view Operation
	) -> void
	{
		RequireSuccess(
			FirmwareStatus(Result, Operation),
			Operation
		);
	}

	[[noreturn]]
	inline auto ContractViolation(
		std::string_view Operation
	) -> void
	{
		throw FirmwareError
		{
			ErrorStage::Contract,
			EFI_COMPROMISED_DATA,
			Operation
		};
	}
}
