// Parrot Zik 2.0 control protocol: RFCOMM, service 8b6814d3-6ce7-4498-9700-9312c1711f63.
// Sources: zik2ctl (github.com/kradhub/zik2ctl), pyParrotZik (github.com/m0sia/pyParrotZik).
//
// Every frame: [len hi][len lo][id] + payload, len counts the 3 header bytes.
//   id 0x00 open session (host -> Zik, empty payload), id 0x02 ack (Zik -> host)
//   id 0x80 request "GET /api/.../get" (host -> Zik)
//           answer [0x01][0x01][len hi][len lo] + XML (Zik -> host), XML is
//           <answer path="..."><system><battery state="in_use" percent="80"/></system></answer>
//           or an unsolicited <notify path="/api/.../get"/> when a value changed on the device

#pragma once

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace MagicPodsCore::Zik
{
    inline constexpr const char *ServiceUuid = "8b6814d3-6ce7-4498-9700-9312c1711f63";
    inline constexpr unsigned char OpenSession = 0x00;
    inline constexpr unsigned char Ack = 0x02;
    inline constexpr unsigned char Request = 0x80;

    inline std::vector<unsigned char> Frame(unsigned char id, const std::string &payload = {})
    {
        size_t len = payload.size() + 3;
        std::vector<unsigned char> frame{static_cast<unsigned char>(len >> 8), static_cast<unsigned char>(len & 0xff), id};
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    // query = "/api/system/battery/get" or "/api/audio/noise_control/set?arg=anc&value=2"
    inline std::vector<unsigned char> EncodeRequest(const std::string &query)
    {
        return Frame(Request, "GET " + query);
    }

    struct Message
    {
        unsigned char id;
        std::string xml; // only for id == Request
    };

    // RFCOMM is a stream: frames arrive split or glued together, so reassemble by length
    class Framer
    {
        std::vector<unsigned char> _buffer;

    public:
        void Reset() { _buffer.clear(); }

        std::vector<Message> Feed(const std::vector<unsigned char> &chunk)
        {
            _buffer.insert(_buffer.end(), chunk.begin(), chunk.end());
            std::vector<Message> messages;
            while (_buffer.size() >= 3)
            {
                size_t len = (_buffer[0] << 8) | _buffer[1];
                if (len < 3) // out of sync, nothing sane to recover
                {
                    _buffer.clear();
                    break;
                }
                if (_buffer.size() < len)
                    break;
                Message m{_buffer[2], {}};
                if (m.id == Request && len > 7)
                    m.xml.assign(_buffer.begin() + 7, _buffer.begin() + len);
                messages.push_back(std::move(m));
                _buffer.erase(_buffer.begin(), _buffer.begin() + len);
            }
            return messages;
        }
    };

    // Value of attribute `attr` on the first <element ...> in xml. The answers are flat,
    // attribute-only XML, so a regex is enough; no XML library needed.
    inline std::optional<std::string> Attr(const std::string &xml, const std::string &element, const std::string &attr)
    {
        std::smatch m;
        if (std::regex_search(xml, m, std::regex("<" + element + "\\b[^>]*\\s" + attr + "=\"([^\"]*)\"")))
            return m[1].str();
        return std::nullopt;
    }

    // Seen on firmware 2.05: <answer path="..."><notify path="..."/></answer> is an answer
    // with a notify inside, only a bare <notify .../> leaves the request open
    inline bool IsAnswer(const std::string &xml)
    {
        return xml.find("<answer") != std::string::npos;
    }

    // "invalid_on" is what the Zik reports for a switch that is on but currently not applicable
    inline std::optional<bool> BoolAttr(const std::string &xml, const std::string &element, const std::string &attr)
    {
        auto v = Attr(xml, element, attr);
        if (!v)
            return std::nullopt;
        return *v == "true" || *v == "invalid_on";
    }
}
