#pragma once

#include <UefiHw/Status.hpp>

#include <cstdint>
#include <format>
#include <string_view>

namespace std
{
	template <> struct formatter<UefiHw::ErrorStage, char> : formatter<string_view, char>
	{
		template <typename FormatContext>
		auto format(
			UefiHw::ErrorStage Stage,
			FormatContext& Context
		) const -> FormatContext::iterator
		{
			string_view Name;

			switch (Stage)
			{
			case UefiHw::ErrorStage::Admission:
			{
				Name = "Admission";
				break;
			}
			case UefiHw::ErrorStage::Firmware:
			{
				Name = "Firmware";
				break;
			}
			case UefiHw::ErrorStage::Contract:
			{
				Name = "Contract";
				break;
			}
			default:
			{
				Name = "Unknown";
				break;
			}
			}

			return formatter<string_view, char>::format(
				Name,
				Context
			);
		}
	};

	template <> struct formatter<UefiHw::FirmwareError, char>
	{
		constexpr auto parse(
			format_parse_context& Context
		) -> format_parse_context::iterator
		{
			auto Iterator = Context.begin();

			if (Iterator != Context.end() && *Iterator != '}')
			{
				throw format_error(
					"Invalid FirmwareError format specifier"
				);
			}

			return Iterator;
		}

		template <typename FormatContext>
		auto format(
			const UefiHw::FirmwareError& Error,
			FormatContext& Context
		) const -> FormatContext::iterator
		{
			return format_to(
				Context.out(),
				"{}: {} failure (EFI_STATUS: {:#x})",
				Error.Operation(),
				Error.Stage(),
				static_cast<uint64_t>(Error.Status())
			);
		}
	};
}
