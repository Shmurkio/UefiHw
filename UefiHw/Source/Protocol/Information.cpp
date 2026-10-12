#include <UefiHw/Protocol/Information.hpp>
#include <UefiHw/Boot/Pool.hpp>
#include <UefiHw/Checked.hpp>
#include <UefiHw/Firmware.hpp>

namespace UefiHw::Protocol
{
	auto OpenInformation(
		EFI_HANDLE Handle,
		EFI_GUID Guid
	) -> std::vector<EFI_OPEN_PROTOCOL_INFORMATION_ENTRY>
	{
		if (!Handle)
		{
			throw std::invalid_argument(
				"OpenInformation requires a handle"
			);
		}

		EFI_OPEN_PROTOCOL_INFORMATION_ENTRY* Entries{};
		UINTN Count{};

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.OpenProtocolInformation(
						Handle,
						&Guid,
						&Entries,
						&Count
					);
				}
			),
			"OpenProtocolInformation"
		);

		auto Allocation = Boot::PoolAllocation::Adopt(
			Entries,
			0
		);

		if (Count && !Entries)
		{
			ContractViolation(
				"OpenProtocolInformation returned null entries"
			);
		}

		[[maybe_unused]]
		const auto Status = CheckedMultiply(
			Count,
			sizeof(*Entries)
		);

		std::vector<EFI_OPEN_PROTOCOL_INFORMATION_ENTRY> Result;

		if (Count)
		{
			Result.assign(
				Entries,
				Entries + Count
			);
		}

		Allocation.Close();

		return Result;
	}
}
