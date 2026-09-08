/*
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "../interop/libomt.h"

#include <protocol/amcp/amcp_command_context.h>

#include <string>
#include <vector>

namespace caspar { namespace omt {

// The subset of the libomt C API used by the producer/consumer, resolved dynamically
// (LoadLibrary+GetProcAddress on Windows, dlopen+dlsym elsewhere) so that CasparCG still
// builds and starts without the OMT runtime installed - it's only required on machines
// that actually use an OMT producer/consumer.
struct omt_lib
{
    decltype(&::omt_discovery_getaddresses) discovery_getaddresses;

    decltype(&::omt_receive_create)  receive_create;
    decltype(&::omt_receive_destroy) receive_destroy;
    decltype(&::omt_receive)         receive;

    decltype(&::omt_send_create)  send_create;
    decltype(&::omt_send_destroy) send_destroy;
    decltype(&::omt_send)         send;
};

// Loads (once) and returns the libomt runtime, or throws caspar::not_supported if it
// couldn't be found/loaded.
omt_lib* load_library();

// Returns the list of OMT sources (Address Name) currently visible via discovery.
std::vector<std::string> get_current_sources();

// AMCP "OMT LIST" query command handler.
std::wstring list_command(protocol::amcp::command_context& ctx);

}} // namespace caspar::omt
