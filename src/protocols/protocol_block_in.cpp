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
#include <bitcoin/node/protocols/protocol_block_in.hpp>

#include <utility>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Start.
// ----------------------------------------------------------------------------

void protocol_block_in::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (started())
        return;

    // There is one common inventory subscription.
    SUBSCRIBE_CHANNEL2(inventory, handle_receive_inventory, _1, _2);
    SEND1(create_get_inventory(), handle_send, _1);
    protocol::start();
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

// local
inline hashes to_hashes(const get_data& getter) NOEXCEPT
{
    hashes out{};
    out.resize(getter.items.size());
    std::transform(getter.items.rbegin(), getter.items.rend(), out.begin(),
        [](const auto& item) NOEXCEPT
        {
            return item.hash;
        });

    // Order reversed for individual erase performance.
    return out;
}

// Receive inventory and send get_data for all blocks that are not found.
bool protocol_block_in::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (stopped(ec))
        return false;

    const auto getter = create_get_data(*message);

    LOGP("Received (" << message->items.size() << ") block inventory from ["
        << authority() << "].");

    // If getter is empty it may be only because we have them all, so iterate.
    if (getter.items.empty())
    {
        // If the original request was maximal, we assume there are more.
        if (message->items.size() == max_get_blocks)
        {
            LOGP("Get inventory [" << authority() << "] (empty maximal).");
            SEND1(create_get_inventory({ message->items.back().hash }),
                handle_send, _1);
        }

        return true;
    }

    LOGP("Requesting (" << getter.items.size() << ") blocks from ["
        << authority() << "].");

    // Track this inventory until exhausted.
    const auto tracker = std::make_shared<track>(track
    {
        message->items.size(),
        message->items.back().hash,
        to_hashes(getter)
    });

    // TODO: these must be limited for DOS protection.
    // There is one block subscription for each received unexhausted inventory.
    SUBSCRIBE_CHANNEL3(block, handle_receive_block, _1, _2, tracker);
    SEND1(getter, handle_send, _1);
    return true;
}

bool protocol_block_in::handle_receive_block(const code& ec,
    const block::cptr& message, const track_ptr& tracker) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (stopped(ec))
        return false;

    if (tracker->hashes.empty())
    {
        LOGF("Exhausted block tracker.");
        return false;
    }

    // An uncorrelated block may have not been announced via inv (ie by miner).
    if (tracker->hashes.back() != message->block_ptr->hash())
    {
        LOGP("Uncorrelated block [" << encode_hash(message->block_ptr->hash())
            << "] from [" << authority() << "].");

        // This may be for another handler.
        return true;
    }

    // TODO: maintain context progression and store with header.
    // block.hash is computed from message buffer and cached on chain object.
    if (!archive().set(*message->block_ptr, database::context{ 1, 42, 7 }))
    {
        if (tracker->announced > maximum_advertisement)
        {
            LOGR("Orphan block inventory ["
                << encode_hash(message->block_ptr->hash()) << "] from ["
                << authority() << "].");
    
            // Treat orphan from larger-than-announce as invalid inventory.
            stop(network::error::protocol_violation);
            return false;
        }
        else
        {
            LOGP("Orphan block announcement ["
                << encode_hash(message->block_ptr->hash()) << "] from ["
                << authority() << "].");
    
            // Unlike headers, block announcements may come before caught-up.
            return false;
        }
    }

    // Order is reversed, so next is at back.
    tracker->hashes.pop_back();

    LOGP("Block [" << encode_hash(message->block_ptr->hash()) << "] from ["
        << authority() << "].");

    // Protocol presumes max_get_blocks unless complete.
    if ((tracker->announced == max_get_blocks) && tracker->hashes.empty())
    {
        LOGP("Get inventory [" << authority() << "] (exhausted maximal).");
        SEND1(create_get_inventory({ tracker->last }), handle_send, _1);
    }

    // Release subscription if exhausted.
    return !tracker->hashes.empty();
}

// private
get_blocks protocol_block_in::create_get_inventory() NOEXCEPT
{
    return create_get_inventory(archive().get_hashes(get_blocks::heights(
        archive().get_top_candidate())));
}

// private
get_blocks protocol_block_in::create_get_inventory(hashes&& hashes) NOEXCEPT
{
    if (!hashes.empty())
    {
        LOGP("Request blocks after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }

    return { std::move(hashes) };
}

// private
get_data protocol_block_in::create_get_data(
    const inventory& message) const NOEXCEPT
{
    constexpr auto block_type = inventory::type_id::block;

    get_data getter{};
    getter.items.reserve(message.count(block_type));
    for (const auto& item: message.items)
        if ((item.type == block_type) && !archive().is_block(item.hash))
            getter.items.push_back(item);

    getter.items.shrink_to_fit();
    return getter;
}


} // namespace node
} // namespace libbitcoin
