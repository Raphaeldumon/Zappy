#include "protocol/gui_handler.hpp"

#include <charconv>

namespace zappy::protocol
{

static bool parse_int(std::string_view s, int &out)
{
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

static std::string_view token(std::string_view &s)
{
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    auto end = s.find(' ');
    std::string_view tok = (end == std::string_view::npos) ? s : s.substr(0, end);
    s.remove_prefix(tok.size());
    return tok;
}

ParsedGuiRequest parse_gui_request(std::string_view line)
{
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.remove_suffix(1);

    ParsedGuiRequest r;

    std::string_view rest = line;
    std::string_view verb = token(rest);

    if (verb == "msz")
    {
        r.type = GuiRequest::Msz;
        return r;
    }
    if (verb == "mct")
    {
        r.type = GuiRequest::Mct;
        return r;
    }
    if (verb == "tna")
    {
        r.type = GuiRequest::Tna;
        return r;
    }
    if (verb == "sgt")
    {
        r.type = GuiRequest::Sgt;
        return r;
    }

    if (verb == "bct")
    {
        std::string_view sx = token(rest);
        std::string_view sy = token(rest);
        if (sx.empty() || sy.empty() || !parse_int(sx, r.x) || !parse_int(sy, r.y))
        {
            r.type = GuiRequest::BadParam;
            return r;
        }
        r.type = GuiRequest::Bct;
        return r;
    }

    auto parse_hash_n = [&](std::string_view tok) -> bool {
        if (tok.empty())
            return false;
        if (tok.front() == '#')
            tok.remove_prefix(1);
        return parse_int(tok, r.n);
    };

    if (verb == "ppo")
    {
        std::string_view sn = token(rest);
        if (!parse_hash_n(sn))
        {
            r.type = GuiRequest::BadParam;
            return r;
        }
        r.type = GuiRequest::Ppo;
        return r;
    }
    if (verb == "plv")
    {
        std::string_view sn = token(rest);
        if (!parse_hash_n(sn))
        {
            r.type = GuiRequest::BadParam;
            return r;
        }
        r.type = GuiRequest::Plv;
        return r;
    }
    if (verb == "pin")
    {
        std::string_view sn = token(rest);
        if (!parse_hash_n(sn))
        {
            r.type = GuiRequest::BadParam;
            return r;
        }
        r.type = GuiRequest::Pin;
        return r;
    }

    if (verb == "sst")
    {
        std::string_view st = token(rest);
        if (st.empty() || !parse_int(st, r.t) || r.t <= 0)
        {
            r.type = GuiRequest::BadParam;
            return r;
        }
        r.type = GuiRequest::Sst;
        return r;
    }

    r.type = GuiRequest::Unknown;
    return r;
}

} // namespace zappy::protocol
