#pragma once

#include <UefiHw/Protocol/Traits.hpp>

#include <memory>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol T>
	class View final
	{
	public:
		explicit View(
			T& Interface
		) noexcept : InterfacePointer(std::addressof(Interface))
		{
		}

		[[nodiscard]]
		auto Get() const noexcept -> T*
		{
			return InterfacePointer;
		}

	private:
		T* InterfacePointer{};
	};
}
