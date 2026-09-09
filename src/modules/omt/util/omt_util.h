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

#include <mutex>
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

    decltype(&::omt_send_create)     send_create;
    decltype(&::omt_send_destroy)    send_destroy;
    decltype(&::omt_send)            send;
    decltype(&::omt_send_getaddress) send_getaddress;
    decltype(&::omt_send_connections) send_connections;
};

// Loads (once) and returns the libomt runtime, or throws caspar::not_supported if it
// couldn't be found/loaded.
omt_lib* load_library();

// Serializes calls into omt_send_create/omt_receive_create (and their matching destroy calls)
// process-wide, and enforces a short minimum spacing between them.
//
// Some versions of libomt appear to have an internal bug where the discovery name (hostname +
// given name) of every sender created after the first one *within the same process* loses its
// case - e.g. three CasparCG channels on host "HOST1" showed "HOST1 (CHANNEL A)" for the first
// one, then "host1 (channel b)" and "host1 (channel c)" for the next two, even though CasparCG
// calls omt_send_create for each of them sequentially on a single thread at startup (so it isn't
// a CasparCG-side race). Holding this lock across each create/destroy call, with a short pause
// between them, doesn't fix the root cause - which is inside the closed-source libomt runtime -
// but gives it a moment to settle in case that's timing-sensitive; it also protects against a
// genuine CasparCG-side race for calls made after startup (e.g. one channel reinitializing its
// consumers on a video-format change while another handles an AMCP ADD/REMOVE concurrently).
std::unique_lock<std::mutex> serialize_create_call();

// Returns an ASCII-safe transliteration of `name` for use as an OMT sender name: diacritics are
// stripped (e.g. "CAMERĂ" -> "CAMERA"), and anything still non-ASCII afterwards (scripts with no
// Latin base letter to fall back to) is replaced with '_'. Returns `name` unchanged if it's already
// pure ASCII.
//
// Confirmed libomt bug: a sender name containing non-ASCII characters (e.g. accented Latin
// characters, as used by many European languages) breaks that sender's discovery entirely - not
// just its display casing - once more than one sender exists within the same process. Only
// affects sending; a producer's connection target name must be left exactly as given, since it
// has to match whatever the remote sender actually announces (which may itself be mangled by the
// same bug).
std::wstring make_ascii_safe_name(const std::wstring& name);

// Returns the list of OMT sources (Address Name) currently visible via discovery.
std::vector<std::string> get_current_sources();

// AMCP "OMT LIST" query command handler.
std::wstring list_command(protocol::amcp::command_context& ctx);

}} // namespace caspar::omt
