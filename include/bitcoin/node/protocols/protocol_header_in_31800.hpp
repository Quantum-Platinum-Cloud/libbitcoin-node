/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_31800_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_31800_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_header_in_31800
  : public node::protocol,
    network::tracker<protocol_header_in_31800>
{
public:
    typedef std::shared_ptr<protocol_header_in_31800> ptr;

    template <typename Session>
    protocol_header_in_31800(Session& session,
        const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_header_in_31800>(session.log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Invoked when initial headers sync is complete.
    virtual void complete(const network::messages::headers& message,
        const network::logger::time& start) NOEXCEPT;

    /// Recieved incoming headers message.
    virtual bool handle_receive_headers(const code& ec,
        const network::messages::headers::cptr& message,
        const network::logger::time& start) NOEXCEPT;

private:
    network::messages::get_headers create_get_headers() NOEXCEPT;
    network::messages::get_headers create_get_headers(
        system::hashes&& start_hashes) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
