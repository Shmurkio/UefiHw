#include <UefiHw/Protocol/Handles.hpp>
#include <UefiHw/Boot/Pool.hpp>
#include <UefiHw/Firmware.hpp>
#include <cstring>

namespace UefiHw::Protocol
{
	auto EnumerateHandles(
		EFI_GUID Guid
	) -> std::vector<EFI_HANDLE>
	{
		UINTN Required{};

		auto Query = [&](
			UINTN& Size,
			void* Buffer
		) -> EFI_STATUS
		{
			return FirmwareStatus(
				UefiCpp::Firmware::TryCall(
					[&](
						EFI_BOOT_SERVICES& BootServices
					) noexcept -> EFI_STATUS
					{
						return BootServices.LocateHandle(
							ByProtocol,
							&Guid,
							nullptr,
							&Size,
							static_cast<EFI_HANDLE*>(Buffer)
						);
					}
				),
				"LocateHandle"
			);
		};

		const auto Probe = Query(
			Required,
			nullptr
		);

		if (Probe == EFI_NOT_FOUND)
		{
			return {};
		}

		if (Probe != EFI_BUFFER_TOO_SMALL)
		{
			RequireSuccess(
				Probe,
				"LocateHandle"
			);

			ContractViolation(
				"LocateHandle probe"
			);
		}

		for (unsigned Attempt{}; Attempt < 8; ++Attempt)
		{
			if (!Required || Required % sizeof(EFI_HANDLE))
			{
				ContractViolation(
					"LocateHandle buffer size"
				);
			}

			Boot::PoolAllocation Buffer(
				Required
			);

			auto Actual = Required;

			const auto Status = Query(
				Actual,
				Buffer.Bytes().data()
			);

			if (Status == EFI_NOT_FOUND)
			{
				return {};
			}

			if (Status == EFI_BUFFER_TOO_SMALL)
			{
				if (Actual <= Required)
				{
					ContractViolation(
						"LocateHandle non-increasing size"
					);
				}

				Required = Actual;

				continue;
			}
			RequireSuccess(
				Status,
				"LocateHandle"
			);

			if (Actual > Required || Actual % sizeof(EFI_HANDLE))
			{
				ContractViolation(
					"LocateHandle result size"
				);
			}
			std::vector<EFI_HANDLE> Handles(Actual / sizeof(EFI_HANDLE));

			if (Actual)
			{
				std::memcpy(
					Handles.data(),
					Buffer.Bytes().data(),
					Actual
				);
			}

			for (const auto Handle : Handles)
			{
				if (!Handle)
				{
					ContractViolation(
						"LocateHandle null handle"
					);
				}
			}

			Buffer.Close();

			return Handles;
		}

		throw FirmwareError
		{
			ErrorStage::Firmware,
			EFI_NOT_READY,
			"LocateHandle unstable snapshot"
		};
	}
}
