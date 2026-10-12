#pragma once

#include <Uefi.h>
#include <chrono>
#include <cstdint>
#include <ratio>
#include <stdexcept>
#include <type_traits>

namespace UefiHw::Boot
{
	template <typename Rep, typename Period>
	requires(std::is_integral_v<Rep> && sizeof(Rep) <= sizeof(std::uint64_t))
	[[nodiscard]]
	auto TimerTicks(
		std::chrono::duration<Rep, Period> Duration
	) -> UINT64
	{
		if constexpr (std::is_signed_v<Rep>)
		{
			if (Duration.count() < 0)
			{
				throw std::invalid_argument(
					"Negative timer duration"
				);
			}
		}

		using Ratio = std::ratio_divide<Period, std::ratio<1, 10000000>>;
		const auto Count = static_cast<UINT64>(Duration.count());
		constexpr auto Numerator = static_cast<UINT64>(Ratio::num);
		constexpr auto Denominator = static_cast<UINT64>(Ratio::den);
		const auto Whole = Count / Denominator;
		const auto Remainder = Count % Denominator;

		if (Whole > UINT64_MAX / Numerator)
		{
			throw std::length_error(
				"Timer duration overflow"
			);
		}

		UINT64 Fraction{};
		UINT64 Residue{};

		for (int Bit = 63; Bit >= 0; --Bit)
		{
			const auto Carry = Residue >= Denominator - Residue;
			Residue = Carry ? Residue - (Denominator - Residue) : Residue + Residue;
			Fraction = Fraction * 2 + (Carry ? 1 : 0);

			if ((Numerator >> Bit) & 1)
			{
				const auto Extra = Residue >= Denominator - Remainder;
				Residue = Extra ? Residue - (Denominator - Remainder) : Residue + Remainder;
				Fraction += Extra ? 1 : 0;
			}
		}

		const auto Base = Whole * Numerator;
		const auto Rounded = Fraction + (Residue != 0 ? 1 : 0);

		if (Rounded > UINT64_MAX - Base)
		{
			throw std::length_error("Timer duration overflow");
		}

		return Base + Rounded;
	}

	class TimerEvent final
	{
	public:
		TimerEvent();
		~TimerEvent() noexcept;
		TimerEvent(const TimerEvent&) = delete;
		auto operator=(const TimerEvent&) -> TimerEvent& = delete;
		TimerEvent(TimerEvent&& Other) noexcept;
		auto operator=(TimerEvent&&) -> TimerEvent& = delete;

		template <typename Rep, typename Period>
		auto ArmAfter(
			std::chrono::duration<Rep, Period> Delay
		) -> void
		{
			Arm(
				TimerRelative,
				TimerTicks(Delay)
			);
		}

		template <typename Rep, typename Period>
		auto ArmPeriodic(
			std::chrono::duration<Rep, Period> Interval
		) -> void
		{
			const auto Ticks = TimerTicks(
				Interval
			);

			if (!Ticks)
			{
				throw std::invalid_argument(
					"Periodic interval must be positive"
				);
			}

			Arm(
				TimerPeriodic,
				Ticks
			);
		}

		auto Cancel() -> void;

		[[nodiscard]]
		auto TryConsume() -> bool;

		auto Close() -> void;

	private:
		auto Arm(
			EFI_TIMER_DELAY Mode,
			UINT64 Ticks
		) -> void;

		EFI_EVENT Handle{};
	};
}
