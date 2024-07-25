/*
 * Copyright (c) 2024 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "subscriptions.h"

#include "spark_descriptor.h"
#include "coap_message_decoder.h"
#include "coap_util.h"
#include "logging.h"

namespace particle::protocol {

ProtocolError Subscriptions::handle_event(Message& msg, SparkDescriptor::CallEventHandlerCallback callback, MessageChannel& channel) {
    CoapMessageDecoder d;
    int r = d.decode((const char*)message.buf(), message.length());
    if (r < 0) {
        return ProtocolError::MALFORMED_MESSAGE;
    }

    if (d.type() == CoapType::CON && channel.is_unreliable()) {
        int r = sendEmptyAckOrRst(channel, resp, CoapType::ACK);
        if (r < 0) {
            LOG(ERROR, "Failed to send ACK: %d", r);
            return ProtocolError::COAP_ERROR;
        }
    }

    char name[MAX_EVENT_NAME_LENGTH + 1];
    size_t nameLen = 0;
    int contentFmt = -1;
    bool skipUriPrefix = true;

    auto it = d.options();
    while (it.next()) {
        switch (it.option()) {
        case CoapOption::URI_PATH: {
            if (skipUriPrefix) {
                skipUriPrefix = false;
                continue; // Skip the "e/" or "E/" part
            }
            nameLen += appendUriPath(name, sizeof(name), nameLen, it);
            if (nameLen >= sizeof(name)) {
                LOG(ERROR, "Event name is too long");
                return ProtocolError::MALFORMED_MESSAGE;
            }
            break;
        }
        case CoapOption::CONTENT_FORMAT: {
            contentFmt = it.toUInt();
        }
        default:
            break;
        }
    }

    for (size_t i = 0; i < MAX_SUBSCRIPTIONS; ++i) {
        if (!event_handlers[i].handler) {
            break;
        }
        size_t filterLen = strnlen(event_handlers[i].filter, sizeof(event_handlers[i].filter));
        if (nameLen < filterLen) {
            continue;
        }
        if (!std::memcmp(event_handlers[i].filter, name, filterLen)) {
            const char* data = nullptr;
            size_t dataSize = d.payloadSize();
            if (dataSize > 0) {
                data = d.payload();
                // Historically, the event handler callback expected a null-terminated string. Keeping that
                // behavior for now
                if (msg.length() >= msg.capacity()) {
                    std::memmove(data - 1, data, dataSize); // Overwrites the payload marker
                }
                data[dataSize] = '\0';
            }
            callback(sizeof(FilteringEventHandler), &event_handlers[i], name, data, dataSize, contentFmt);
        }
    }
    return ProtocolError::NO_ERROR;
}

} // namespace particle::protocol
